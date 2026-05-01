// Copyright (C) 2026 Tencent. All rights reserved.
#include "pagx/pag/ElementTags.h"
#include "pagx/pag/PropertyEncoding.h"
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

// Maps VectorElementType -> outer TagCode used on the wire.
TagCode TagCodeForElementType(VectorElementType t) {
  switch (t) {
    case VectorElementType::Rectangle:
      return TagCode::ElementRectangle;
    case VectorElementType::Ellipse:
      return TagCode::ElementEllipse;
    case VectorElementType::Polystar:
      return TagCode::ElementPolystar;
    case VectorElementType::ShapePath:
      return TagCode::ElementShapePath;
    case VectorElementType::FillStyle:
      return TagCode::ElementFillStyle;
    case VectorElementType::StrokeStyle:
      return TagCode::ElementStrokeStyle;
    case VectorElementType::TrimPath:
      return TagCode::ElementTrimPath;
    case VectorElementType::RoundCorner:
      return TagCode::ElementRoundCorner;
    case VectorElementType::MergePath:
      return TagCode::ElementMergePath;
    case VectorElementType::Repeater:
      return TagCode::ElementRepeater;
    case VectorElementType::Text:
      return TagCode::ElementText;
    case VectorElementType::TextPath:
      return TagCode::ElementTextPath;
    case VectorElementType::TextModifier:
      return TagCode::ElementTextModifier;
    case VectorElementType::VectorGroup:
      return TagCode::ElementVectorGroup;
  }
  // Unreachable — VectorElementType is a closed enum.
  return TagCode::ElementRectangle;
}

VectorElementType ElementTypeForTagCode(TagCode code, bool* outOk) {
  *outOk = true;
  switch (code) {
    case TagCode::ElementRectangle:
      return VectorElementType::Rectangle;
    case TagCode::ElementEllipse:
      return VectorElementType::Ellipse;
    case TagCode::ElementPolystar:
      return VectorElementType::Polystar;
    case TagCode::ElementShapePath:
      return VectorElementType::ShapePath;
    case TagCode::ElementFillStyle:
      return VectorElementType::FillStyle;
    case TagCode::ElementStrokeStyle:
      return VectorElementType::StrokeStyle;
    case TagCode::ElementTrimPath:
      return VectorElementType::TrimPath;
    case TagCode::ElementRoundCorner:
      return VectorElementType::RoundCorner;
    case TagCode::ElementMergePath:
      return VectorElementType::MergePath;
    case TagCode::ElementRepeater:
      return VectorElementType::Repeater;
    case TagCode::ElementText:
      return VectorElementType::Text;
    case TagCode::ElementTextPath:
      return VectorElementType::TextPath;
    case TagCode::ElementTextModifier:
      return VectorElementType::TextModifier;
    case TagCode::ElementVectorGroup:
      return VectorElementType::VectorGroup;
    default:
      *outOk = false;
      return VectorElementType::Rectangle;
  }
}

// ============================================================================
// Per-element InlineBody readers/writers.
//
// Convention: each Write<Foo>InlineBody emits the field bytes only (no
// TagHeader); the public Write<Foo> function wraps it via WriteTag.
// Read<Foo>InlineBody mirrors and consumes exactly the field bytes; the
// caller (ReadVectorElement) handles the SeekTo(tagEnd) alignment so that
// future field-level appends are forward-compatible (§6.5 ①).
// ============================================================================

// ---------- Rectangle ----------

void WriteElementRectangleBody(::pag::EncodeStream* body, const ElementRectangleData& d) {
  WriteProperty<tgfx::Point>(body, d.position, /*default=*/tgfx::Point{});
  WriteProperty<tgfx::Point>(body, d.size, /*default=*/tgfx::Point{});
  WriteProperty<float>(body, d.roundness, /*default=*/0.0f);
  body->writeBoolean(d.reversed);
}

std::unique_ptr<ElementRectangleData> ReadElementRectangleBody(::pag::DecodeStream* s,
                                                               DecodeContext* /*ctx*/,
                                                               uint32_t te) {
  auto d = std::make_unique<ElementRectangleData>();
  d->position = ReadProperty<tgfx::Point>(s, /*default=*/tgfx::Point{}, te);
  d->size = ReadProperty<tgfx::Point>(s, /*default=*/tgfx::Point{}, te);
  d->roundness = ReadProperty<float>(s, /*default=*/0.0f, te);
  if (s->position() < te) {
    d->reversed = s->readBoolean();
  }
  return d;
}

// ---------- Ellipse ----------

void WriteElementEllipseBody(::pag::EncodeStream* body, const ElementEllipseData& d) {
  WriteProperty<tgfx::Point>(body, d.position, /*default=*/tgfx::Point{});
  WriteProperty<tgfx::Point>(body, d.size, /*default=*/tgfx::Point{});
  body->writeBoolean(d.reversed);
}

std::unique_ptr<ElementEllipseData> ReadElementEllipseBody(::pag::DecodeStream* s,
                                                           DecodeContext* /*ctx*/, uint32_t te) {
  auto d = std::make_unique<ElementEllipseData>();
  d->position = ReadProperty<tgfx::Point>(s, /*default=*/tgfx::Point{}, te);
  d->size = ReadProperty<tgfx::Point>(s, /*default=*/tgfx::Point{}, te);
  if (s->position() < te) {
    d->reversed = s->readBoolean();
  }
  return d;
}

