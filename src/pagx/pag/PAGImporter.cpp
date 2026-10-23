/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  or required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/PAGImporter.h"
#include <fstream>
#include <vector>
#include "pagx/nodes/ColorStop.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/DiamondGradient.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Gradient.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/pag/PAGAttributeIO.h"
#include "pagx/pag/PAGDataTypes.h"
#include "pagx/pag/PAGFileHeader.h"
#include "pagx/pag/PAGPathIO.h"
#include "pagx/pag/PAGTagHeader.h"
#include "pagx/pag/TagCode.h"
#include "pagx/pag/exporter/ImageProcessor.h"

namespace pagx {
namespace {

struct DecodeContext {
  std::vector<std::string> strings;
  // Zero-copy backing buffer for all decoded image slices: the adopted whole-file data. Slices
  // keep this parent alive (spec §5.1), so no image bytes are ever copied.
  std::shared_ptr<Data> fileData = nullptr;
  // ImageTable entries (spec §6.6); entry data are slices into fileData.
  std::vector<ImageTableEntry> imageEntries = {};
  // ResourceBlock Image nodes in table order; ImagePattern refs resolve against this list.
  std::vector<Image*> resourceImages = {};
  // ResourceBlock Font nodes in table order; GlyphRun refs resolve against this list.
  std::vector<Font*> resourceFonts = {};
  // Image nodes with their pending ImageTable refs; the ImageTable sits at the end of the body,
  // so entry data and decode metadata are backfilled after the whole body is parsed.
  std::vector<std::pair<Image*, uint32_t>> pendingImageRefs = {};
  PAGXDocument* document = nullptr;
  uint16_t tagLevel = 0;

