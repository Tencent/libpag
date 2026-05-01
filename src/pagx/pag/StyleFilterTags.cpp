// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 7 — Filter/Style byte codec + LayerFilters/LayerStyles containers.
//
// Write side: each Filter/Style instance is emitted as its own sub-Tag whose
// TagCode encodes the variant (Blur/DropShadow/... for filters;
// DropShadow/InnerShadow/BackgroundBlur for styles). The outer LayerFilters /
// LayerStyles container holds only a varU32 count and then the sub-Tag
// sequence (§D.9). A non-empty container is required before the container
// itself is emitted — the LayerBlock writer guards that.
//
// Read side: the dispatch on the inner TagCode does double duty as the
// enum-value check — an unrecognized Filter/Style TagCode is length-skipped
// with UnknownTagCode=400 (matches §6.4), keeping forward-compat doors open
// without losing stream alignment.
#include "pagx/pag/StyleFilterTags.h"
#include <cstdint>
#include <memory>
#include <utility>
#include "pagx/pag/DiagBuild.h"
#include "pagx/pag/PropertyEncoding.h"
#include "pagx/pag/TagHeader.h"
#include "pagx/pag/ValueCodec.h"
#include "pagx/pag/limits.h"

namespace pagx::pag {

namespace {

// Thin adapter so ReadProperty's "push warning when propHeader reserved bits
// are set" path has a collector without each call site building one.
DiagnosticCollectorGuard MakeGuard(DecodeContext* ctx) {
  DiagnosticCollectorGuard g;
  g.collector = static_cast<DiagnosticCollector*>(ctx);
  return g;
}

// Same 64-bit overflow guard used by CodecTags / CodecTagsLayer. Keeps Tag
// length additions out of the 32-bit wraparound zone that would otherwise
// let `bodyStart + length > stream->length()` overflow silently.
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

// ---- Per-filter body writers (inline; wrapped by WriteFilterTag below) ----

void WriteFilterBlurBody(::pag::EncodeStream* body, const LayerFilter& f) {
  WriteProperty<float>(body, f.blurX, /*default=*/0.0f);
  WriteProperty<float>(body, f.blurY, /*default=*/0.0f);
  body->writeUint8(static_cast<uint8_t>(f.tileMode));
}

void WriteFilterDropShadowBody(::pag::EncodeStream* body, const LayerFilter& f) {
  WriteProperty<float>(body, f.offsetX, /*default=*/0.0f);
  WriteProperty<float>(body, f.offsetY, /*default=*/0.0f);
  WriteProperty<float>(body, f.blurX, /*default=*/0.0f);
  WriteProperty<float>(body, f.blurY, /*default=*/0.0f);
  WriteProperty<tgfx::Color>(body, f.color, /*default=*/tgfx::Color{});
  body->writeBoolean(f.shadowOnly);
}

// InnerShadow shares DropShadow's wire layout (§D.12 explicit "同上").
void WriteFilterInnerShadowBody(::pag::EncodeStream* body, const LayerFilter& f) {
  WriteFilterDropShadowBody(body, f);
}

void WriteFilterColorMatrixBody(::pag::EncodeStream* body, const LayerFilter& f) {
  WriteProperty<std::array<float, 20>>(body, f.colorMatrix,
                                       /*default=*/std::array<float, 20>{});
}

void WriteFilterBlendBody(::pag::EncodeStream* body, const LayerFilter& f) {
  WriteProperty<tgfx::Color>(body, f.blendColor, /*default=*/tgfx::Color{});
  body->writeUint8(static_cast<uint8_t>(f.blendFilterMode));
}

// ---- Per-style body writers ----

void WriteCommonStyleHead(::pag::EncodeStream* body, const LayerStyle& s) {
  body->writeBoolean(s.excludeChildEffects);
  body->writeUint8(static_cast<uint8_t>(s.blendMode));
}

void WriteStyleDropShadowBody(::pag::EncodeStream* body, const LayerStyle& s) {
  WriteCommonStyleHead(body, s);
  WriteProperty<float>(body, s.offsetX, /*default=*/0.0f);
  WriteProperty<float>(body, s.offsetY, /*default=*/0.0f);
  WriteProperty<float>(body, s.blurX, /*default=*/0.0f);
  WriteProperty<float>(body, s.blurY, /*default=*/0.0f);
  WriteProperty<tgfx::Color>(body, s.color, /*default=*/tgfx::Color{});
  body->writeBoolean(s.showBehindLayer);
}

// InnerShadow = DropShadow without `showBehindLayer`.
void WriteStyleInnerShadowBody(::pag::EncodeStream* body, const LayerStyle& s) {
  WriteCommonStyleHead(body, s);
  WriteProperty<float>(body, s.offsetX, /*default=*/0.0f);
  WriteProperty<float>(body, s.offsetY, /*default=*/0.0f);
  WriteProperty<float>(body, s.blurX, /*default=*/0.0f);
  WriteProperty<float>(body, s.blurY, /*default=*/0.0f);
  WriteProperty<tgfx::Color>(body, s.color, /*default=*/tgfx::Color{});
}

void WriteStyleBackgroundBlurBody(::pag::EncodeStream* body, const LayerStyle& s) {
  WriteCommonStyleHead(body, s);
  WriteProperty<float>(body, s.blurX, /*default=*/0.0f);
  WriteProperty<float>(body, s.blurY, /*default=*/0.0f);
  body->writeUint8(static_cast<uint8_t>(s.tileMode));
}

// Map the in-memory type enum to its wire TagCode. Encoding side only —
// Reader goes the other way via the switch in Read<LayerFilters/Styles>.
TagCode TagCodeForFilterType(LayerFilterType t) {
  switch (t) {
    case LayerFilterType::Blur:
      return TagCode::FilterBlur;
    case LayerFilterType::DropShadow:
      return TagCode::FilterDropShadow;
    case LayerFilterType::InnerShadow:
      return TagCode::FilterInnerShadow;
    case LayerFilterType::ColorMatrix:
      return TagCode::FilterColorMatrix;
    case LayerFilterType::Blend:
      return TagCode::FilterBlend;
  }
  return TagCode::FilterBlur;
}

TagCode TagCodeForStyleType(LayerStyleType t) {
  switch (t) {
    case LayerStyleType::DropShadow:
      return TagCode::StyleDropShadow;
    case LayerStyleType::InnerShadow:
      return TagCode::StyleInnerShadow;
    case LayerStyleType::BackgroundBlur:
      return TagCode::StyleBackgroundBlur;
  }
  return TagCode::StyleDropShadow;
}

void WriteFilterTag(::pag::EncodeStream* stream, const LayerFilter& f, EncodeSession* session) {
  ::pag::EncodeStream body(session->sc);
  switch (f.type) {
    case LayerFilterType::Blur:
      WriteFilterBlurBody(&body, f);
      break;
    case LayerFilterType::DropShadow:
      WriteFilterDropShadowBody(&body, f);
      break;
    case LayerFilterType::InnerShadow:
      WriteFilterInnerShadowBody(&body, f);
      break;
    case LayerFilterType::ColorMatrix:
      WriteFilterColorMatrixBody(&body, f);
      break;
    case LayerFilterType::Blend:
      WriteFilterBlendBody(&body, f);
      break;
  }
  WriteTag(stream, TagCodeForFilterType(f.type), &body);
}

void WriteStyleTag(::pag::EncodeStream* stream, const LayerStyle& s, EncodeSession* session) {
  ::pag::EncodeStream body(session->sc);
  switch (s.type) {
    case LayerStyleType::DropShadow:
      WriteStyleDropShadowBody(&body, s);
      break;
    case LayerStyleType::InnerShadow:
      WriteStyleInnerShadowBody(&body, s);
      break;
    case LayerStyleType::BackgroundBlur:
      WriteStyleBackgroundBlurBody(&body, s);
      break;
  }
  WriteTag(stream, TagCodeForStyleType(s.type), &body);
}

// ---- Per-filter body readers ----
//
// Each reader takes the inner Tag's `tagEnd` so ReadProperty skip-to-tag-end
// alignment still works inside the filter body. The unique_ptr<LayerFilter>
// it returns has `type` pre-set by the caller based on TagCode.

std::unique_ptr<LayerFilter> ReadFilterBlurBody(::pag::DecodeStream* s, DecodeContext* /*ctx*/,
                                                uint32_t tagEnd) {
  auto f = std::make_unique<LayerFilter>();
  f->type = LayerFilterType::Blur;
  f->blurX = ReadProperty<float>(s, /*default=*/0.0f, tagEnd);
  f->blurY = ReadProperty<float>(s, /*default=*/0.0f, tagEnd);
  if (s->position() < tagEnd) {
    f->tileMode = static_cast<tgfx::TileMode>(s->readUint8());
  }
  return f;
}

std::unique_ptr<LayerFilter> ReadFilterDropShadowBody(::pag::DecodeStream* s,
                                                      DecodeContext* /*ctx*/, uint32_t tagEnd) {
  auto f = std::make_unique<LayerFilter>();
  f->type = LayerFilterType::DropShadow;
  f->offsetX = ReadProperty<float>(s, /*default=*/0.0f, tagEnd);
  f->offsetY = ReadProperty<float>(s, /*default=*/0.0f, tagEnd);
  f->blurX = ReadProperty<float>(s, /*default=*/0.0f, tagEnd);
  f->blurY = ReadProperty<float>(s, /*default=*/0.0f, tagEnd);
  f->color = ReadProperty<tgfx::Color>(s, /*default=*/tgfx::Color{}, tagEnd);
  if (s->position() < tagEnd) {
    f->shadowOnly = s->readBoolean();
  }
  return f;
}

std::unique_ptr<LayerFilter> ReadFilterInnerShadowBody(::pag::DecodeStream* s, DecodeContext* ctx,
                                                       uint32_t tagEnd) {
  auto f = ReadFilterDropShadowBody(s, ctx, tagEnd);
  f->type = LayerFilterType::InnerShadow;
  return f;
}

std::unique_ptr<LayerFilter> ReadFilterColorMatrixBody(::pag::DecodeStream* s,
                                                       DecodeContext* /*ctx*/, uint32_t tagEnd) {
  auto f = std::make_unique<LayerFilter>();
  f->type = LayerFilterType::ColorMatrix;
  f->colorMatrix =
      ReadProperty<std::array<float, 20>>(s, /*default=*/std::array<float, 20>{}, tagEnd);
  return f;
}

std::unique_ptr<LayerFilter> ReadFilterBlendBody(::pag::DecodeStream* s, DecodeContext* /*ctx*/,
                                                 uint32_t tagEnd) {
  auto f = std::make_unique<LayerFilter>();
  f->type = LayerFilterType::Blend;
  f->blendColor = ReadProperty<tgfx::Color>(s, /*default=*/tgfx::Color{}, tagEnd);
  if (s->position() < tagEnd) {
    f->blendFilterMode = static_cast<tgfx::BlendMode>(s->readUint8());
  }
  return f;
}

// ---- Per-style body readers ----

void ReadCommonStyleHead(::pag::DecodeStream* s, LayerStyle* st, uint32_t tagEnd) {
  if (s->position() < tagEnd) {
    st->excludeChildEffects = s->readBoolean();
  }
  if (s->position() < tagEnd) {
    st->blendMode = static_cast<tgfx::BlendMode>(s->readUint8());
  }
}

std::unique_ptr<LayerStyle> ReadStyleDropShadowBody(::pag::DecodeStream* s, DecodeContext* /*ctx*/,
                                                    uint32_t tagEnd) {
  auto st = std::make_unique<LayerStyle>();
  st->type = LayerStyleType::DropShadow;
  ReadCommonStyleHead(s, st.get(), tagEnd);
  st->offsetX = ReadProperty<float>(s, /*default=*/0.0f, tagEnd);
  st->offsetY = ReadProperty<float>(s, /*default=*/0.0f, tagEnd);
  st->blurX = ReadProperty<float>(s, /*default=*/0.0f, tagEnd);
  st->blurY = ReadProperty<float>(s, /*default=*/0.0f, tagEnd);
  st->color = ReadProperty<tgfx::Color>(s, /*default=*/tgfx::Color{}, tagEnd);
  if (s->position() < tagEnd) {
    st->showBehindLayer = s->readBoolean();
  }
  return st;
}

std::unique_ptr<LayerStyle> ReadStyleInnerShadowBody(::pag::DecodeStream* s, DecodeContext* /*ctx*/,
                                                     uint32_t tagEnd) {
  auto st = std::make_unique<LayerStyle>();
  st->type = LayerStyleType::InnerShadow;
  ReadCommonStyleHead(s, st.get(), tagEnd);
  st->offsetX = ReadProperty<float>(s, /*default=*/0.0f, tagEnd);
  st->offsetY = ReadProperty<float>(s, /*default=*/0.0f, tagEnd);
  st->blurX = ReadProperty<float>(s, /*default=*/0.0f, tagEnd);
  st->blurY = ReadProperty<float>(s, /*default=*/0.0f, tagEnd);
  st->color = ReadProperty<tgfx::Color>(s, /*default=*/tgfx::Color{}, tagEnd);
  return st;
}

std::unique_ptr<LayerStyle> ReadStyleBackgroundBlurBody(::pag::DecodeStream* s,
                                                        DecodeContext* /*ctx*/, uint32_t tagEnd) {
  auto st = std::make_unique<LayerStyle>();
  st->type = LayerStyleType::BackgroundBlur;
  ReadCommonStyleHead(s, st.get(), tagEnd);
  st->blurX = ReadProperty<float>(s, /*default=*/0.0f, tagEnd);
  st->blurY = ReadProperty<float>(s, /*default=*/0.0f, tagEnd);
  if (s->position() < tagEnd) {
    st->tileMode = static_cast<tgfx::TileMode>(s->readUint8());
  }
  return st;
}

}  // namespace

// ---- Public: LayerFilters container ----

void WriteLayerFilters(::pag::EncodeStream* stream,
                       const std::vector<std::unique_ptr<LayerFilter>>& filters,
                       EncodeSession* session) {
  ::pag::EncodeStream body(session->sc);
  body.writeEncodedUint32(static_cast<uint32_t>(filters.size()));
  for (const auto& f : filters) {
    if (f != nullptr) {
      WriteFilterTag(&body, *f, session);
    }
  }
  WriteTag(stream, TagCode::LayerFilters, &body);
}

void ReadLayerFilters(::pag::DecodeStream* stream, DecodeContext* ctx, uint64_t tagEnd,
                      std::vector<std::unique_ptr<LayerFilter>>* out) {
  auto guard = MakeGuard(ctx);
  uint32_t filterCount = ReadEncodedUint32Safe(stream, &guard);
  if (filterCount > limits::MAX_FILTERS_PER_LAYER) {
    ctx->error(ErrorCode::StructureLimitExceeded,
               "LayerFilters count exceeds MAX_FILTERS_PER_LAYER");
    return;
  }
  for (uint32_t i = 0; i < filterCount; ++i) {
    if (stream->position() >= tagEnd) {
      ctx->error(ErrorCode::TruncatedData, "LayerFilters truncated before all Filter tags read");
      return;
    }
    TagHeader header = ReadTagHeader(stream);
    uint64_t bodyStart = stream->position();
    uint64_t childEnd = 0;
    if (!SafeTagEnd(stream, bodyStart, header.length, ctx, &childEnd)) {
      return;
    }
    if (childEnd > tagEnd) {
      ctx->error(ErrorCode::MalformedTag, "Filter tag extends past LayerFilters body");
      return;
    }
    uint32_t childEnd32 = static_cast<uint32_t>(childEnd);
    std::unique_ptr<LayerFilter> filter;
    switch (header.code) {
      case TagCode::FilterBlur:
        filter = ReadFilterBlurBody(stream, ctx, childEnd32);
        break;
      case TagCode::FilterDropShadow:
        filter = ReadFilterDropShadowBody(stream, ctx, childEnd32);
        break;
      case TagCode::FilterInnerShadow:
        filter = ReadFilterInnerShadowBody(stream, ctx, childEnd32);
        break;
      case TagCode::FilterColorMatrix:
        filter = ReadFilterColorMatrixBody(stream, ctx, childEnd32);
        break;
      case TagCode::FilterBlend:
        filter = ReadFilterBlendBody(stream, ctx, childEnd32);
        break;
      default:
        ctx->warn(ErrorCode::UnknownTagCode, "unknown Filter TagCode; skipped");
        break;
    }
    SeekTo(stream, childEnd32);
    if (filter != nullptr) {
      out->push_back(std::move(filter));
    }
  }
}

// ---- Public: LayerStyles container ----

void WriteLayerStyles(::pag::EncodeStream* stream,
                      const std::vector<std::unique_ptr<LayerStyle>>& styles,
                      EncodeSession* session) {
  ::pag::EncodeStream body(session->sc);
  body.writeEncodedUint32(static_cast<uint32_t>(styles.size()));
  for (const auto& s : styles) {
    if (s != nullptr) {
      WriteStyleTag(&body, *s, session);
    }
  }
  WriteTag(stream, TagCode::LayerStyles, &body);
}

void ReadLayerStyles(::pag::DecodeStream* stream, DecodeContext* ctx, uint64_t tagEnd,
                     std::vector<std::unique_ptr<LayerStyle>>* out) {
  auto guard = MakeGuard(ctx);
  uint32_t styleCount = ReadEncodedUint32Safe(stream, &guard);
  if (styleCount > limits::MAX_STYLES_PER_LAYER) {
    ctx->error(ErrorCode::StructureLimitExceeded, "LayerStyles count exceeds MAX_STYLES_PER_LAYER");
    return;
  }
  for (uint32_t i = 0; i < styleCount; ++i) {
    if (stream->position() >= tagEnd) {
      ctx->error(ErrorCode::TruncatedData, "LayerStyles truncated before all Style tags read");
      return;
    }
    TagHeader header = ReadTagHeader(stream);
    uint64_t bodyStart = stream->position();
    uint64_t childEnd = 0;
    if (!SafeTagEnd(stream, bodyStart, header.length, ctx, &childEnd)) {
      return;
    }
    if (childEnd > tagEnd) {
      ctx->error(ErrorCode::MalformedTag, "Style tag extends past LayerStyles body");
      return;
    }
    uint32_t childEnd32 = static_cast<uint32_t>(childEnd);
    std::unique_ptr<LayerStyle> style;
    switch (header.code) {
      case TagCode::StyleDropShadow:
        style = ReadStyleDropShadowBody(stream, ctx, childEnd32);
        break;
      case TagCode::StyleInnerShadow:
        style = ReadStyleInnerShadowBody(stream, ctx, childEnd32);
        break;
      case TagCode::StyleBackgroundBlur:
        style = ReadStyleBackgroundBlurBody(stream, ctx, childEnd32);
        break;
      default:
        ctx->warn(ErrorCode::UnknownTagCode, "unknown Style TagCode; skipped");
        break;
    }
    SeekTo(stream, childEnd32);
    if (style != nullptr) {
      out->push_back(std::move(style));
    }
  }
}

}  // namespace pagx::pag