// ---------- Polystar ----------

void WriteElementPolystarBody(::pag::EncodeStream* body, const ElementPolystarData& d) {
  WriteProperty<tgfx::Point>(body, d.position, /*default=*/tgfx::Point{});
  WriteProperty<float>(body, d.pointCount, /*default=*/5.0f);
  WriteProperty<float>(body, d.outerRadius, /*default=*/0.0f);
  WriteProperty<float>(body, d.innerRadius, /*default=*/0.0f);
  WriteProperty<float>(body, d.outerRoundness, /*default=*/0.0f);
  WriteProperty<float>(body, d.innerRoundness, /*default=*/0.0f);
  WriteProperty<float>(body, d.rotation, /*default=*/0.0f);
  body->writeBoolean(d.reversed);
  body->writeUint8(static_cast<uint8_t>(d.polystarType));
}

std::unique_ptr<ElementPolystarData> ReadElementPolystarBody(::pag::DecodeStream* s,
                                                             DecodeContext* ctx, uint32_t te) {
  auto d = std::make_unique<ElementPolystarData>();
  d->position = ReadProperty<tgfx::Point>(s, /*default=*/tgfx::Point{}, te);
  d->pointCount = ReadProperty<float>(s, /*default=*/5.0f, te);
  d->outerRadius = ReadProperty<float>(s, /*default=*/0.0f, te);
  d->innerRadius = ReadProperty<float>(s, /*default=*/0.0f, te);
  d->outerRoundness = ReadProperty<float>(s, /*default=*/0.0f, te);
  d->innerRoundness = ReadProperty<float>(s, /*default=*/0.0f, te);
  d->rotation = ReadProperty<float>(s, /*default=*/0.0f, te);
  if (s->position() < te) {
    d->reversed = s->readBoolean();
  }
  if (s->position() < te) {
    uint8_t pt = s->readUint8();
    // PolystarType only declares Star (0) and Polygon (1).
    if (pt > 1) {
      ctx->warn(ErrorCode::InvalidEnumValue,
                "ElementPolystar.polystarType out of range; using Polygon");
      d->polystarType = ::tgfx::PolystarType::Polygon;
    } else {
      d->polystarType = static_cast<::tgfx::PolystarType>(pt);
    }
  }
  return d;
}

// ---------- ShapePath (Phase 5b: includes Path Property) ----------
//
// Body layout (Phase 5b):
//   Property<Point> position
//   Property<Path>  path
//   bool            reversed

void WriteElementShapePathBody(::pag::EncodeStream* body, const ElementShapePathData& d) {
  WriteProperty<tgfx::Point>(body, d.position, /*default=*/tgfx::Point{});
  WritePathProperty(body, d.path, /*default=*/tgfx::Path{});
  body->writeBoolean(d.reversed);
}

std::unique_ptr<ElementShapePathData> ReadElementShapePathBody(::pag::DecodeStream* s,
                                                               DecodeContext* ctx, uint32_t te) {
  auto d = std::make_unique<ElementShapePathData>();
  d->position = ReadProperty<tgfx::Point>(s, /*default=*/tgfx::Point{}, te);
  d->path = ReadPathProperty(s, ctx, /*default=*/tgfx::Path{}, te);
  if (ctx->hasError()) {
    return nullptr;
  }
  if (s->position() < te) {
    d->reversed = s->readBoolean();
  }
  return d;
}

// ---------- ShapeStyleData inline (all 6 sourceType branches — Phase 6) ----
//
// §D.11 prescribes a wrapping `u16 innerLength` (u16=0xFFFF escape → u32
// extension) for forward-compat. The inner payload starts with the 4-byte
// common header (sourceType / alpha / blendMode / overrideBlendMode) and
// then branches by sourceType. A newer writer appending fields at the tail
// of a branch stays compatible because the Reader always force-aligns to
// innerEnd after the switch. An unknown sourceType collapses to the
// common-header shape via warn + skip-to-innerEnd.

