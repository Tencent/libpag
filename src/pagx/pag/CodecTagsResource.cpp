// Copyright (C) 2026 Tencent. All rights reserved.
//
// PAG v2 resource Tags — ImageAssetTable / ImageAsset / EmbeddedFontTable /
// EmbeddedFont.
//
// Phase 16 (v2.20) removed the FontAssetTable / FontAsset writer path
// entirely: runtime-shape mode carries font information as fontFamily /
// fontStyle strings on ElementTextData, so there are no opaque font bytes
// to serialise. The Decoder still tolerates the tag codes (3 / 7) through
// the generic UnknownTagCode=400 path to remain forward-compatible with
// byte streams produced by pre-v2.20 branches.
//
// Phase 17 (v2.23) introduces EmbeddedFontTable / EmbeddedFont — path-based
// glyph resources owned by the PAG document itself (mirrors PAGX <Font>).
// Not a ttf/otf subset; ttf font files are still never embedded in PAG.
//
// Authoritative byte layouts: §D.6 (with v2.19 P0-R1 sub-Tag rationale).
#include <cstring>
#include "pagx/pag/CodecTags.h"
#include "pagx/pag/PathCodec.h"
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

// =============================================================================
// EmbeddedFont (TagCode = 9) — Phase 17 v2.23
// =============================================================================

namespace {

// EmbeddedFont sub-Tag body: varU32 unitsPerEm, varU32 glyphCount,
// repeat[ float advance, Path path ].
//
// Path encoding reuses the §D.2 quantised path codec (PathCodec.h::WritePath
// / ReadPath) — same encoding ElementShapePath uses, so glyph outlines pay
// the same per-verb cost (verbCount + xs[] + ys[]). The PAGX y-up source
// orientation is preserved in the bytes; the Inflater applies the y-flip
// matrix at render time (case A inflateTextAsPath).
void WriteEmbeddedFont(::pag::EncodeStream* stream, const EmbeddedFont& font,
                       EncodeSession* session) {
  ::pag::EncodeStream body(session->sc);
  body.writeEncodedUint32(font.unitsPerEm);
  body.writeEncodedUint32(static_cast<uint32_t>(font.glyphs.size()));
  for (const auto& glyph : font.glyphs) {
    body.writeFloat(glyph.advance);
    WritePath(&body, glyph.path);
  }
  WriteTag(stream, TagCode::EmbeddedFontItem, &body);
}

std::unique_ptr<EmbeddedFont> ReadEmbeddedFont(::pag::DecodeStream* stream, DecodeContext* ctx,
                                               uint64_t tagEnd) {
  auto font = std::make_unique<EmbeddedFont>();
  auto guard = MakeGuard(ctx);

  font->unitsPerEm = ReadEncodedUint32Safe(stream, &guard);
  uint32_t glyphCount = ReadEncodedUint32Safe(stream, &guard);
  // Each EmbeddedFont can hold the glyph table for an entire script (e.g.
  // a Chinese subset of several thousand glyphs). MAX_GLYPHS_PER_RUN is the
  // closest hard limit and is generous enough; reuse it rather than minting
  // a new constant for the same magnitude.
  if (glyphCount > limits::MAX_GLYPHS_PER_RUN) {
    ctx->error(ErrorCode::GlyphCountLimitExceeded,
               "EmbeddedFont glyphCount exceeds MAX_GLYPHS_PER_RUN");
    return nullptr;
  }

  font->glyphs.resize(glyphCount);
  for (uint32_t i = 0; i < glyphCount; ++i) {
    font->glyphs[i].advance = stream->readFloat();
    font->glyphs[i].path = ReadPath(stream, ctx);
    if (ctx->hasError()) {
      return nullptr;
    }
  }

  (void)tagEnd;
  return font;
}

}  // namespace

void WriteEmbeddedFontTable(::pag::EncodeStream* stream,
                            const std::vector<std::unique_ptr<EmbeddedFont>>& fonts,
                            EncodeSession* session) {
  ::pag::EncodeStream body(session->sc);
  body.writeEncodedUint32(static_cast<uint32_t>(fonts.size()));
  for (const auto& font : fonts) {
    WriteEmbeddedFont(&body, *font, session);
  }
  WriteTag(stream, TagCode::EmbeddedFontTable, &body);
}

void ReadEmbeddedFontTable(::pag::DecodeStream* stream, DecodeContext* ctx, uint64_t tagEnd,
                           std::vector<std::unique_ptr<EmbeddedFont>>* out) {
  auto guard = MakeGuard(ctx);
  uint32_t count = ReadEncodedUint32Safe(stream, &guard);
  // Reuse MAX_FONTS — Phase 16 left this constant in place even after
  // FontAssetTable was removed; semantically still represents "max font
  // resources per document".
  if (count > limits::MAX_FONTS) {
    ctx->error(ErrorCode::StructureLimitExceeded, "EmbeddedFontTable count exceeds MAX_FONTS");
    return;
  }

  for (uint32_t i = 0; i < count; ++i) {
    if (stream->position() >= tagEnd) {
      ctx->error(ErrorCode::TruncatedData,
                 "EmbeddedFontTable truncated before all EmbeddedFont tags read");
      return;
    }
    TagHeader header = ReadTagHeader(stream);
    if (header.code != TagCode::EmbeddedFontItem) {
      ctx->error(ErrorCode::MalformedTag, "EmbeddedFontTable expected nested EmbeddedFont tag");
      return;
    }
    uint64_t bodyStart = stream->position();
    uint64_t childEnd = 0;
    if (!SafeTagEnd(stream, bodyStart, header.length, ctx, &childEnd)) {
      return;
    }
    if (childEnd > tagEnd) {
      ctx->error(ErrorCode::MalformedTag, "EmbeddedFont extends past EmbeddedFontTable");
      return;
    }
    auto font = ReadEmbeddedFont(stream, ctx, childEnd);
    if (ctx->hasError()) {
      return;
    }
    if (font != nullptr) {
      out->push_back(std::move(font));
    }
    SeekTo(stream, static_cast<uint32_t>(childEnd));
  }
}

}  // namespace pagx::pag
