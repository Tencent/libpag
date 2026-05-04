// Copyright (C) 2026 Tencent. All rights reserved.
#include "pagx/pag/Codec.h"
#include <cstring>
#include "codec/utils/DecodeStream.h"
#include "codec/utils/EncodeStream.h"
#include "codec/utils/StreamContext.h"
#include "pag/types.h"
#include "pagx/pag/CodecTags.h"
#include "pagx/pag/DecodeContext.h"
#include "pagx/pag/EncodeSession.h"
#include "pagx/pag/TagHeader.h"
#include "pagx/pag/limits.h"

namespace pagx::pag {

namespace {

// File container layout — see §D.4. Magic is 3 ASCII bytes; version 1 byte;
// bodyLength 4 bytes (uint32); compression 1 byte. Hard-coded constants
// instead of std::string_view because they're checked byte-by-byte.
constexpr uint8_t kMagicP = 'P';
constexpr uint8_t kMagicA = 'A';
constexpr uint8_t kMagicG = 'G';
constexpr uint8_t kFormatVersion = 0x02;
constexpr uint8_t kCompressionUncompressed = 0x00;

// Pre-body framing: 3-byte magic + 1-byte version + 4-byte bodyLength + 1-byte
// compression = 9 bytes. Decoder rejects any input shorter than this with
// TruncatedData=303 before touching the bytes.
constexpr size_t kFramingBytes = 9;

}  // namespace

// =============================================================================
// Encode
// =============================================================================

EncodeResult Codec::Encode(const PAGDocument& doc) {
  EncodeResult result;
  DiagnosticCollector diag;
  ::pag::StreamContext sc;
  EncodeSession session{&diag, &sc};

  ::pag::EncodeStream body(&sc);
  WriteFileHeader(&body, doc.header, &session);
  WriteImageAssetTable(&body, doc.images, &session);
  // Phase 16 (v2.20): FontAssetTable is no longer produced. Font information
  // is carried verbatim on ElementTextData (fontFamily / fontStyle) and
  // resolved at load time through the Inflater's FontProvider.
  WriteCompositionList(&body, doc.compositions, &session);
  WriteEndTag(&body);

  ::pag::EncodeStream file(&sc);
  file.writeUint8(kMagicP);
  file.writeUint8(kMagicA);
  file.writeUint8(kMagicG);
  file.writeUint8(kFormatVersion);
  file.writeUint32(body.length());
  file.writeUint8(kCompressionUncompressed);
  file.writeBytes(&body);

  result.bytes = file.release();
  result.warnings = std::move(diag.warnings);
  return result;
}

// =============================================================================
// Decode
// =============================================================================

DecodeResult Codec::Decode(const uint8_t* data, size_t length) {
  DecodeResult result;
  DecodeContext ctx;

  // Layer 1: minimum framing length. Anything below 9 bytes can't even hold
  // the file container — short-circuit before constructing any DecodeStream.
  if (data == nullptr || length < kFramingBytes) {
    ctx.error(ErrorCode::TruncatedData, "input shorter than file container framing (9 bytes)");
    result.errors = std::move(ctx.errors);
    result.warnings = std::move(ctx.warnings);
    return result;
  }

  // Layer 2: bound `length` to uint32_t (DecodeStream uses uint32 sizes).
  // The MAX_TOTAL_BODY_BYTES upper bound keeps allocations bounded later.
  if (length > UINT32_MAX) {
    ctx.error(ErrorCode::BodyLengthOutOfRange, "input exceeds 4 GB DecodeStream limit");
    result.errors = std::move(ctx.errors);
    result.warnings = std::move(ctx.warnings);
    return result;
  }

  ::pag::DecodeStream stream(&ctx.streamContext, data, static_cast<uint32_t>(length));
  CurrentStreamScope rootScope(&ctx, &stream);

  // ---- Layer 3: magic check ----
  uint8_t magic[3] = {stream.readUint8(), stream.readUint8(), stream.readUint8()};
  if (magic[0] != kMagicP || magic[1] != kMagicA || magic[2] != kMagicG) {
    ctx.error(ErrorCode::InvalidMagic, "magic bytes are not 'PAG'");
    result.errors = std::move(ctx.errors);
    result.warnings = std::move(ctx.warnings);
    return result;
  }

  // ---- Layer 4: version check ----
  uint8_t version = stream.readUint8();
  if (version != kFormatVersion) {
    ctx.error(ErrorCode::UnsupportedVersion, "unsupported FORMAT_VERSION (expected 0x02)");
    result.errors = std::move(ctx.errors);
    result.warnings = std::move(ctx.warnings);
    return result;
  }

  // ---- Layer 5: bodyLength ----
  uint32_t bodyLength = stream.readUint32();
  // Use safe-subtract pattern from §H.1: never write `pos + bodyLength` into
  // a 32-bit type before the comparison.
  uint32_t bytesAfterCompressionByte =
      stream.bytesAvailable() > 0 ? stream.bytesAvailable() - 1 : 0;
  if (bodyLength > limits::MAX_TOTAL_BODY_BYTES || bodyLength > bytesAfterCompressionByte) {
    ctx.error(ErrorCode::BodyLengthOutOfRange,
              "bodyLength exceeds MAX_TOTAL_BODY_BYTES or remaining stream");
    result.errors = std::move(ctx.errors);
    result.warnings = std::move(ctx.warnings);
    return result;
  }

  // ---- Layer 6: compression ----
  uint8_t compression = stream.readUint8();
  if (compression != kCompressionUncompressed) {
    ctx.error(ErrorCode::UnsupportedCompression,
              "compression byte != 0x00 (only UNCOMPRESSED supported)");
    result.errors = std::move(ctx.errors);
    result.warnings = std::move(ctx.warnings);
    return result;
  }

  // ---- Body loop ----
  // bodyStart is the position where the first Tag begins; bodyEnd is one
  // byte past the last byte the framing claims belongs to the body.
  uint64_t bodyStart = stream.position();
  uint64_t bodyEnd = bodyStart + static_cast<uint64_t>(bodyLength);
  if (bodyEnd > stream.length()) {
    // Same condition as Layer 5 but defends against the (theoretical)
    // race where bytesAvailable changed between checks. Belt + suspenders.
    ctx.error(ErrorCode::BodyLengthOutOfRange, "computed body end exceeds stream size");
    result.errors = std::move(ctx.errors);
    result.warnings = std::move(ctx.warnings);
    return result;
  }

  auto doc = std::make_unique<PAGDocument>();
  bool sawFileHeader = false;
  bool sawEnd = false;

  while (stream.position() < bodyEnd) {
    // ---- Read the next TagHeader ----
    if (stream.bytesAvailable() < 2) {
      ctx.error(ErrorCode::TruncatedData, "truncated before TagHeader");
      doc.reset();
      break;
    }
    TagHeader header = ReadTagHeader(&stream);
    ctx.syncFromStreamContext();
    if (ctx.hasError()) {
      doc.reset();
      break;
    }

    // ---- End tag terminates the body ----
    if (header.code == TagCode::End) {
      // §8.3 P0-15: warn (not error) if there are still unread bytes — some
      // pipelines append metadata after End. strict mode at PAGExporter
      // promotes this to a fatal.
      if (stream.position() != bodyEnd) {
        ctx.warn(ErrorCode::PrematureEndTag, "End tag reached before bodyLength fully consumed");
      }
      sawEnd = true;
      break;
    }

    // ---- Compute child tagEnd with overflow guard (§D.3) ----
    uint64_t tagBodyStart = stream.position();
    uint64_t tagEnd = tagBodyStart + static_cast<uint64_t>(header.length);
    if (tagEnd > bodyEnd) {
      ctx.error(ErrorCode::MalformedTag, "tag length extends past declared body end");
      doc.reset();
      break;
    }

    // ---- Dispatch ----
    switch (header.code) {
      case TagCode::FileHeader: {
        if (sawFileHeader) {
          ctx.warn(ErrorCode::UnknownTagCode, "duplicate FileHeader tag — ignored");
          break;
        }
        doc->header = ReadFileHeader(&stream, &ctx, tagEnd);
        sawFileHeader = true;
        break;
      }
      case TagCode::ImageAssetTable: {
        ReadImageAssetTable(&stream, &ctx, tagEnd, &doc->images);
        break;
      }
      case TagCode::FontAssetTable: {
        // Phase 16 (v2.20): forward-compat no-op. Byte streams produced by
        // pre-v2.20 branches may still carry a FontAssetTable; we ignore
        // the payload and let the generic length skip below align the
        // cursor. The warn surfaces the downgrade so CI can flag stale
        // Writers still emitting this tag.
        ctx.warn(ErrorCode::UnknownTagCode, "FontAssetTable dropped (Phase 16 runtime-shape mode)");
        break;
      }
      case TagCode::CompositionList: {
        ReadCompositionList(&stream, &ctx, tagEnd, &doc->compositions);
        break;
      }
      default: {
        // Unknown / forward-compat Tags — warn + length-skip per §6.4.
        ctx.warn(ErrorCode::UnknownTagCode, "unknown TagCode at top level; skipped");
        break;
      }
    }

    ctx.syncFromStreamContext();
    if (ctx.hasError()) {
      doc.reset();
      break;
    }

    // Force-align to the declared tag end. This handles two cases:
    //   1) the reader stopped early (Phase 4a Composition skips layer bytes);
    //   2) the writer appended forward-compat fields beyond what we know.
    SeekTo(&stream, static_cast<uint32_t>(tagEnd));
  }

  if (doc != nullptr && !sawEnd && !ctx.hasError()) {
    ctx.error(ErrorCode::TruncatedData, "body ended without End tag");
    doc.reset();
  }

  if (!ctx.hasError()) {
    result.doc = std::move(doc);
  }
  result.errors = std::move(ctx.errors);
  result.warnings = std::move(ctx.warnings);
  return result;
}

}  // namespace pagx::pag