void WriteShapeStyleDataInline(::pag::EncodeStream* body, const ShapeStyleData& s) {
  ::pag::EncodeStream inner(body->context);
  inner.writeUint8(static_cast<uint8_t>(s.sourceType));
  WriteProperty<float>(&inner, s.alpha, /*default=*/1.0f);
  inner.writeUint8(static_cast<uint8_t>(s.blendMode));
  inner.writeBoolean(s.overrideBlendMode);
  switch (s.sourceType) {
    case ColorSourceType::SolidColor:
      WriteProperty<tgfx::Color>(&inner, s.color, /*default=*/tgfx::Color{});
      break;
    case ColorSourceType::LinearGradient:
      WriteProperty<std::vector<tgfx::Color>>(&inner, s.stopColors,
                                              /*default=*/std::vector<tgfx::Color>{});
      WriteProperty<std::vector<float>>(&inner, s.stopPositions,
                                        /*default=*/std::vector<float>{});
      WriteProperty<tgfx::Matrix>(&inner, s.gradientMatrix, /*default=*/tgfx::Matrix::I());
      inner.writeBoolean(s.fitsToGeometry);
      WriteProperty<tgfx::Point>(&inner, s.startPoint, /*default=*/tgfx::Point{0.0f, 0.0f});
      WriteProperty<tgfx::Point>(&inner, s.endPoint, /*default=*/tgfx::Point{1.0f, 0.0f});
      break;
    case ColorSourceType::RadialGradient:
    case ColorSourceType::DiamondGradient:
      WriteProperty<std::vector<tgfx::Color>>(&inner, s.stopColors,
                                              /*default=*/std::vector<tgfx::Color>{});
      WriteProperty<std::vector<float>>(&inner, s.stopPositions,
                                        /*default=*/std::vector<float>{});
      WriteProperty<tgfx::Matrix>(&inner, s.gradientMatrix, /*default=*/tgfx::Matrix::I());
      inner.writeBoolean(s.fitsToGeometry);
      WriteProperty<tgfx::Point>(&inner, s.center, /*default=*/tgfx::Point{});
      WriteProperty<float>(&inner, s.radius, /*default=*/0.0f);
      break;
    case ColorSourceType::ConicGradient:
      WriteProperty<std::vector<tgfx::Color>>(&inner, s.stopColors,
                                              /*default=*/std::vector<tgfx::Color>{});
      WriteProperty<std::vector<float>>(&inner, s.stopPositions,
                                        /*default=*/std::vector<float>{});
      WriteProperty<tgfx::Matrix>(&inner, s.gradientMatrix, /*default=*/tgfx::Matrix::I());
      inner.writeBoolean(s.fitsToGeometry);
      WriteProperty<tgfx::Point>(&inner, s.center, /*default=*/tgfx::Point{});
      WriteProperty<float>(&inner, s.radius, /*default=*/0.0f);
      WriteProperty<float>(&inner, s.startAngle, /*default=*/0.0f);
      WriteProperty<float>(&inner, s.endAngle, /*default=*/360.0f);
      break;
    case ColorSourceType::ImagePattern:
      inner.writeUint32(s.patternImageIndex);
      inner.writeUint8(static_cast<uint8_t>(s.tileModeX));
      inner.writeUint8(static_cast<uint8_t>(s.tileModeY));
      inner.writeUint8(static_cast<uint8_t>(s.filterMode));
      inner.writeUint8(static_cast<uint8_t>(s.mipmapMode));
      inner.writeUint8(static_cast<uint8_t>(s.scaleMode));
      WriteProperty<tgfx::Matrix>(&inner, s.patternMatrix, /*default=*/tgfx::Matrix::I());
      break;
  }
  uint32_t innerLen = inner.length();
  if (innerLen < 0xFFFF) {
    body->writeUint16(static_cast<uint16_t>(innerLen));
  } else {
    body->writeUint16(0xFFFF);
    body->writeUint32(innerLen);
  }
  body->writeBytes(&inner);
}

