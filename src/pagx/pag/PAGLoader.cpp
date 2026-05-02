// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 10.5 — Public load API. Wires `Codec::Decode → LayerInflater →
// Result` with strict-mode promotion scoped to the Codec stage (Inflater
// warnings are never promoted per §15.3). Also exposes the shallow Peek
// path that parses only the FileHeader Tag for O(1) metadata probing.

#include "pagx/PAGLoader.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <utility>
#include <vector>
#include "codec/utils/DecodeStream.h"
#include "codec/utils/StreamContext.h"
#include "pagx/pag/Codec.h"
#include "pagx/pag/CodecTags.h"
#include "pagx/pag/DecodeContext.h"
#include "pagx/pag/LayerInflater.h"
#include "pagx/pag/PAGDocument.h"
#include "pagx/pag/TagHeader.h"
#include "pagx/pag/limits.h"

namespace pagx {

namespace {

// File container framing constants (mirrors Codec.cpp).
constexpr uint8_t kMagicP = 'P';
constexpr uint8_t kMagicA = 'A';
constexpr uint8_t kMagicG = 'G';
constexpr uint8_t kFormatVersion = 0x02;
constexpr uint8_t kCompressionUncompressed = 0x00;
constexpr size_t kFramingBytes = 9;

// Promote Codec-only Warning-severity diagnostics to errors. Inflater
// warnings stay in `warnings`. Baker warnings can't appear on this path
// (there's no Baker in the load pipeline).
void PromoteCodecWarningsToErrors(std::vector<Diagnostic>* errors,
                                  std::vector<Diagnostic>* warnings) {
  std::vector<Diagnostic> keep;
  keep.reserve(warnings->size());
  for (auto& w : *warnings) {
    if (SeverityOf(w.code) == DiagnosticSeverity::Warning &&
        StageOf(w.code) == DiagnosticStage::Codec) {
      errors->push_back(std::move(w));
    } else {
      keep.push_back(std::move(w));
    }
  }
  *warnings = std::move(keep);
}

// Reads an entire binary file into memory. Returns false with `errOut`
// populated on any IO failure; callers wrap into FileReadFailed=307.
bool ReadFileToBytes(const std::string& path, std::vector<uint8_t>* out, std::string* errOut) {
  std::error_code ec;
  if (!std::filesystem::exists(path, ec) || ec) {
    *errOut = "file does not exist: " + path;
    return false;
  }
  auto size = std::filesystem::file_size(path, ec);
  if (ec) {
    *errOut = "failed to stat file: " + path + " (" + ec.message() + ")";
    return false;
  }
  // Defence-in-depth: file_size is uintmax_t, clamp to our absolute cap so
  // a truncated size_t below can't alias a massive file. The Codec layer
  // also guards on bodyLength > MAX_TOTAL_BODY_BYTES; this check bails
  // before even allocating the buffer.
  if (size > static_cast<std::uintmax_t>(pag::limits::MAX_TOTAL_BODY_BYTES + kFramingBytes)) {
    *errOut = "file too large: " + path;
    return false;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    *errOut = "failed to open file: " + path;
    return false;
  }
  out->resize(static_cast<size_t>(size));
  if (size > 0) {
    in.read(reinterpret_cast<char*>(out->data()), static_cast<std::streamsize>(size));
    if (!in.good()) {
      *errOut = "read error on file: " + path;
      return false;
    }
  }
  return true;
}

}  // namespace

// =============================================================================
// LoadFromBytes
// =============================================================================

PAGLoader::Result PAGLoader::LoadFromBytes(const uint8_t* data, size_t length,
                                           const Options& options) {
  Result result;

  auto decode = pag::Codec::Decode(data, length);
  for (auto& e : decode.errors) {
    result.errors.push_back(std::move(e));
  }
  for (auto& w : decode.warnings) {
    result.warnings.push_back(std::move(w));
  }

  // If Decode failed hard, skip Inflate — we have no document to walk.
  if (decode.doc == nullptr) {
    if (options.strict) {
      PromoteCodecWarningsToErrors(&result.errors, &result.warnings);
    }
    result.ok = result.errors.empty();
    return result;
  }

  auto inflate = pag::LayerInflater::Inflate(std::move(decode.doc));
  result.layer = std::move(inflate.layer);
  for (auto& w : inflate.warnings) {
    result.warnings.push_back(std::move(w));
  }

  if (options.strict) {
    PromoteCodecWarningsToErrors(&result.errors, &result.warnings);
  }
  result.ok = result.errors.empty();
  return result;
}

// =============================================================================
// LoadFromFile
// =============================================================================

PAGLoader::Result PAGLoader::LoadFromFile(const std::string& filePath, const Options& options) {
  Result result;

  std::vector<uint8_t> bytes;
  std::string readErr;
  if (!ReadFileToBytes(filePath, &bytes, &readErr)) {
    result.errors.push_back({DiagnosticCode::FileReadFailed, std::move(readErr), /*byteOffset=*/0,
                             /*contextIndex=*/UINT32_MAX});
    result.ok = false;
    return result;
  }
  return LoadFromBytes(bytes.data(), bytes.size(), options);
}

// =============================================================================
// Peek — shallow FileHeader decode
// =============================================================================
//
// We do only enough work to reach the first FileHeader Tag: read the 9-byte
// file framing, read one TagHeader, verify it's FileHeader, call
// ReadFileHeader. We intentionally skip the full Decode loop to keep this
// O(header) regardless of document size.

namespace {

PAGLoader::PeekResult PeekImpl(const uint8_t* data, size_t length) {
  PAGLoader::PeekResult result;
  pag::DecodeContext ctx;

  if (data == nullptr || length < kFramingBytes) {
    result.errors.push_back({DiagnosticCode::TruncatedData,
                             "input shorter than file container framing (9 bytes)",
                             /*byteOffset=*/0, /*contextIndex=*/UINT32_MAX});
    result.ok = false;
    return result;
  }
  if (length > UINT32_MAX) {
    result.errors.push_back({DiagnosticCode::BodyLengthOutOfRange,
                             "input exceeds 4 GB DecodeStream limit",
                             /*byteOffset=*/0, /*contextIndex=*/UINT32_MAX});
    result.ok = false;
    return result;
  }

  ::pag::DecodeStream stream(&ctx.streamContext, data, static_cast<uint32_t>(length));
  pag::CurrentStreamScope rootScope(&ctx, &stream);

  const uint8_t magic[3] = {stream.readUint8(), stream.readUint8(), stream.readUint8()};
  if (magic[0] != kMagicP || magic[1] != kMagicA || magic[2] != kMagicG) {
    ctx.error(DiagnosticCode::InvalidMagic, "magic bytes are not 'PAG'");
    result.errors = std::move(ctx.errors);
    result.warnings = std::move(ctx.warnings);
    result.ok = false;
    return result;
  }
  const uint8_t version = stream.readUint8();
  if (version != kFormatVersion) {
    ctx.error(DiagnosticCode::UnsupportedVersion, "unsupported FORMAT_VERSION (expected 0x02)");
    result.errors = std::move(ctx.errors);
    result.warnings = std::move(ctx.warnings);
    result.ok = false;
    return result;
  }
  const uint32_t bodyLength = stream.readUint32();
  const uint32_t bytesAfterCompression =
      stream.bytesAvailable() > 0 ? stream.bytesAvailable() - 1 : 0;
  if (bodyLength > pag::limits::MAX_TOTAL_BODY_BYTES || bodyLength > bytesAfterCompression) {
    ctx.error(DiagnosticCode::BodyLengthOutOfRange,
              "bodyLength exceeds MAX_TOTAL_BODY_BYTES or remaining stream");
    result.errors = std::move(ctx.errors);
    result.warnings = std::move(ctx.warnings);
    result.ok = false;
    return result;
  }
  const uint8_t compression = stream.readUint8();
  if (compression != kCompressionUncompressed) {
    ctx.error(DiagnosticCode::UnsupportedCompression,
              "compression byte != 0x00 (only UNCOMPRESSED supported)");
    result.errors = std::move(ctx.errors);
    result.warnings = std::move(ctx.warnings);
    result.ok = false;
    return result;
  }

  if (stream.bytesAvailable() < 2) {
    ctx.error(DiagnosticCode::TruncatedData, "truncated before first TagHeader");
    result.errors = std::move(ctx.errors);
    result.warnings = std::move(ctx.warnings);
    result.ok = false;
    return result;
  }

  const uint64_t bodyStart = stream.position();
  const uint64_t bodyEnd = bodyStart + static_cast<uint64_t>(bodyLength);
  pag::TagHeader header = pag::ReadTagHeader(&stream);
  ctx.syncFromStreamContext();
  if (ctx.hasError()) {
    result.errors = std::move(ctx.errors);
    result.warnings = std::move(ctx.warnings);
    result.ok = false;
    return result;
  }
  if (header.code != pag::TagCode::FileHeader) {
    ctx.error(DiagnosticCode::MalformedTag, "first Tag is not FileHeader — Peek cannot continue");
    result.errors = std::move(ctx.errors);
    result.warnings = std::move(ctx.warnings);
    result.ok = false;
    return result;
  }
  const uint64_t tagBodyStart = stream.position();
  const uint64_t tagEnd = tagBodyStart + static_cast<uint64_t>(header.length);
  if (tagEnd > bodyEnd) {
    ctx.error(DiagnosticCode::MalformedTag, "FileHeader Tag extends past declared body end");
    result.errors = std::move(ctx.errors);
    result.warnings = std::move(ctx.warnings);
    result.ok = false;
    return result;
  }

  pag::FileHeader fh = pag::ReadFileHeader(&stream, &ctx, tagEnd);
  if (ctx.hasError()) {
    result.errors = std::move(ctx.errors);
    result.warnings = std::move(ctx.warnings);
    result.ok = false;
    return result;
  }

  result.width = fh.width;
  result.height = fh.height;
  result.backgroundColor = fh.backgroundColor;
  result.frameRateNumerator = static_cast<uint32_t>(fh.frameRate.numerator);
  result.frameRateDenominator = static_cast<uint32_t>(fh.frameRate.denominator);
  result.duration = fh.duration;
  result.errors = std::move(ctx.errors);
  result.warnings = std::move(ctx.warnings);
  result.ok = result.errors.empty();
  return result;
}

}  // namespace

PAGLoader::PeekResult PAGLoader::Peek(const std::string& filePath) {
  std::vector<uint8_t> bytes;
  std::string readErr;
  if (!ReadFileToBytes(filePath, &bytes, &readErr)) {
    PeekResult result;
    result.errors.push_back({DiagnosticCode::FileReadFailed, std::move(readErr), /*byteOffset=*/0,
                             /*contextIndex=*/UINT32_MAX});
    result.ok = false;
    return result;
  }
  return PeekImpl(bytes.data(), bytes.size());
}

PAGLoader::PeekResult PAGLoader::Peek(const uint8_t* data, size_t length) {
  return PeekImpl(data, length);
}

}  // namespace pagx
