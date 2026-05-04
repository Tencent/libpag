// Copyright (C) 2026 Tencent. All rights reserved.
//
// PAG v2 resource Tags — ImageAssetTable / ImageAsset.
//
// Phase 16 (v2.20) removed the FontAssetTable / FontAsset writer path
// entirely: runtime-shape mode carries font information as fontFamily /
// fontStyle strings on ElementTextData, so there are no opaque font bytes
// to serialise. The Decoder still tolerates the tag codes (3 / 7) through
// the generic UnknownTagCode=400 path to remain forward-compatible with
// byte streams produced by pre-v2.20 branches.
//
// Authoritative byte layouts: §D.6 (with v2.19 P0-R1 sub-Tag rationale).
#include <cstring>
#include "pagx/pag/CodecTags.h"
#include "pagx/pag/TagHeader.h"
#include "pagx/pag/ValueCodec.h"
#include "pagx/pag/limits.h"

namespace pagx::pag {

namespace {

DiagnosticCollectorGuard MakeGuard(DecodeContext* ctx) {
  DiagnosticCollectorGuard g;
  g.collector = static_cast<DiagnosticCollector*>(ctx);
  return g;
}

bool SafeTagEnd(::pag::DecodeStream* stream, uint64_t bodyStart, uint32_t bodyLength,
                DecodeContext* ctx, uint64_t* outEnd) {
  uint64_t end = bodyStart + static_cast<uint64_t>(bodyLength);
  if (end > stream->length()) {
    ctx->error(ErrorCode::MalformedTag, "tag length overflows stream");
    return false;
  }
  *outEnd = end;
  return true;
}

// Writes raw owning bytes via varU32 length prefix. Mirrors
// ReadLengthPrefixedBytes in ValueCodec.h. Empty / null buffers serialize as
// length=0 — matches the "asset present but data missing" sentinel pattern
// the loader uses.
void WriteLengthPrefixedBytes(::pag::EncodeStream* stream,
                              const std::shared_ptr<const tgfx::Data>& data) {
  uint32_t n = data ? static_cast<uint32_t>(data->size()) : 0;
  stream->writeEncodedUint32(n);
  if (n > 0) {
    stream->writeBytes(reinterpret_cast<uint8_t*>(const_cast<void*>(data->data())), n);
  }
}

}  // namespace

// =============================================================================
// ImageAsset (TagCode = 6)
// =============================================================================

namespace {

void WriteImageAsset(::pag::EncodeStream* stream, const ImageAsset& asset, EncodeSession* session) {
  ::pag::EncodeStream body(session->sc);
  WriteLengthPrefixedBytes(&body, asset.data);
  body.writeInt32(asset.width);
  body.writeInt32(asset.height);
  body.writeUint8(static_cast<uint8_t>(asset.kind));
  WriteTag(stream, TagCode::ImageAssetItem, &body);
}

// Reads one ImageAsset sub-Tag body. Returns nullptr on fatal so the caller
// can drop the parent table. On warn-only paths (oversize, bad kind) returns
// a best-effort asset with sentinel fields so downstream Inflater can flag
// the missing data without crashing.
std::unique_ptr<ImageAsset> ReadImageAsset(::pag::DecodeStream* stream, DecodeContext* ctx,
                                           uint64_t tagEnd) {
  auto asset = std::make_unique<ImageAsset>();
  auto guard = MakeGuard(ctx);

  asset->data = ReadLengthPrefixedBytes(stream, &guard, limits::MAX_IMAGE_BYTES,
                                        ErrorCode::ImageResourceSizeExceeded);

  // §D.6 P1-14: validate dims even after size warn so we don't propagate
  // negative / unbounded values into Surface allocation.
  int32_t w = stream->readInt32();
  int32_t h = stream->readInt32();
  if (w <= 0 || w > static_cast<int32_t>(limits::MAX_CANVAS_DIM) || h <= 0 ||
      h > static_cast<int32_t>(limits::MAX_CANVAS_DIM)) {
    ctx->warn(ErrorCode::MalformedTag, "ImageAsset dimensions out of range; degraded to 1x1");
    asset->width = 1;
    asset->height = 1;
  } else {
    asset->width = w;
    asset->height = h;
  }

  uint8_t kindByte = stream->readUint8();
  if (kindByte == 0) {
    asset->kind = ImageAssetKind::Raster;
  } else if (kindByte <= 3) {
    // Unsupported but reserved kind — degrade to Raster + warn so the
    // Inflater treats `data` as encoded bytes regardless. This matches
    // §D.6 §6.1 ImageAsset kind handling.
    ctx->warn(ErrorCode::InvalidEnumValue,
              "ImageAsset kind reserved for future use; downgraded to Raster");
    asset->kind = ImageAssetKind::Raster;
  } else {
    ctx->warn(ErrorCode::MalformedTag, "ImageAsset kind out of range; entry skipped");
    asset.reset();
  }

  (void)tagEnd;
  return asset;
}

}  // namespace

void WriteImageAssetTable(::pag::EncodeStream* stream,
                          const std::vector<std::unique_ptr<ImageAsset>>& images,
                          EncodeSession* session) {
  ::pag::EncodeStream body(session->sc);
  body.writeEncodedUint32(static_cast<uint32_t>(images.size()));
  for (const auto& img : images) {
    WriteImageAsset(&body, *img, session);
  }
  WriteTag(stream, TagCode::ImageAssetTable, &body);
}

void ReadImageAssetTable(::pag::DecodeStream* stream, DecodeContext* ctx, uint64_t tagEnd,
                         std::vector<std::unique_ptr<ImageAsset>>* out) {
  auto guard = MakeGuard(ctx);
  uint32_t count = ReadEncodedUint32Safe(stream, &guard);
  if (count > limits::MAX_IMAGES) {
    ctx->error(ErrorCode::StructureLimitExceeded, "ImageAssetTable count exceeds MAX_IMAGES");
    return;
  }

  for (uint32_t i = 0; i < count; ++i) {
    if (stream->position() >= tagEnd) {
      ctx->error(ErrorCode::TruncatedData,
                 "ImageAssetTable truncated before all ImageAsset tags read");
      return;
    }
    TagHeader header = ReadTagHeader(stream);
    if (header.code != TagCode::ImageAssetItem) {
      ctx->error(ErrorCode::MalformedTag, "ImageAssetTable expected nested ImageAsset tag");
      return;
    }
    uint64_t bodyStart = stream->position();
    uint64_t childEnd = 0;
    if (!SafeTagEnd(stream, bodyStart, header.length, ctx, &childEnd)) {
      return;
    }
    if (childEnd > tagEnd) {
      ctx->error(ErrorCode::MalformedTag, "ImageAsset extends past ImageAssetTable");
      return;
    }
    auto img = ReadImageAsset(stream, ctx, childEnd);
    if (ctx->hasError()) {
      return;
    }
    if (img != nullptr) {
      out->push_back(std::move(img));
    }
    // Each sub-Tag is forward-compat (length-bounded), so always align.
    SeekTo(stream, static_cast<uint32_t>(childEnd));
  }
}

}  // namespace pagx::pag