std::unique_ptr<ShapeStyleData> ReadShapeStyleDataInline(::pag::DecodeStream* s, DecodeContext* ctx,
                                                         uint32_t enclosingEnd) {
  auto style = std::make_unique<ShapeStyleData>();

  uint64_t innerStart = s->position();
  uint32_t innerLen = s->readUint16();
  uint32_t innerLenBytes = 2;
  if (innerLen == 0xFFFF) {
    innerLen = s->readUint32();
    innerLenBytes = 6;
  }
  uint64_t innerContentEnd64 = innerStart + innerLenBytes + static_cast<uint64_t>(innerLen);
  if (innerContentEnd64 > enclosingEnd) {
    ctx->error(ErrorCode::MalformedTag, "ShapeStyle innerLength overflow or exceeds enclosing tag");
    return nullptr;
  }
  uint32_t innerEnd = static_cast<uint32_t>(innerContentEnd64);

  uint8_t srcByte = s->readUint8();
  // ColorSourceType currently holds 0..5 (SolidColor / LinearGradient /
  // RadialGradient / ConicGradient / DiamondGradient / ImagePattern). An
  // unknown value collapses the whole branch payload to SolidColor defaults
  // and jumps to innerEnd below.
  bool knownSource = (srcByte <= 5);
  if (!knownSource) {
    ctx->warn(ErrorCode::InvalidEnumValue, "ShapeStyle sourceType out of range; using SolidColor");
    style->sourceType = ColorSourceType::SolidColor;
  } else {
    style->sourceType = static_cast<ColorSourceType>(srcByte);
  }
  style->alpha = ReadProperty<float>(s, /*default=*/1.0f, innerEnd);
  style->blendMode = static_cast<tgfx::BlendMode>(s->readUint8());
  style->overrideBlendMode = (s->position() < innerEnd) ? s->readBoolean() : false;

  if (knownSource) {
    switch (style->sourceType) {
      case ColorSourceType::SolidColor:
        if (s->position() < innerEnd) {
          style->color = ReadProperty<tgfx::Color>(s, /*default=*/tgfx::Color{}, innerEnd);
        }
        break;
      case ColorSourceType::LinearGradient:
        style->stopColors = ReadProperty<std::vector<tgfx::Color>>(
            s, /*default=*/std::vector<tgfx::Color>{}, innerEnd);
        style->stopPositions =
            ReadProperty<std::vector<float>>(s, /*default=*/std::vector<float>{}, innerEnd);
        style->gradientMatrix =
            ReadProperty<tgfx::Matrix>(s, /*default=*/tgfx::Matrix::I(), innerEnd);
        style->fitsToGeometry = (s->position() < innerEnd) ? s->readBoolean() : true;
        style->startPoint =
            ReadProperty<tgfx::Point>(s, /*default=*/tgfx::Point{0.0f, 0.0f}, innerEnd);
        style->endPoint =
            ReadProperty<tgfx::Point>(s, /*default=*/tgfx::Point{1.0f, 0.0f}, innerEnd);
        break;
      case ColorSourceType::RadialGradient:
      case ColorSourceType::DiamondGradient:
        style->stopColors = ReadProperty<std::vector<tgfx::Color>>(
            s, /*default=*/std::vector<tgfx::Color>{}, innerEnd);
        style->stopPositions =
            ReadProperty<std::vector<float>>(s, /*default=*/std::vector<float>{}, innerEnd);
        style->gradientMatrix =
            ReadProperty<tgfx::Matrix>(s, /*default=*/tgfx::Matrix::I(), innerEnd);
        style->fitsToGeometry = (s->position() < innerEnd) ? s->readBoolean() : true;
        style->center = ReadProperty<tgfx::Point>(s, /*default=*/tgfx::Point{}, innerEnd);
        style->radius = ReadProperty<float>(s, /*default=*/0.0f, innerEnd);
        break;
      case ColorSourceType::ConicGradient:
        style->stopColors = ReadProperty<std::vector<tgfx::Color>>(
            s, /*default=*/std::vector<tgfx::Color>{}, innerEnd);
        style->stopPositions =
            ReadProperty<std::vector<float>>(s, /*default=*/std::vector<float>{}, innerEnd);
        style->gradientMatrix =
            ReadProperty<tgfx::Matrix>(s, /*default=*/tgfx::Matrix::I(), innerEnd);
        style->fitsToGeometry = (s->position() < innerEnd) ? s->readBoolean() : true;
        style->center = ReadProperty<tgfx::Point>(s, /*default=*/tgfx::Point{}, innerEnd);
        style->radius = ReadProperty<float>(s, /*default=*/0.0f, innerEnd);
        style->startAngle = ReadProperty<float>(s, /*default=*/0.0f, innerEnd);
        style->endAngle = ReadProperty<float>(s, /*default=*/360.0f, innerEnd);
        break;
      case ColorSourceType::ImagePattern:
        // Positional enums are raw bytes — reading past innerEnd is guarded
        // by the outer skip below, but we still check before each read to
        // honour a writer that emitted an undersized tail.
        if (s->position() + 4 <= innerEnd) {
          style->patternImageIndex = s->readUint32();
        }
        if (s->position() < innerEnd)
          style->tileModeX = static_cast<tgfx::TileMode>(s->readUint8());
        if (s->position() < innerEnd)
          style->tileModeY = static_cast<tgfx::TileMode>(s->readUint8());
        if (s->position() < innerEnd)
          style->filterMode = static_cast<tgfx::FilterMode>(s->readUint8());
        if (s->position() < innerEnd)
          style->mipmapMode = static_cast<tgfx::MipmapMode>(s->readUint8());
        if (s->position() < innerEnd) style->scaleMode = static_cast<ScaleMode>(s->readUint8());
        if (s->position() < innerEnd) {
          style->patternMatrix =
              ReadProperty<tgfx::Matrix>(s, /*default=*/tgfx::Matrix::I(), innerEnd);
        }
        break;
    }
  }

  // Forward-compat tail align: any unread bytes before innerEnd belong to
  // future fields the writer added. Skip them.
  uint32_t cur = s->position();
  if (cur < innerEnd) {
    s->skip(innerEnd - cur);
  }
  return style;
}

// ---------- FillStyle ----------

void WriteElementFillStyleBody(::pag::EncodeStream* body, const ElementFillStyleData& d,
                               EncodeSession* session) {
  if (d.style != nullptr) {
    WriteShapeStyleDataInline(body, *d.style);
  } else {
    // Style missing — emit a minimal SolidColor inline so the wire is
    // self-consistent. Baker should not produce nullptr style (Phase 5c
    // will validate); defensive fallback for tests / fuzz.
    ShapeStyleData fallback{};
    WriteShapeStyleDataInline(body, fallback);
  }
  body->writeUint8(static_cast<uint8_t>(d.fillRule));
  body->writeUint8(static_cast<uint8_t>(d.placement));
  (void)session;
}

std::unique_ptr<ElementFillStyleData> ReadElementFillStyleBody(::pag::DecodeStream* s,
                                                               DecodeContext* ctx, uint32_t te) {
  auto d = std::make_unique<ElementFillStyleData>();
  d->style = ReadShapeStyleDataInline(s, ctx, te);
  if (ctx->hasError()) {
    return nullptr;
  }
  if (s->position() < te) {
    uint8_t fr = s->readUint8();
    if (fr > 1) {
      ctx->warn(ErrorCode::InvalidEnumValue,
                "ElementFillStyle.fillRule out of range; using Winding");
      d->fillRule = ::tgfx::FillRule::Winding;
    } else {
      d->fillRule = static_cast<::tgfx::FillRule>(fr);
    }
  }
  if (s->position() < te) {
    uint8_t pl = s->readUint8();
    if (pl > 1) {
      ctx->warn(ErrorCode::InvalidEnumValue,
                "ElementFillStyle.placement out of range; using Background");
      d->placement = ::tgfx::LayerPlacement::Background;
    } else {
      d->placement = static_cast<::tgfx::LayerPlacement>(pl);
    }
  }
  return d;
}

// ---------- StrokeStyle (lineDashPattern is Property<vector<float>>; Phase
//            5a writes empty vector default to keep wire small) ----------