  const std::string& getString(uint32_t index) const {
    static const std::string empty = {};
    if (index >= strings.size()) return empty;
    return strings[index];
  }
};

// Image tag (spec §9.1 §288): a single Fixed varuint — the ImageTable reference (1-based,
// 0 = no data after a missing-image export warning). The entry data and decode metadata are
// backfilled once the ImageTable has been read.
Image* ReadImageTag(pag::DecodeStream* tagStream, DecodeContext* context) {
  auto* image = context->document->makeNode<Image>();
  auto ref = tagStream->readEncodedUint32();
  context->pendingImageRefs.push_back({image, ref});
  return image;
}

// ResourceBlock content (spec §6.3): resource tags in first-reference order, End-terminated.
Font* ReadFontTag(pag::DecodeStream* tagStream, DecodeContext* context);

// Unknown resource tags are skipped (§4.2).
void ReadResourceBlock(pag::DecodeStream* tagStream, DecodeContext* context) {
  while (true) {
    auto header = ReadPAGTagHeader(tagStream);
    if (header.code == TagCode::End) break;
    auto tagBytes = tagStream->readBytes(header.length);
    if (header.code == TagCode::Image) {
      context->resourceImages.push_back(ReadImageTag(&tagBytes, context));
    } else if (header.code == TagCode::Font) {
      context->resourceFonts.push_back(ReadFontTag(&tagBytes, context));
    }
  }
}

// ImageTable content (spec §6.6). contentFileOffset is the table content's offset within the
// adopted file data, so each entry's byte range becomes a zero-copy slice.
void ReadImageTable(pag::DecodeStream* tagStream, size_t contentFileOffset,
                    DecodeContext* context) {
  auto count = tagStream->readEncodedUint32();
  if (count > 65536) {
    PAGThrowError(tagStream->context, "image table count exceeds the spec limit");
    return;
  }
  context->imageEntries.resize(count);
  for (uint32_t i = 0; i < count; i++) {
    auto& entry = context->imageEntries[i];
    entry.mediaType = tagStream->readUint8();
    auto byteLength = tagStream->readEncodedUint32();
    if (byteLength > 256u * 1024 * 1024) {
      PAGThrowError(tagStream->context, "image data length exceeds the spec limit");
      return;
    }
    auto offsetInContent = tagStream->position();
    auto dataOffset = contentFileOffset + offsetInContent;
    if (byteLength > tagStream->bytesAvailable()) {
      PAGThrowError(tagStream->context, "image data extends past the image table");
      return;
    }
    if (entry.mediaType <= 1 && byteLength > 0) {
      entry.data = Data::MakeSlice(context->fileData, dataOffset, byteLength);
    }
    // Unknown mediaType (>1, phase 3): the bytes are consumed but the entry stays empty and
    // referencing images render as empty (§6.6).
    tagStream->readBytes(byteLength);
    entry.logicalWidth = tagStream->readUint32();
    entry.logicalHeight = tagStream->readUint32();
    entry.scaleFactor = tagStream->readFloat();
    entry.offsetX = tagStream->readFloat();
    entry.offsetY = tagStream->readFloat();
  }
}

Rectangle* ReadRectangleTag(pag::DecodeStream* tagStream, DecodeContext* context) {
  auto* rect = context->document->makeNode<Rectangle>();
  PAGAttributeBlockReader reader(tagStream, 6);
  bool hasPosX = reader.flag(0);
  bool hasPosY = reader.flag(1);
  bool hasRoundness = reader.flag(4);
  rect->reversed = reader.flag(5);
  if (hasPosX) rect->position.x = tagStream->readFloat();
  if (hasPosY) rect->position.y = tagStream->readFloat();
  rect->size.width = tagStream->readFloat();
  rect->size.height = tagStream->readFloat();
  if (hasRoundness) rect->roundness = tagStream->readFloat();
  return rect;
}

Ellipse* ReadEllipseTag(pag::DecodeStream* tagStream, DecodeContext* context) {
  auto* ellipse = context->document->makeNode<Ellipse>();
  PAGAttributeBlockReader reader(tagStream, 5);
  bool hasPosX = reader.flag(0);
  bool hasPosY = reader.flag(1);
  ellipse->reversed = reader.flag(4);
  if (hasPosX) ellipse->position.x = tagStream->readFloat();
  if (hasPosY) ellipse->position.y = tagStream->readFloat();
  ellipse->size.width = tagStream->readFloat();
  ellipse->size.height = tagStream->readFloat();
  return ellipse;
}

Path* ReadPathTag(pag::DecodeStream* tagStream, DecodeContext* context) {
  auto* path = context->document->makeNode<Path>();
  PAGAttributeBlockReader reader(tagStream, 4);
  bool hasData = reader.flag(0);
  bool hasPosX = reader.flag(1);
  bool hasPosY = reader.flag(2);
  path->reversed = reader.flag(3);
  if (hasData) {
    auto dataRef = tagStream->readEncodedUint32();
    if (dataRef == 0) {
      auto csHeader = ReadPAGTagHeader(tagStream);
      auto csBytes = tagStream->readBytes(csHeader.length);
      if (csHeader.code == TagCode::PathData) {
        auto* pathData = context->document->makeNode<PathData>();
        ReadPAGPath(&csBytes, pathData);
        path->data = pathData;
      }
    }
  }
  if (hasPosX) path->position.x = tagStream->readFloat();
  if (hasPosY) path->position.y = tagStream->readFloat();
  return path;
}

// Reads the columnar gradient stop tail (spec §9.1): stopCount varuint, one packed
// FloatList of offsets, then byte-aligned colors.
void ReadGradientStops(pag::DecodeStream* tagStream, Gradient* gradient, DecodeContext* context) {
  auto stopCount = tagStream->readEncodedUint32();
  if (stopCount > 1024) {
    // Spec §11 limit; a larger count cannot be trusted to be well-formed input.
    PAGThrowError(tagStream->context, "gradient stop count exceeds the spec limit");
    return;
  }
  std::vector<float> offsets(stopCount, 0.0f);
  tagStream->readFloatList(offsets.data(), stopCount, 0.00002f);
  tagStream->alignWithBytes();
  for (uint32_t i = 0; i < stopCount; i++) {
    auto* stop = context->document->makeNode<ColorStop>();
    stop->offset = offsets[i];
    stop->color = ReadPAGColor(tagStream);
    gradient->colorStops.push_back(stop);
  }
}

// ImagePattern tag (spec §9.1 #295). The image ref is 0 = inline Image sub-tag at the
// value-area end, or k = the ResourceBlock's (k-1)-th entry.
ColorSource* ReadImagePatternTag(pag::DecodeStream* tagStream, DecodeContext* context) {
  auto* pattern = context->document->makeNode<ImagePattern>();
  PAGAttributeBlockReader reader(tagStream, 9);
  bool hasImage = reader.flag(0);
  bool hasTileModeX = reader.flag(1);
  bool hasTileModeY = reader.flag(2);
  bool hasFilterMode = reader.flag(3);
  bool hasMipmapMode = reader.flag(4);
  bool hasMatrix = reader.flag(5);
  bool hasScaleMode = reader.flag(6);
  bool hasDataKeys = reader.flag(7);
  bool hasDataValues = reader.flag(8);
  // Values follow flag order, so the image ref comes first; its inline sub-tag still sits at
  // the value-area end (§9.0) and is read after all values.
  uint32_t imageRef = 0;
  if (hasImage) {
    imageRef = tagStream->readEncodedUint32();
  }
  if (hasTileModeX) pattern->tileModeX = static_cast<TileMode>(tagStream->readUint8());
  if (hasTileModeY) pattern->tileModeY = static_cast<TileMode>(tagStream->readUint8());
  if (hasFilterMode) pattern->filterMode = static_cast<FilterMode>(tagStream->readUint8());
  if (hasMipmapMode) pattern->mipmapMode = static_cast<MipmapMode>(tagStream->readUint8());
  if (hasMatrix) pattern->matrix = ReadPAGMatrix(tagStream);
  if (hasScaleMode) pattern->scaleMode = static_cast<ScaleMode>(tagStream->readUint8());
  std::vector<std::string> keys = {};
  if (hasDataKeys) {
    auto keyCount = tagStream->readEncodedUint32();
    if (keyCount > 65536) {
      PAGThrowError(tagStream->context, "image pattern data key count exceeds the spec limit");
      return pattern;
    }
    keys.resize(keyCount);
    for (uint32_t i = 0; i < keyCount; i++) {
      keys[i] = context->getString(tagStream->readEncodedUint32());
    }
  }
  if (hasDataValues) {
    auto valueCount = tagStream->readEncodedUint32();
    for (uint32_t i = 0; i < valueCount; i++) {
      auto value = context->getString(tagStream->readEncodedUint32());
      if (i < keys.size()) {
        pattern->customData[keys[i]] = value;
      }
    }
  }
  if (hasImage) {
    if (imageRef == 0) {
      auto imageHeader = ReadPAGTagHeader(tagStream);
      auto imageBytes = tagStream->readBytes(imageHeader.length);
      if (imageHeader.code == TagCode::Image) {
        pattern->image = ReadImageTag(&imageBytes, context);
      }
    } else if (imageRef <= context->resourceImages.size()) {
      pattern->image = context->resourceImages[imageRef - 1];
    }
  }
  return pattern;
}

ColorSource* ReadColorSourceTag(pag::DecodeStream* tagStream, TagCode code,
                                DecodeContext* context) {
  switch (code) {
    case TagCode::SolidColor: {
      auto* solid = context->document->makeNode<SolidColor>();
      PAGAttributeBlockReader reader(tagStream, 1);
      if (reader.flag(0)) solid->color = ReadPAGColor(tagStream);
      return solid;
    }
    case TagCode::LinearGradient: {
      auto* grad = context->document->makeNode<LinearGradient>();
      PAGAttributeBlockReader reader(tagStream, 4);
      bool hasStart = reader.flag(0);
      bool hasEnd = reader.flag(1);
      bool hasMatrix = reader.flag(2);
      bool notFitsToGeometry = reader.flag(3);
      if (hasStart) {
        grad->startPoint.x = tagStream->readFloat();
        grad->startPoint.y = tagStream->readFloat();
      }
      if (hasEnd) {
        grad->endPoint.x = tagStream->readFloat();
        grad->endPoint.y = tagStream->readFloat();
      }
      if (hasMatrix) grad->matrix = ReadPAGMatrix(tagStream);
      if (notFitsToGeometry) grad->fitsToGeometry = tagStream->readBoolean();
      ReadGradientStops(tagStream, grad, context);
      return grad;
    }
    case TagCode::RadialGradient: {
      auto* grad = context->document->makeNode<RadialGradient>();
      PAGAttributeBlockReader reader(tagStream, 4);
      bool hasCenter = reader.flag(0);
      bool hasRadius = reader.flag(1);
      bool hasMatrix = reader.flag(2);
      bool notFitsToGeometry = reader.flag(3);
      if (hasCenter) {
        grad->center.x = tagStream->readFloat();
        grad->center.y = tagStream->readFloat();
      }
      if (hasRadius) grad->radius = tagStream->readFloat();
      if (hasMatrix) grad->matrix = ReadPAGMatrix(tagStream);
      if (notFitsToGeometry) grad->fitsToGeometry = tagStream->readBoolean();
      ReadGradientStops(tagStream, grad, context);
      return grad;
    }
    case TagCode::ConicGradient: {
      auto* grad = context->document->makeNode<ConicGradient>();
      PAGAttributeBlockReader reader(tagStream, 5);
      bool hasCenter = reader.flag(0);
      bool hasStart = reader.flag(1);
      bool hasEnd = reader.flag(2);
      bool hasMatrix = reader.flag(3);
      bool notFitsToGeometry = reader.flag(4);
      if (hasCenter) {
        grad->center.x = tagStream->readFloat();
        grad->center.y = tagStream->readFloat();
      }
      if (hasStart) grad->startAngle = tagStream->readFloat();
      if (hasEnd) grad->endAngle = tagStream->readFloat();
      if (hasMatrix) grad->matrix = ReadPAGMatrix(tagStream);
      if (notFitsToGeometry) grad->fitsToGeometry = tagStream->readBoolean();
      ReadGradientStops(tagStream, grad, context);
      return grad;
    }
    case TagCode::DiamondGradient: {
      auto* grad = context->document->makeNode<DiamondGradient>();
      PAGAttributeBlockReader reader(tagStream, 4);
      bool hasCenter = reader.flag(0);
      bool hasRadius = reader.flag(1);
      bool hasMatrix = reader.flag(2);
      bool notFitsToGeometry = reader.flag(3);
      if (hasCenter) {
        grad->center.x = tagStream->readFloat();
        grad->center.y = tagStream->readFloat();
      }
      if (hasRadius) grad->radius = tagStream->readFloat();
      if (hasMatrix) grad->matrix = ReadPAGMatrix(tagStream);
      if (notFitsToGeometry) grad->fitsToGeometry = tagStream->readBoolean();
      ReadGradientStops(tagStream, grad, context);
      return grad;
    }
    case TagCode::ImagePattern:
      return ReadImagePatternTag(tagStream, context);
    default:
      return nullptr;
  }
}

Element* ReadFillTag(pag::DecodeStream* tagStream, DecodeContext* context) {
  auto* fill = context->document->makeNode<Fill>();
  PAGAttributeBlockReader reader(tagStream, 5);
  bool hasColor = reader.flag(0);
  bool hasAlpha = reader.flag(1);
  bool hasBlend = reader.flag(2);
  bool hasFillRule = reader.flag(3);
  bool hasPlacement = reader.flag(4);
  if (hasColor) {
    auto colorRef = tagStream->readEncodedUint32();
    if (colorRef == 0) {
      auto csHeader = ReadPAGTagHeader(tagStream);
      auto csBytes = tagStream->readBytes(csHeader.length);
      fill->color = ReadColorSourceTag(&csBytes, csHeader.code, context);
    }
  }
  if (hasAlpha) fill->alpha = static_cast<float>(tagStream->readUint8()) / 255.0f;
  if (hasBlend) fill->blendMode = static_cast<BlendMode>(tagStream->readUint8());
  if (hasFillRule) fill->fillRule = static_cast<FillRule>(tagStream->readUint8());
  if (hasPlacement) fill->placement = static_cast<LayerPlacement>(tagStream->readUint8());
  return fill;
}

Element* ReadStrokeTag(pag::DecodeStream* tagStream, DecodeContext* context) {
  auto* stroke = context->document->makeNode<Stroke>();
  PAGAttributeBlockReader reader(tagStream, 12);
  bool hasColor = reader.flag(0);
  bool hasWidth = reader.flag(1);
  bool hasAlpha = reader.flag(2);
  bool hasBlend = reader.flag(3);
  bool hasCap = reader.flag(4);
  bool hasJoin = reader.flag(5);
  bool hasMiter = reader.flag(6);
  bool hasDashes = reader.flag(7);
  bool hasDashOffset = reader.flag(8);
  stroke->dashAdaptive = reader.flag(9);
  bool hasAlign = reader.flag(10);
  bool hasPlacement = reader.flag(11);
  if (hasColor) {
    auto colorRef = tagStream->readEncodedUint32();
    if (colorRef == 0) {
      auto csHeader = ReadPAGTagHeader(tagStream);
      auto csBytes = tagStream->readBytes(csHeader.length);
      stroke->color = ReadColorSourceTag(&csBytes, csHeader.code, context);
    }
  }
  if (hasWidth) stroke->width = tagStream->readFloat();
  if (hasAlpha) stroke->alpha = static_cast<float>(tagStream->readUint8()) / 255.0f;
  if (hasBlend) stroke->blendMode = static_cast<BlendMode>(tagStream->readUint8());
  if (hasCap) stroke->cap = static_cast<LineCap>(tagStream->readUint8());
  if (hasJoin) stroke->join = static_cast<LineJoin>(tagStream->readUint8());
  if (hasMiter) stroke->miterLimit = tagStream->readFloat();
  if (hasDashes) {
    auto count = tagStream->readEncodedUint32();
    if (count > 65536) {
      PAGThrowError(tagStream->context, "stroke dash count exceeds the spec limit");
      return stroke;
    }
    stroke->dashes.resize(count);
    for (uint32_t i = 0; i < count; i++) stroke->dashes[i] = tagStream->readFloat();
  }
  if (hasDashOffset) stroke->dashOffset = tagStream->readFloat();
  if (hasAlign) stroke->align = static_cast<StrokeAlign>(tagStream->readUint8());
  if (hasPlacement) stroke->placement = static_cast<LayerPlacement>(tagStream->readUint8());
  return stroke;
}

// Font tag (spec §9.1 #296). Bitmap glyph images record pending refs; the ImageTable sits at
// the end of the body, so entry data is backfilled after the whole file is parsed.
Font* ReadFontTag(pag::DecodeStream* tagStream, DecodeContext* context) {
  auto* font = context->document->makeNode<Font>();
  PAGAttributeBlockReader reader(tagStream, 1);
  bool hasUnitsPerEm = reader.flag(0);
  // Values follow declaration order: unitsPerEm (exist bit) first, glyphCount (Fixed) after.
  if (hasUnitsPerEm) {
    font->unitsPerEm = static_cast<int>(tagStream->readEncodedUint32());
  }
  auto glyphCount = tagStream->readEncodedUint32();
  if (glyphCount > 65535) {
    PAGThrowError(tagStream->context, "font glyph count exceeds the spec limit");
    return font;
  }
  for (uint32_t i = 0; i < glyphCount; i++) {
    auto* glyph = context->document->makeNode<Glyph>();
    auto flags = tagStream->readUint8();
    glyph->advance = tagStream->readFloat();
    glyph->offset.x = tagStream->readFloat();
    glyph->offset.y = tagStream->readFloat();
    if ((flags & 2) != 0) {
      auto* image = context->document->makeNode<Image>();
      auto imageRef = tagStream->readEncodedUint32();
      glyph->image = image;
      context->pendingImageRefs.push_back({image, imageRef});
    }
    if ((flags & 1) != 0) {
      auto* pathData = context->document->makeNode<PathData>();
      if (!ReadPAGPath(tagStream, pathData)) {
        PAGThrowError(tagStream->context, "glyph outline path is malformed");
        return font;
      }
      glyph->path = pathData;
    }
    font->glyphs.push_back(glyph);
  }
  return font;
}

// GlyphRun tag (spec §9.2 #422): 11 exist bits, glyphCount + bit-packed glyph ids (Fixed),
// then the per-glyph columns. The font ref is 0 = inline Font sub-tag at the value-area end,
// or k = the ResourceBlock's (k-1)-th entry.
GlyphRun* ReadGlyphRunTag(pag::DecodeStream* tagStream, DecodeContext* context) {
  auto* run = context->document->makeNode<GlyphRun>();
  PAGAttributeBlockReader reader(tagStream, 11);
  bool hasFont = reader.flag(0);
  bool hasFontSize = reader.flag(1);
  bool hasX = reader.flag(2);
  bool hasY = reader.flag(3);
  bool hasXOffsets = reader.flag(4);
  bool hasPositions = reader.flag(5);
  bool hasAnchors = reader.flag(6);
  bool hasScales = reader.flag(7);
  bool hasRotations = reader.flag(8);
  bool hasSkews = reader.flag(9);
  bool hasBounds = reader.flag(10);
  uint32_t fontRef = 0;
  if (hasFont) {
    fontRef = tagStream->readEncodedUint32();
  }
  if (hasFontSize) {
    run->fontSize = tagStream->readFloat();
  }
  auto glyphCount = tagStream->readEncodedUint32();
  if (glyphCount > 65536) {
    PAGThrowError(tagStream->context, "glyph count exceeds the spec limit");
    return run;
  }
  auto numBits = static_cast<uint8_t>(tagStream->readUBits(5) + 1);
  run->glyphs.resize(glyphCount);
  for (uint32_t i = 0; i < glyphCount; i++) {
    run->glyphs[i] = static_cast<uint16_t>(tagStream->readUBits(numBits));
  }
  tagStream->alignWithBytes();
  if (hasX) {
    run->x = tagStream->readFloat();
  }
  if (hasY) {
    run->y = tagStream->readFloat();
  }
  if (hasXOffsets) {
    run->xOffsets.resize(glyphCount);
    tagStream->readFloatList(run->xOffsets.data(), glyphCount, 0.05f);
    tagStream->alignWithBytes();
  }
  if (hasPositions) {
    std::vector<pag::Point> points(glyphCount);
    tagStream->readPoint2DList(points.data(), glyphCount, 0.05f);
    tagStream->alignWithBytes();
    run->positions.resize(glyphCount);
    for (uint32_t i = 0; i < glyphCount; i++) {
      run->positions[i].x = points[i].x;
      run->positions[i].y = points[i].y;
    }
  }
  if (hasAnchors) {
    std::vector<pag::Point> points(glyphCount);
    tagStream->readPoint2DList(points.data(), glyphCount, 0.05f);
    tagStream->alignWithBytes();
    run->anchors.resize(glyphCount);
    for (uint32_t i = 0; i < glyphCount; i++) {
      run->anchors[i].x = points[i].x;
      run->anchors[i].y = points[i].y;
    }
  }
  if (hasScales) {
    auto count = tagStream->readEncodedUint32();
    if (count > 65536) {
      PAGThrowError(tagStream->context, "glyph scale count exceeds the spec limit");
      return run;
    }
    run->scales.resize(count);
    for (uint32_t i = 0; i < count; i++) {
      run->scales[i].x = tagStream->readFloat();
      run->scales[i].y = tagStream->readFloat();
    }
  }
  if (hasRotations) {
    auto count = tagStream->readEncodedUint32();
    if (count > 65536) {
      PAGThrowError(tagStream->context, "glyph rotation count exceeds the spec limit");
      return run;
    }
    run->rotations.resize(count);
    for (uint32_t i = 0; i < count; i++) {
      run->rotations[i] = tagStream->readFloat();
    }
  }
  if (hasSkews) {
    auto count = tagStream->readEncodedUint32();
    if (count > 65536) {
      PAGThrowError(tagStream->context, "glyph skew count exceeds the spec limit");
      return run;
    }
    run->skews.resize(count);
    for (uint32_t i = 0; i < count; i++) {
      run->skews[i] = tagStream->readFloat();
    }
  }
  if (hasBounds) {
    run->bounds.x = tagStream->readFloat();
    run->bounds.y = tagStream->readFloat();
    run->bounds.width = tagStream->readFloat();
    run->bounds.height = tagStream->readFloat();
  }
  if (hasFont) {
    if (fontRef == 0) {
      auto fontHeader = ReadPAGTagHeader(tagStream);
      auto fontBytes = tagStream->readBytes(fontHeader.length);
      if (fontHeader.code == TagCode::Font) {
        run->font = ReadFontTag(&fontBytes, context);
      }
    } else if (fontRef <= context->resourceFonts.size()) {
      run->font = context->resourceFonts[fontRef - 1];
    }
  }
  return run;
}

Text* ReadTextTag(pag::DecodeStream* tagStream, DecodeContext* context) {
  auto* text = context->document->makeNode<Text>();
  while (true) {
    auto header = ReadPAGTagHeader(tagStream);
    if (header.code == TagCode::End) break;
    auto tagBytes = tagStream->readBytes(header.length);
    if (header.code == TagCode::TextAttributes) {
      PAGAttributeBlockReader reader(&tagBytes, 13);
      bool hasPosX = reader.flag(0);
      bool hasPosY = reader.flag(1);
      bool hasWidth = reader.flag(2);
      bool hasHeight = reader.flag(3);
      bool hasText = reader.flag(4);
      bool hasFamily = reader.flag(5);
      bool hasStyle = reader.flag(6);
      bool hasSize = reader.flag(7);
      bool hasSpacing = reader.flag(8);
      text->fauxBold = reader.flag(9);
      text->fauxItalic = reader.flag(10);
      bool hasAnchor = reader.flag(11);
      bool hasBaseline = reader.flag(12);
      tagBytes.alignWithBytes();
      if (hasPosX) text->position.x = tagBytes.readFloat();
      if (hasPosY) text->position.y = tagBytes.readFloat();
      if (hasWidth) text->width = tagBytes.readFloat();
      if (hasHeight) text->height = tagBytes.readFloat();
      if (hasText) text->text = context->getString(tagBytes.readEncodedUint32());
      if (hasFamily) text->fontFamily = context->getString(tagBytes.readEncodedUint32());
      if (hasStyle) text->fontStyle = context->getString(tagBytes.readEncodedUint32());
      if (hasSize) text->fontSize = tagBytes.readFloat();
      if (hasSpacing) text->letterSpacing = tagBytes.readFloat();
      if (hasAnchor) text->textAnchor = static_cast<TextAnchor>(tagBytes.readUint8());
      if (hasBaseline) text->baseline = static_cast<TextBaseline>(tagBytes.readUint8());
    } else if (header.code == TagCode::GlyphRun) {
      auto* run = ReadGlyphRunTag(&tagBytes, context);
      if (run != nullptr) {
        text->glyphRuns.push_back(run);
      }
    }
  }
  return text;
}

Element* ReadElementTag(pag::DecodeStream* tagStream, TagCode code, DecodeContext* context);

Group* ReadGroupTag(pag::DecodeStream* tagStream, DecodeContext* context) {
  auto* group = context->document->makeNode<Group>();
  while (true) {
    auto header = ReadPAGTagHeader(tagStream);
    if (header.code == TagCode::End) break;
    auto tagBytes = tagStream->readBytes(header.length);
    if (header.code == TagCode::GroupAttributes) {
      PAGAttributeBlockReader reader(&tagBytes, 12);
      bool hasAnchorX = reader.flag(0);
      bool hasAnchorY = reader.flag(1);
      bool hasPosX = reader.flag(2);
      bool hasPosY = reader.flag(3);
      bool hasRotation = reader.flag(4);
      bool hasScaleX = reader.flag(5);
      bool hasScaleY = reader.flag(6);
      bool hasSkew = reader.flag(7);
      bool hasSkewAxis = reader.flag(8);
      bool hasAlpha = reader.flag(9);
      bool hasWidth = reader.flag(10);
      bool hasHeight = reader.flag(11);
      tagBytes.alignWithBytes();
      if (hasAnchorX) group->anchor.x = tagBytes.readFloat();
      if (hasAnchorY) group->anchor.y = tagBytes.readFloat();
      if (hasPosX) group->position.x = tagBytes.readFloat();
      if (hasPosY) group->position.y = tagBytes.readFloat();
      if (hasRotation) group->rotation = tagBytes.readFloat();
      if (hasScaleX) group->scale.x = tagBytes.readFloat();
      if (hasScaleY) group->scale.y = tagBytes.readFloat();
      if (hasSkew) group->skew = tagBytes.readFloat();
      if (hasSkewAxis) group->skewAxis = tagBytes.readFloat();
      if (hasAlpha) group->alpha = static_cast<float>(tagBytes.readUint8()) / 255.0f;
      if (hasWidth) group->width = tagBytes.readFloat();
      if (hasHeight) group->height = tagBytes.readFloat();
    } else {
      auto* element = ReadElementTag(&tagBytes, header.code, context);
      if (element != nullptr) group->elements.push_back(element);
    }
  }
  return group;
}

Element* ReadElementTag(pag::DecodeStream* tagStream, TagCode code, DecodeContext* context) {
  switch (code) {
    case TagCode::Rectangle:
      return ReadRectangleTag(tagStream, context);
    case TagCode::Ellipse:
      return ReadEllipseTag(tagStream, context);
    case TagCode::Path:
      return ReadPathTag(tagStream, context);
    case TagCode::Fill:
      return ReadFillTag(tagStream, context);
    case TagCode::Stroke:
      return ReadStrokeTag(tagStream, context);
    case TagCode::TextBlock:
      return ReadTextTag(tagStream, context);
    case TagCode::GroupBlock:
      return ReadGroupTag(tagStream, context);
    default:
      return nullptr;
  }
}

void ReadLayerBlockContent(pag::DecodeStream* tagStream, Layer* layer, DecodeContext* context) {
  while (true) {
    auto header = ReadPAGTagHeader(tagStream);
    if (header.code == TagCode::End) break;
    auto tagBytes = tagStream->readBytes(header.length);
    if (header.code == TagCode::LayerAttributes) {
      PAGAttributeBlockReader reader(&tagBytes, 20);
      bool hasName = reader.flag(0);
      bool hasX = reader.flag(1);
      bool hasY = reader.flag(2);
      bool notVisible = reader.flag(3);
      bool hasAlpha = reader.flag(4);
      bool hasBlend = reader.flag(5);
      bool hasMatrix = reader.flag(6);
      bool hasMatrix3D = reader.flag(7);
      layer->preserve3D = reader.flag(8);  // BitFlag (default false): the bit is the value.
      bool notAntiAlias = reader.flag(9);
      bool notGroupOpacity = reader.flag(10);
      bool notPassThrough = reader.flag(11);
      bool hasWidth = reader.flag(12);
      bool hasHeight = reader.flag(13);
      bool hasScrollRect = reader.flag(14);
      bool hasMask = reader.flag(15);
      bool hasMaskType = reader.flag(16);
      // flags 17/18 (hasComposition/hasSubDocument) are phase-2 features, ignored here.
      bool hasVmContext = reader.flag(19);
      tagBytes.alignWithBytes();
      if (hasName) layer->name = context->getString(tagBytes.readEncodedUint32());
      if (hasX) layer->x = tagBytes.readFloat();
      if (hasY) layer->y = tagBytes.readFloat();
      if (notVisible) layer->visible = tagBytes.readBoolean();
      if (hasAlpha) layer->alpha = static_cast<float>(tagBytes.readUint8()) / 255.0f;
      if (hasBlend) layer->blendMode = static_cast<BlendMode>(tagBytes.readUint8());
      if (hasMatrix) layer->matrix = ReadPAGMatrix(&tagBytes);
      if (hasMatrix3D) layer->matrix3D = ReadPAGMatrix3D(&tagBytes);
      if (notAntiAlias) layer->antiAlias = tagBytes.readBoolean();
      if (notGroupOpacity) layer->groupOpacity = tagBytes.readBoolean();
      if (notPassThrough) layer->passThroughBackground = tagBytes.readBoolean();
      if (hasWidth) layer->width = tagBytes.readFloat();
      if (hasHeight) layer->height = tagBytes.readFloat();
      if (hasScrollRect) {
        layer->scrollRect.x = tagBytes.readFloat();
        layer->scrollRect.y = tagBytes.readFloat();
        layer->scrollRect.width = tagBytes.readFloat();
        layer->scrollRect.height = tagBytes.readFloat();
        layer->hasScrollRect = true;
      }
      if (hasMask) tagBytes.readEncodedUint32();  // NodeRef, phase 1 always none
      if (hasMaskType) layer->maskType = static_cast<MaskType>(tagBytes.readUint8());
      if (hasVmContext) layer->vmContext = context->getString(tagBytes.readEncodedUint32());
    } else if (header.code == TagCode::LayerBlock) {
      auto* child = context->document->makeNode<Layer>();
      ReadLayerBlockContent(&tagBytes, child, context);
      layer->children.push_back(child);
    } else {
      auto* element = ReadElementTag(&tagBytes, header.code, context);
      if (element != nullptr) layer->contents.push_back(element);
    }
  }
}

void ReadDocumentBlock(pag::DecodeStream* tagStream, PAGXDocument* doc, DecodeContext* context) {
  while (true) {
    auto header = ReadPAGTagHeader(tagStream);
    if (header.code == TagCode::End) break;
    auto tagBytes = tagStream->readBytes(header.length);
    if (header.code == TagCode::DocumentAttributes) {
      PAGAttributeBlockReader reader(&tagBytes, 4);
      bool hasWidth = reader.flag(0);
      bool hasHeight = reader.flag(1);
      tagBytes.alignWithBytes();
      if (hasWidth) doc->width = tagBytes.readFloat();
      if (hasHeight) doc->height = tagBytes.readFloat();
    } else if (header.code == TagCode::RetainedIdTable) {
      auto count = tagBytes.readEncodedUint32();
      for (uint32_t i = 0; i < count; i++) {
        tagBytes.readEncodedUint32();
        tagBytes.readEncodedUint32();
      }
    } else if (header.code == TagCode::LayerBlock) {
      auto* layer = context->document->makeNode<Layer>();
      ReadLayerBlockContent(&tagBytes, layer, context);
      doc->layers.push_back(layer);
    }
  }
}

}  // namespace
static PAGFormat DetectPAGFormatBytes(const uint8_t* bytes, size_t length) {
  PAGFileHeader header = {};
  if (header.parse(bytes, length)) {
    if (header.version <= 3) {
      return PAGFormat::PAGLegacy;
    }
    if (header.version == 4) {
      return PAGFormat::PAGv4;
    }
    return PAGFormat::Unknown;
  }
  size_t offset = 0;
  if (length >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
    offset = 3;
  }
  while (offset < length) {
    auto c = bytes[offset];
    if (c == '<') {
      return PAGFormat::PAGX;
    }
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
      break;
    }
    offset++;
  }
  return PAGFormat::Unknown;
}