void WriteElementStrokeStyleBody(::pag::EncodeStream* body, const ElementStrokeStyleData& d) {
  if (d.style != nullptr) {
    WriteShapeStyleDataInline(body, *d.style);
  } else {
    ShapeStyleData fallback{};
    WriteShapeStyleDataInline(body, fallback);
  }
  WriteProperty<float>(body, d.strokeWidth, /*default=*/1.0f);
  body->writeUint8(static_cast<uint8_t>(d.lineCap));
  body->writeUint8(static_cast<uint8_t>(d.lineJoin));
  WriteProperty<float>(body, d.miterLimit, /*default=*/4.0f);
  // Phase 5a: emit lineDashPattern's count varU32 only (= 0 for the default
  // empty vector). Property<vector<T>> full encoding lands together with
  // Phase 6 PaintBaker, where its sole producer is.
  body->writeEncodedUint32(static_cast<uint32_t>(d.lineDashPattern.value.size()));
  for (float v : d.lineDashPattern.value) {
    body->writeFloat(v);
  }
  WriteProperty<float>(body, d.lineDashPhase, /*default=*/0.0f);
  body->writeBoolean(d.lineDashAdaptive);
  body->writeUint8(static_cast<uint8_t>(d.strokeAlign));
  body->writeUint8(static_cast<uint8_t>(d.placement));
}

std::unique_ptr<ElementStrokeStyleData> ReadElementStrokeStyleBody(::pag::DecodeStream* s,
                                                                   DecodeContext* ctx,
                                                                   uint32_t te) {
  auto d = std::make_unique<ElementStrokeStyleData>();
  d->style = ReadShapeStyleDataInline(s, ctx, te);
  if (ctx->hasError()) {
    return nullptr;
  }
  d->strokeWidth = ReadProperty<float>(s, /*default=*/1.0f, te);
  if (s->position() < te) {
    uint8_t lc = s->readUint8();
    if (lc > 2) {  // Butt / Round / Square
      ctx->warn(ErrorCode::InvalidEnumValue, "ElementStroke.lineCap out of range; using Butt");
      d->lineCap = ::tgfx::LineCap::Butt;
    } else {
      d->lineCap = static_cast<::tgfx::LineCap>(lc);
    }
  }
  if (s->position() < te) {
    uint8_t lj = s->readUint8();
    if (lj > 2) {  // Miter / Round / Bevel
      ctx->warn(ErrorCode::InvalidEnumValue, "ElementStroke.lineJoin out of range; using Miter");
      d->lineJoin = ::tgfx::LineJoin::Miter;
    } else {
      d->lineJoin = static_cast<::tgfx::LineJoin>(lj);
    }
  }
  d->miterLimit = ReadProperty<float>(s, /*default=*/4.0f, te);
  // Read lineDashPattern: varU32 count + count * float.
  if (s->position() < te) {
    auto guard = MakeGuard(ctx);
    uint32_t cnt = ReadEncodedUint32Safe(s, &guard);
    if (cnt > limits::MAX_VECTOR_VALUE_ELEMENTS) {
      ctx->error(ErrorCode::StructureLimitExceeded,
                 "ElementStroke.lineDashPattern count exceeds MAX_VECTOR_VALUE_ELEMENTS");
      return nullptr;
    }
    std::vector<float> v;
    v.reserve(cnt);
    for (uint32_t i = 0; i < cnt; ++i) {
      v.push_back(s->readFloat());
    }
    d->lineDashPattern = MakeProp(std::move(v));
  }
  d->lineDashPhase = ReadProperty<float>(s, /*default=*/0.0f, te);
  if (s->position() < te) {
    d->lineDashAdaptive = s->readBoolean();
  }
  if (s->position() < te) {
    uint8_t sa = s->readUint8();
    if (sa > 2) {  // Center / Inside / Outside
      ctx->warn(ErrorCode::InvalidEnumValue,
                "ElementStroke.strokeAlign out of range; using Center");
      d->strokeAlign = ::tgfx::StrokeAlign::Center;
    } else {
      d->strokeAlign = static_cast<::tgfx::StrokeAlign>(sa);
    }
  }
  if (s->position() < te) {
    uint8_t pl = s->readUint8();
    if (pl > 1) {
      ctx->warn(ErrorCode::InvalidEnumValue,
                "ElementStroke.placement out of range; using Background");
      d->placement = ::tgfx::LayerPlacement::Background;
    } else {
      d->placement = static_cast<::tgfx::LayerPlacement>(pl);
    }
  }
  return d;
}

// ---------- TrimPath ----------

void WriteElementTrimPathBody(::pag::EncodeStream* body, const ElementTrimPathData& d) {
  WriteProperty<float>(body, d.start, /*default=*/0.0f);
  WriteProperty<float>(body, d.end, /*default=*/100.0f);
  WriteProperty<float>(body, d.offset, /*default=*/0.0f);
  body->writeUint8(static_cast<uint8_t>(d.trimType));
}

std::unique_ptr<ElementTrimPathData> ReadElementTrimPathBody(::pag::DecodeStream* s,
                                                             DecodeContext* ctx, uint32_t te) {
  auto d = std::make_unique<ElementTrimPathData>();
  d->start = ReadProperty<float>(s, /*default=*/0.0f, te);
  d->end = ReadProperty<float>(s, /*default=*/100.0f, te);
  d->offset = ReadProperty<float>(s, /*default=*/0.0f, te);
  if (s->position() < te) {
    uint8_t tt = s->readUint8();
    if (tt > 1) {
      ctx->warn(ErrorCode::InvalidEnumValue,
                "ElementTrimPath.trimType out of range; using Separate");
      d->trimType = ::tgfx::TrimPathType::Separate;
    } else {
      d->trimType = static_cast<::tgfx::TrimPathType>(tt);
    }
  }
  return d;
}

// ---------- RoundCorner ----------

void WriteElementRoundCornerBody(::pag::EncodeStream* body, const ElementRoundCornerData& d) {
  WriteProperty<float>(body, d.radius, /*default=*/0.0f);
}

std::unique_ptr<ElementRoundCornerData> ReadElementRoundCornerBody(::pag::DecodeStream* s,
                                                                   DecodeContext* /*ctx*/,
                                                                   uint32_t te) {
  auto d = std::make_unique<ElementRoundCornerData>();
  d->radius = ReadProperty<float>(s, /*default=*/0.0f, te);
  return d;
}

// ---------- MergePath ----------

void WriteElementMergePathBody(::pag::EncodeStream* body, const ElementMergePathData& d) {
  body->writeUint8(static_cast<uint8_t>(d.mode));
}

std::unique_ptr<ElementMergePathData> ReadElementMergePathBody(::pag::DecodeStream* s,
                                                               DecodeContext* ctx, uint32_t te) {
  auto d = std::make_unique<ElementMergePathData>();
  if (s->position() < te) {
    uint8_t mode = s->readUint8();
    if (mode > 4) {  // Append / Union / Difference / Intersect / XOR
      ctx->warn(ErrorCode::InvalidEnumValue, "ElementMergePath.mode out of range; using Append");
      d->mode = ::tgfx::MergePathOp::Append;
    } else {
      d->mode = static_cast<::tgfx::MergePathOp>(mode);
    }
  }
  return d;
}

// ---------- Repeater ----------

void WriteElementRepeaterBody(::pag::EncodeStream* body, const ElementRepeaterData& d) {
  WriteProperty<float>(body, d.copies, /*default=*/1.0f);
  WriteProperty<float>(body, d.offset, /*default=*/0.0f);
  body->writeUint8(static_cast<uint8_t>(d.order));
  WriteProperty<tgfx::Point>(body, d.anchor, /*default=*/tgfx::Point{});
  WriteProperty<tgfx::Point>(body, d.position, /*default=*/tgfx::Point{});
  WriteProperty<float>(body, d.rotation, /*default=*/0.0f);
  WriteProperty<tgfx::Point>(body, d.scale, /*default=*/tgfx::Point{1.0f, 1.0f});
  WriteProperty<float>(body, d.startAlpha, /*default=*/1.0f);
  WriteProperty<float>(body, d.endAlpha, /*default=*/1.0f);
}

std::unique_ptr<ElementRepeaterData> ReadElementRepeaterBody(::pag::DecodeStream* s,
                                                             DecodeContext* ctx, uint32_t te) {
  auto d = std::make_unique<ElementRepeaterData>();
  d->copies = ReadProperty<float>(s, /*default=*/1.0f, te);
  d->offset = ReadProperty<float>(s, /*default=*/0.0f, te);
  if (s->position() < te) {
    uint8_t ord = s->readUint8();
    if (ord > 1) {
      ctx->warn(ErrorCode::InvalidEnumValue,
                "ElementRepeater.order out of range; using BelowOriginal");
      d->order = ::tgfx::RepeaterOrder::BelowOriginal;
    } else {
      d->order = static_cast<::tgfx::RepeaterOrder>(ord);
    }
  }
  d->anchor = ReadProperty<tgfx::Point>(s, /*default=*/tgfx::Point{}, te);
  d->position = ReadProperty<tgfx::Point>(s, /*default=*/tgfx::Point{}, te);
  d->rotation = ReadProperty<float>(s, /*default=*/0.0f, te);
  d->scale = ReadProperty<tgfx::Point>(s, /*default=*/tgfx::Point{1.0f, 1.0f}, te);
  d->startAlpha = ReadProperty<float>(s, /*default=*/1.0f, te);
  d->endAlpha = ReadProperty<float>(s, /*default=*/1.0f, te);
  return d;
}

// ---------- VectorGroup (recursive) ----------
//
// §D.11 VectorGroup body: varU32 elementCount + elementCount × Element Tag,
// followed by group-level Property fields (anchor / position / scale /
// rotation / alpha / skew / skewAxis). Recursion is bounded by
// MAX_VECTOR_ELEMENT_DEPTH via DecodeContext.currentVectorElementDepth.

void WriteElementVectorGroupBody(::pag::EncodeStream* body, const ElementVectorGroupData& d,
                                 EncodeSession* session) {
  body->writeEncodedUint32(static_cast<uint32_t>(d.elements.size()));
  for (const auto& child : d.elements) {
    WriteVectorElement(body, *child, session);
  }
  WriteProperty<tgfx::Point>(body, d.anchor, /*default=*/tgfx::Point{});
  WriteProperty<tgfx::Point>(body, d.position, /*default=*/tgfx::Point{});
  WriteProperty<tgfx::Point>(body, d.scale, /*default=*/tgfx::Point{1.0f, 1.0f});
  WriteProperty<float>(body, d.rotation, /*default=*/0.0f);
  WriteProperty<float>(body, d.alpha, /*default=*/1.0f);
  WriteProperty<float>(body, d.skew, /*default=*/0.0f);
  WriteProperty<float>(body, d.skewAxis, /*default=*/0.0f);
}