PAGFormat PAGImporter::DetectFormat(const void* bytes, size_t length) {
  if (bytes == nullptr || length == 0) {
    return PAGFormat::Unknown;
  }
  return DetectPAGFormatBytes(static_cast<const uint8_t*>(bytes), length);
}

PAGFormat PAGImporter::DetectFormat(const std::string& filePath) {
  std::ifstream in(filePath, std::ios::binary);
  uint8_t header[16] = {};
  in.read(reinterpret_cast<char*>(header), sizeof(header));
  auto length = static_cast<size_t>(in.gcount());
  return DetectPAGFormatBytes(header, length);
}

std::shared_ptr<PAGXDocument> PAGImporter::LoadFromPAG(const std::string& filePath,
                                                       PAGImportResult* result) {
  if (result != nullptr) result->format = PAGFormat::PAGv4;
  std::ifstream in(filePath, std::ios::binary | std::ios::ate);
  if (!in.is_open()) {
    if (result != nullptr) result->error = "cannot open file";
    return nullptr;
  }
  auto fileSize = static_cast<size_t>(in.tellg());
  if (fileSize < 16) {
    if (result != nullptr) result->error = "file too short";
    return nullptr;
  }
  in.seekg(0);
  auto buffer = std::make_unique<uint8_t[]>(fileSize);
  in.read(reinterpret_cast<char*>(buffer.get()), static_cast<std::streamsize>(fileSize));
  in.close();
  // The buffer is adopted by a Data so decoded image slices can reference the raw file bytes
  // without copying (spec §5.1 zero-copy image references).
  auto fileData = Data::MakeAdopt(buffer.release(), fileSize);

  PAGFileHeader header = {};
  if (!header.parse(fileData->bytes(), fileSize) || header.version != 4) {
    if (result != nullptr) result->error = "invalid PAG v4 header";
    return nullptr;
  }
  if (16 + header.bodyLength > fileSize) {
    if (result != nullptr) result->error = "body length exceeds file size";
    return nullptr;
  }

  auto doc = PAGXDocument::Make(0, 0);
  DecodeContext context = {};
  context.document = doc.get();
  context.fileData = fileData;

  pag::StreamContext streamContext;
  pag::DecodeStream bodyStream(&streamContext, fileData->bytes() + 16, header.bodyLength);

  while (true) {
    auto tagHeader = ReadPAGTagHeader(&bodyStream);
    if (static_cast<uint16_t>(tagHeader.code) > context.tagLevel) {
      context.tagLevel = static_cast<uint16_t>(tagHeader.code);
    }
    if (tagHeader.code == TagCode::End) break;
    if (bodyStream.context->hasException()) break;
    auto contentStartInBody = bodyStream.position();
    auto tagBytes = bodyStream.readBytes(tagHeader.length);
    switch (tagHeader.code) {
      case TagCode::StringTable:
        ReadPAGStringTable(&tagBytes, &context.strings);
        break;
      case TagCode::ResourceBlock:
        ReadResourceBlock(&tagBytes, &context);
        break;
      case TagCode::DocumentBlock:
        ReadDocumentBlock(&tagBytes, doc.get(), &context);
        break;
      case TagCode::ImageTable:
        // Entry byte ranges live inside the adopted file data at 16 + contentStartInBody +
        // in-table offset, which is what MakeSlice needs for zero-copy references.
        ReadImageTable(&tagBytes, 16 + contentStartInBody, &context);
        break;
      default:
        break;
    }
    if (bodyStream.context->hasException()) break;
  }

  // Backfill image refs: the ImageTable sits after the document in the body, so Image nodes
  // only recorded their refs during decoding (spec §5.1 unified backfill pass).
  for (auto& pending : context.pendingImageRefs) {
    if (pending.second == 0 || pending.second > context.imageEntries.size()) {
      continue;
    }
    auto& entry = context.imageEntries[pending.second - 1];
    pending.first->data = entry.data;
    pending.first->logicalWidth = entry.logicalWidth;
    pending.first->logicalHeight = entry.logicalHeight;
    pending.first->scaleFactor = entry.scaleFactor;
    pending.first->offsetX = entry.offsetX;
    pending.first->offsetY = entry.offsetY;
  }

  if (bodyStream.context->hasException()) {
    // Failed decodes return nullptr (interface draft §2): handing a partially decoded document
    // to PAGScene::Make would render a malformed tree, so the doc never escapes on failure.
    if (result != nullptr) {
      result->success = false;
      result->tagLevel = context.tagLevel;
      result->error = bodyStream.context->errorMessages.empty()
                          ? "decode stream error"
                          : bodyStream.context->errorMessages.front();
    }
    return nullptr;
  }
  if (result != nullptr) {
    result->success = true;
    result->tagLevel = context.tagLevel;
  }
  return doc;
}

}  // namespace pagx