std::unique_ptr<ElementVectorGroupData> ReadElementVectorGroupBody(::pag::DecodeStream* s,
                                                                   DecodeContext* ctx,
                                                                   uint32_t te) {
  if (ctx->currentVectorElementDepth >= limits::MAX_VECTOR_ELEMENT_DEPTH) {
    ctx->error(ErrorCode::StructureLimitExceeded,
               "VectorGroup nesting exceeds MAX_VECTOR_ELEMENT_DEPTH");
    return nullptr;
  }
  ++ctx->currentVectorElementDepth;

  auto d = std::make_unique<ElementVectorGroupData>();
  auto guard = MakeGuard(ctx);

  uint32_t childCount = ReadEncodedUint32Safe(s, &guard);
  if (childCount > limits::MAX_VECTOR_ELEMENTS_PER_PAYLOAD) {
    ctx->error(ErrorCode::StructureLimitExceeded,
               "VectorGroup elementCount exceeds MAX_VECTOR_ELEMENTS_PER_PAYLOAD");
    --ctx->currentVectorElementDepth;
    return nullptr;
  }

  for (uint32_t i = 0; i < childCount; ++i) {
    if (s->position() >= te) {
      ctx->error(ErrorCode::TruncatedData, "VectorGroup truncated before all element tags read");
      --ctx->currentVectorElementDepth;
      return nullptr;
    }
    TagHeader header = ReadTagHeader(s);
    uint64_t bodyStart = s->position();
    uint64_t childEnd = 0;
    if (!SafeTagEnd(s, bodyStart, header.length, ctx, &childEnd)) {
      --ctx->currentVectorElementDepth;
      return nullptr;
    }
    if (childEnd > te) {
      ctx->error(ErrorCode::MalformedTag, "child element extends past VectorGroup body");
      --ctx->currentVectorElementDepth;
      return nullptr;
    }
    auto child = ReadVectorElement(s, ctx, header.code, childEnd);
    if (ctx->hasError()) {
      --ctx->currentVectorElementDepth;
      return nullptr;
    }
    if (child != nullptr) {
      d->elements.push_back(std::move(child));
    }
    SeekTo(s, static_cast<uint32_t>(childEnd));
  }

  d->anchor = ReadProperty<tgfx::Point>(s, /*default=*/tgfx::Point{}, te);
  d->position = ReadProperty<tgfx::Point>(s, /*default=*/tgfx::Point{}, te);
  d->scale = ReadProperty<tgfx::Point>(s, /*default=*/tgfx::Point{1.0f, 1.0f}, te);
  d->rotation = ReadProperty<float>(s, /*default=*/0.0f, te);
  d->alpha = ReadProperty<float>(s, /*default=*/1.0f, te);
  d->skew = ReadProperty<float>(s, /*default=*/0.0f, te);
  d->skewAxis = ReadProperty<float>(s, /*default=*/0.0f, te);

  --ctx->currentVectorElementDepth;
  return d;
}

}  // namespace

// ============================================================================
// Public entry points
// ============================================================================

void WriteVectorElement(::pag::EncodeStream* stream, const VectorElement& element,
                        EncodeSession* session) {
  ::pag::EncodeStream body(session->sc);
  switch (element.type) {
    case VectorElementType::Rectangle: {
      const auto& payload = std::get<std::unique_ptr<ElementRectangleData>>(element.payload);
      WriteElementRectangleBody(&body, *payload);
      break;
    }
    case VectorElementType::Ellipse: {
      const auto& payload = std::get<std::unique_ptr<ElementEllipseData>>(element.payload);
      WriteElementEllipseBody(&body, *payload);
      break;
    }
    case VectorElementType::Polystar: {
      const auto& payload = std::get<std::unique_ptr<ElementPolystarData>>(element.payload);
      WriteElementPolystarBody(&body, *payload);
      break;
    }
    case VectorElementType::ShapePath: {
      const auto& payload = std::get<std::unique_ptr<ElementShapePathData>>(element.payload);
      WriteElementShapePathBody(&body, *payload);
      break;
    }
    case VectorElementType::FillStyle: {
      const auto& payload = std::get<std::unique_ptr<ElementFillStyleData>>(element.payload);
      WriteElementFillStyleBody(&body, *payload, session);
      break;
    }
    case VectorElementType::StrokeStyle: {
      const auto& payload = std::get<std::unique_ptr<ElementStrokeStyleData>>(element.payload);
      WriteElementStrokeStyleBody(&body, *payload);
      break;
    }
    case VectorElementType::TrimPath: {
      const auto& payload = std::get<std::unique_ptr<ElementTrimPathData>>(element.payload);
      WriteElementTrimPathBody(&body, *payload);
      break;
    }
    case VectorElementType::RoundCorner: {
      const auto& payload = std::get<std::unique_ptr<ElementRoundCornerData>>(element.payload);
      WriteElementRoundCornerBody(&body, *payload);
      break;
    }
    case VectorElementType::MergePath: {
      const auto& payload = std::get<std::unique_ptr<ElementMergePathData>>(element.payload);
      WriteElementMergePathBody(&body, *payload);
      break;
    }
    case VectorElementType::Repeater: {
      const auto& payload = std::get<std::unique_ptr<ElementRepeaterData>>(element.payload);
      WriteElementRepeaterBody(&body, *payload);
      break;
    }
    case VectorElementType::VectorGroup: {
      const auto& payload = std::get<std::unique_ptr<ElementVectorGroupData>>(element.payload);
      WriteElementVectorGroupBody(&body, *payload, session);
      break;
    }
    case VectorElementType::Text:
    case VectorElementType::TextPath:
    case VectorElementType::TextModifier: {
      // Phase 5a deferral: emit empty body so downstream Phase 8 can land
      // the GlyphRunBlob format without changing the wrapping framing.
      // Decoder will warn UnknownTagCode + length-skip during this Phase.
      session->diag->warnings.push_back(
          {ErrorCode::UnknownTagCode,
           "Text-family element encoded as empty body (Phase 8 will land it)", 0, UINT32_MAX});
      break;
    }
  }
  WriteTag(stream, TagCodeForElementType(element.type), &body);
}

std::unique_ptr<VectorElement> ReadVectorElement(::pag::DecodeStream* s, DecodeContext* ctx,
                                                 TagCode code, uint64_t tagEnd) {
  bool ok = false;
  VectorElementType type = ElementTypeForTagCode(code, &ok);
  if (!ok) {
    ctx->warn(ErrorCode::UnknownTagCode, "TagCode is not a VectorElement; skipped");
    return nullptr;
  }

  // Text-family deferrals — warn + skip without producing a VectorElement.
  if (type == VectorElementType::Text || type == VectorElementType::TextPath ||
      type == VectorElementType::TextModifier) {
    ctx->warn(ErrorCode::UnknownTagCode,
              "Text-family VectorElement not yet decoded by Phase 5a; skipped");
    return nullptr;
  }

  uint32_t te = static_cast<uint32_t>(tagEnd);
  auto element = std::make_unique<VectorElement>();
  element->type = type;

  switch (type) {
    case VectorElementType::Rectangle:
      element->payload = ReadElementRectangleBody(s, ctx, te);
      break;
    case VectorElementType::Ellipse:
      element->payload = ReadElementEllipseBody(s, ctx, te);
      break;
    case VectorElementType::Polystar:
      element->payload = ReadElementPolystarBody(s, ctx, te);
      break;
    case VectorElementType::ShapePath:
      element->payload = ReadElementShapePathBody(s, ctx, te);
      break;
    case VectorElementType::FillStyle:
      element->payload = ReadElementFillStyleBody(s, ctx, te);
      break;
    case VectorElementType::StrokeStyle:
      element->payload = ReadElementStrokeStyleBody(s, ctx, te);
      break;
    case VectorElementType::TrimPath:
      element->payload = ReadElementTrimPathBody(s, ctx, te);
      break;
    case VectorElementType::RoundCorner:
      element->payload = ReadElementRoundCornerBody(s, ctx, te);
      break;
    case VectorElementType::MergePath:
      element->payload = ReadElementMergePathBody(s, ctx, te);
      break;
    case VectorElementType::Repeater:
      element->payload = ReadElementRepeaterBody(s, ctx, te);
      break;
    case VectorElementType::VectorGroup:
      element->payload = ReadElementVectorGroupBody(s, ctx, te);
      break;
    case VectorElementType::Text:
    case VectorElementType::TextPath:
    case VectorElementType::TextModifier:
      // Already early-returned above.
      return nullptr;
  }
  if (ctx->hasError()) {
    return nullptr;
  }
  return element;
}

void WriteVectorPayload(::pag::EncodeStream* stream, const VectorPayload& payload,
                        EncodeSession* session) {
  ::pag::EncodeStream body(session->sc);
  body.writeEncodedUint32(static_cast<uint32_t>(payload.contents.size()));
  for (const auto& element : payload.contents) {
    WriteVectorElement(&body, *element, session);
  }
  WriteTag(stream, TagCode::VectorPayload, &body);
}

std::unique_ptr<VectorPayload> ReadVectorPayload(::pag::DecodeStream* s, DecodeContext* ctx,
                                                 uint64_t tagEnd) {
  auto payload = std::make_unique<VectorPayload>();
  auto guard = MakeGuard(ctx);

  uint32_t count = ReadEncodedUint32Safe(s, &guard);
  if (count > limits::MAX_VECTOR_ELEMENTS_PER_PAYLOAD) {
    ctx->error(ErrorCode::StructureLimitExceeded,
               "VectorPayload elementCount exceeds MAX_VECTOR_ELEMENTS_PER_PAYLOAD");
    return nullptr;
  }

  for (uint32_t i = 0; i < count; ++i) {
    if (s->position() >= tagEnd) {
      ctx->error(ErrorCode::TruncatedData, "VectorPayload truncated before all element tags read");
      return nullptr;
    }
    TagHeader header = ReadTagHeader(s);
    uint64_t bodyStart = s->position();
    uint64_t childEnd = 0;
    if (!SafeTagEnd(s, bodyStart, header.length, ctx, &childEnd)) {
      return nullptr;
    }
    if (childEnd > tagEnd) {
      ctx->error(ErrorCode::MalformedTag, "element extends past VectorPayload body");
      return nullptr;
    }
    auto element = ReadVectorElement(s, ctx, header.code, childEnd);
    if (ctx->hasError()) {
      return nullptr;
    }
    if (element != nullptr) {
      payload->contents.push_back(std::move(element));
    }
    SeekTo(s, static_cast<uint32_t>(childEnd));
  }

  return payload;
}

}  // namespace pagx::pag
