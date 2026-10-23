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
#include <unordered_set>
#include <utility>
#include <vector>
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/nodes/BackgroundBlurStyle.h"
#include "pagx/nodes/BlendFilter.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/ColorMatrixFilter.h"
#include "pagx/nodes/ColorStop.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/DiamondGradient.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlassStyle.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Gradient.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/InnerShadowFilter.h"
#include "pagx/nodes/InnerShadowStyle.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/MergePath.h"
#include "pagx/nodes/NoiseFilter.h"
#include "pagx/nodes/NoiseStyle.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/RoundCorner.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/nodes/TextPath.h"
#include "pagx/nodes/TrimPath.h"
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
  // ResourceBlock Compositions in table order; layer composition refs resolve against this list.
  std::vector<Composition*> resourceCompositions = {};
  // Layer composition refs (inline handled during decode); resource refs backfill after the
  // ResourceBlock is read.
  std::vector<std::pair<Layer*, uint32_t>> pendingCompositionRefs = {};
  // Image nodes with their pending ImageTable refs; the ImageTable sits at the end of the body,
  // so entry data and decode metadata are backfilled after the whole body is parsed.
  std::vector<std::pair<Image*, uint32_t>> pendingImageRefs = {};
  // SubDocumentTable entries (spec §6.5), decoded once; every referencing layer then gets an
  // independent instance built from its template.
  struct SubDocumentEntry {
    std::string name = {};
    std::shared_ptr<PAGXDocument> document = nullptr;
    // Template-level pending refs: layer indices within the template's top-level layers that
    // reference other table entries.
    std::vector<std::pair<size_t, uint32_t>> pendingRefs = {};
  };
  std::vector<SubDocumentEntry> subDocuments = {};
  // Root layers referencing sub-documents: attached after the table (later in the body) decodes.
  std::vector<std::pair<Layer*, uint32_t>> pendingSubDocRefs = {};
  // Layer mask NodeRefs: backfilled after the whole body decodes, when the node table
  // (creation order = emission order, spec §5.1) is complete.
  std::vector<std::pair<Layer*, uint32_t>> pendingMaskRefs = {};
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

Composition* ReadCompositionBlock(pag::DecodeStream* tagStream, DecodeContext* context);
LayerStyle* ReadLayerStyleTag(pag::DecodeStream* tagStream, TagCode code, DecodeContext* context);
void ReadLayerBlockContent(pag::DecodeStream* tagStream, Layer* layer, DecodeContext* context,
                           std::vector<std::pair<Layer*, uint32_t>>* subDocRefs,
                           std::vector<std::pair<Layer*, uint32_t>>* pendingMaskRefsTarget);

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
    } else if (header.code == TagCode::CompositionBlock) {
      context->resourceCompositions.push_back(ReadCompositionBlock(&tagBytes, context));
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
  uint32_t dataRef = 0;
  if (hasData) {
    dataRef = tagStream->readEncodedUint32();
  }
  if (hasPosX) path->position.x = tagStream->readFloat();
  if (hasPosY) path->position.y = tagStream->readFloat();
  // The inline PathData sub-tag sits at the value-area END (spec §9.0).
  if (hasData && dataRef == 0) {
    auto dataHeader = ReadPAGTagHeader(tagStream);
    auto dataBytes = tagStream->readBytes(dataHeader.length);
    if (dataHeader.code == TagCode::PathData) {
      auto* pathData = context->document->makeNode<PathData>();
      ReadPAGPath(&dataBytes, pathData);
      path->data = pathData;
    }
  }
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

Element* ReadPolystarTag(pag::DecodeStream* tagStream, DecodeContext* context) {
  auto* polystar = context->document->makeNode<Polystar>();
  PAGAttributeBlockReader reader(tagStream, 10);
  bool hasPosX = reader.flag(0);
  bool hasPosY = reader.flag(1);
  bool hasType = reader.flag(2);
  bool hasPointCount = reader.flag(3);
  bool hasOuterRadius = reader.flag(4);
  bool hasInnerRadius = reader.flag(5);
  bool hasRotation = reader.flag(6);
  bool hasOuterRoundness = reader.flag(7);
  bool hasInnerRoundness = reader.flag(8);
  polystar->reversed = reader.flag(9);
  if (hasPosX) polystar->position.x = tagStream->readFloat();
  if (hasPosY) polystar->position.y = tagStream->readFloat();
  if (hasType) polystar->type = static_cast<PolystarType>(tagStream->readUint8());
  if (hasPointCount) polystar->pointCount = tagStream->readFloat();
  if (hasOuterRadius) polystar->outerRadius = tagStream->readFloat();
  if (hasInnerRadius) polystar->innerRadius = tagStream->readFloat();
  if (hasRotation) polystar->rotation = tagStream->readFloat();
  if (hasOuterRoundness) polystar->outerRoundness = tagStream->readFloat();
  if (hasInnerRoundness) polystar->innerRoundness = tagStream->readFloat();
  return polystar;
}

Element* ReadTrimPathTag(pag::DecodeStream* tagStream, DecodeContext* context) {
  auto* trim = context->document->makeNode<TrimPath>();
  PAGAttributeBlockReader reader(tagStream, 4);
  bool hasStart = reader.flag(0);
  bool hasEnd = reader.flag(1);
  bool hasOffset = reader.flag(2);
  bool hasType = reader.flag(3);
  if (hasStart) trim->start = tagStream->readFloat();
  if (hasEnd) trim->end = tagStream->readFloat();
  if (hasOffset) trim->offset = tagStream->readFloat();
  if (hasType) trim->type = static_cast<TrimType>(tagStream->readUint8());
  return trim;
}

Element* ReadRoundCornerTag(pag::DecodeStream* tagStream, DecodeContext* context) {
  auto* round = context->document->makeNode<RoundCorner>();
  PAGAttributeBlockReader reader(tagStream, 1);
  if (reader.flag(0)) round->radius = tagStream->readFloat();
  return round;
}

Element* ReadMergePathTag(pag::DecodeStream* tagStream, DecodeContext* context) {
  auto* merge = context->document->makeNode<MergePath>();
  PAGAttributeBlockReader reader(tagStream, 1);
  if (reader.flag(0)) merge->mode = static_cast<MergePathMode>(tagStream->readUint8());
  return merge;
}

Element* ReadTextPathTag(pag::DecodeStream* tagStream, DecodeContext* context) {
  auto* textPath = context->document->makeNode<TextPath>();
  PAGAttributeBlockReader reader(tagStream, 9);
  bool hasPath = reader.flag(0);
  bool hasOriginX = reader.flag(1);
  bool hasOriginY = reader.flag(2);
  bool hasAngle = reader.flag(3);
  bool hasFirstMargin = reader.flag(4);
  bool hasLastMargin = reader.flag(5);
  bool notPerpendicular = reader.flag(6);
  textPath->reversed = reader.flag(7);
  textPath->forceAlignment = reader.flag(8);
  uint32_t pathRef = 0;
  if (hasPath) {
    pathRef = tagStream->readEncodedUint32();
  }
  if (hasOriginX) textPath->baselineOrigin.x = tagStream->readFloat();
  if (hasOriginY) textPath->baselineOrigin.y = tagStream->readFloat();
  if (hasAngle) textPath->baselineAngle = tagStream->readFloat();
  if (hasFirstMargin) textPath->firstMargin = tagStream->readFloat();
  if (hasLastMargin) textPath->lastMargin = tagStream->readFloat();
  if (notPerpendicular) textPath->perpendicular = tagStream->readBoolean();
  if (hasPath && pathRef == 0) {
    auto dataHeader = ReadPAGTagHeader(tagStream);
    auto dataBytes = tagStream->readBytes(dataHeader.length);
    if (dataHeader.code == TagCode::PathData) {
      auto* pathData = context->document->makeNode<PathData>();
      ReadPAGPath(&dataBytes, pathData);
      textPath->path = pathData;
    }
  }
  return textPath;
}

// CompositionBlock (spec §9.1 #297): CompositionAttributes (298) + layers + End.
Composition* ReadCompositionBlock(pag::DecodeStream* tagStream, DecodeContext* context) {
  auto* composition = context->document->makeNode<Composition>();
  while (true) {
    auto header = ReadPAGTagHeader(tagStream);
    if (header.code == TagCode::End) break;
    auto tagBytes = tagStream->readBytes(header.length);
    if (header.code == TagCode::DocumentAttributes) {
      PAGAttributeBlockReader reader(&tagBytes, 4);
      bool hasWidth = reader.flag(0);
      bool hasHeight = reader.flag(1);
      tagBytes.alignWithBytes();
      if (hasWidth) composition->width = tagBytes.readFloat();
      if (hasHeight) composition->height = tagBytes.readFloat();
    } else if (header.code == TagCode::LayerBlock) {
      auto* layer = context->document->makeNode<Layer>();
      ReadLayerBlockContent(&tagBytes, layer, context, &context->pendingSubDocRefs,
                            &context->pendingMaskRefs);
      composition->layers.push_back(layer);
    }
  }
  return composition;
}

LayerStyle* ReadLayerStyleTag(pag::DecodeStream* tagStream, TagCode code, DecodeContext* context) {
  switch (code) {
    case TagCode::DropShadowStyle: {
      auto* style = context->document->makeNode<DropShadowStyle>();
      PAGAttributeBlockReader reader(tagStream, 8);
      bool hasBlend = reader.flag(0);
      style->excludeChildEffects = reader.flag(1);
      bool hasOffsetX = reader.flag(2);
      bool hasOffsetY = reader.flag(3);
      bool hasBlurX = reader.flag(4);
      bool hasBlurY = reader.flag(5);
      bool hasColor = reader.flag(6);
      bool notShowBehind = reader.flag(7);
      tagStream->alignWithBytes();
      if (hasBlend) style->blendMode = static_cast<BlendMode>(tagStream->readUint8());
      if (hasOffsetX) style->offsetX = tagStream->readFloat();
      if (hasOffsetY) style->offsetY = tagStream->readFloat();
      if (hasBlurX) style->blurX = tagStream->readFloat();
      if (hasBlurY) style->blurY = tagStream->readFloat();
      if (hasColor) style->color = ReadPAGColor(tagStream);
      if (notShowBehind) style->showBehindLayer = tagStream->readBoolean();
      return style;
    }
    case TagCode::InnerShadowStyle: {
      auto* style = context->document->makeNode<InnerShadowStyle>();
      PAGAttributeBlockReader reader(tagStream, 7);
      bool hasBlend = reader.flag(0);
      style->excludeChildEffects = reader.flag(1);
      bool hasOffsetX = reader.flag(2);
      bool hasOffsetY = reader.flag(3);
      bool hasBlurX = reader.flag(4);
      bool hasBlurY = reader.flag(5);
      bool hasColor = reader.flag(6);
      tagStream->alignWithBytes();
      if (hasBlend) style->blendMode = static_cast<BlendMode>(tagStream->readUint8());
      if (hasOffsetX) style->offsetX = tagStream->readFloat();
      if (hasOffsetY) style->offsetY = tagStream->readFloat();
      if (hasBlurX) style->blurX = tagStream->readFloat();
      if (hasBlurY) style->blurY = tagStream->readFloat();
      if (hasColor) style->color = ReadPAGColor(tagStream);
      return style;
    }
    case TagCode::BackgroundBlurStyle: {
      auto* style = context->document->makeNode<BackgroundBlurStyle>();
      PAGAttributeBlockReader reader(tagStream, 5);
      bool hasBlend = reader.flag(0);
      style->excludeChildEffects = reader.flag(1);
      bool hasBlurX = reader.flag(2);
      bool hasBlurY = reader.flag(3);
      bool hasTileMode = reader.flag(4);
      tagStream->alignWithBytes();
      if (hasBlend) style->blendMode = static_cast<BlendMode>(tagStream->readUint8());
      if (hasBlurX) style->blurX = tagStream->readFloat();
      if (hasBlurY) style->blurY = tagStream->readFloat();
      if (hasTileMode) style->tileMode = static_cast<TileMode>(tagStream->readUint8());
      return style;
    }
    case TagCode::NoiseStyle: {
      auto* style = context->document->makeNode<NoiseStyle>();
      PAGAttributeBlockReader reader(tagStream, 10);
      bool hasBlend = reader.flag(0);
      style->excludeChildEffects = reader.flag(1);
      bool hasMode = reader.flag(2);
      bool hasSize = reader.flag(3);
      bool hasDensity = reader.flag(4);
      bool hasSeed = reader.flag(5);
      bool hasColor = reader.flag(6);
      bool hasFirstColor = reader.flag(7);
      bool hasSecondColor = reader.flag(8);
      bool hasOpacity = reader.flag(9);
      tagStream->alignWithBytes();
      if (hasBlend) style->blendMode = static_cast<BlendMode>(tagStream->readUint8());
      if (hasMode) style->mode = static_cast<NoiseMode>(tagStream->readUint8());
      if (hasSize) style->size = tagStream->readFloat();
      if (hasDensity) style->density = tagStream->readFloat();
      if (hasSeed) style->seed = tagStream->readFloat();
      if (hasColor) style->color = ReadPAGColor(tagStream);
      if (hasFirstColor) style->firstColor = ReadPAGColor(tagStream);
      if (hasSecondColor) style->secondColor = ReadPAGColor(tagStream);
      if (hasOpacity) {
        style->opacity = static_cast<float>(tagStream->readUint8()) / 255.0f;
      }
      return style;
    }
    case TagCode::GlassStyle: {
      auto* style = context->document->makeNode<GlassStyle>();
      PAGAttributeBlockReader reader(tagStream, 9);
      bool hasBlend = reader.flag(0);
      style->excludeChildEffects = reader.flag(1);
      bool hasRefraction = reader.flag(2);
      bool hasDepth = reader.flag(3);
      bool hasFrost = reader.flag(4);
      bool hasDispersion = reader.flag(5);
      bool hasSplay = reader.flag(6);
      bool hasLightAngle = reader.flag(7);
      bool hasLightIntensity = reader.flag(8);
      tagStream->alignWithBytes();
      if (hasBlend) style->blendMode = static_cast<BlendMode>(tagStream->readUint8());
      if (hasRefraction) style->refraction = tagStream->readFloat();
      if (hasDepth) style->depth = tagStream->readFloat();
      if (hasFrost) style->frost = tagStream->readFloat();
      if (hasDispersion) style->dispersion = tagStream->readFloat();
      if (hasSplay) style->splay = tagStream->readFloat();
      if (hasLightAngle) style->lightAngle = tagStream->readFloat();
      if (hasLightIntensity) style->lightIntensity = tagStream->readFloat();
      return style;
    }
    default:
      return nullptr;
  }
}

LayerFilter* ReadLayerFilterTag(pag::DecodeStream* tagStream, TagCode code,
                                DecodeContext* context) {
  switch (code) {
    case TagCode::BlurFilter: {
      auto* filter = context->document->makeNode<BlurFilter>();
      PAGAttributeBlockReader reader(tagStream, 3);
      bool hasBlurX = reader.flag(0);
      bool hasBlurY = reader.flag(1);
      bool hasTileMode = reader.flag(2);
      tagStream->alignWithBytes();
      if (hasBlurX) filter->blurX = tagStream->readFloat();
      if (hasBlurY) filter->blurY = tagStream->readFloat();
      if (hasTileMode) filter->tileMode = static_cast<TileMode>(tagStream->readUint8());
      return filter;
    }
    case TagCode::DropShadowFilter: {
      auto* filter = context->document->makeNode<DropShadowFilter>();
      PAGAttributeBlockReader reader(tagStream, 6);
      bool hasOffsetX = reader.flag(0);
      bool hasOffsetY = reader.flag(1);
      bool hasBlurX = reader.flag(2);
      bool hasBlurY = reader.flag(3);
      bool hasColor = reader.flag(4);
      filter->shadowOnly = reader.flag(5);
      tagStream->alignWithBytes();
      if (hasOffsetX) filter->offsetX = tagStream->readFloat();
      if (hasOffsetY) filter->offsetY = tagStream->readFloat();
      if (hasBlurX) filter->blurX = tagStream->readFloat();
      if (hasBlurY) filter->blurY = tagStream->readFloat();
      if (hasColor) filter->color = ReadPAGColor(tagStream);
      return filter;
    }
    case TagCode::InnerShadowFilter: {
      auto* filter = context->document->makeNode<InnerShadowFilter>();
      PAGAttributeBlockReader reader(tagStream, 6);
      bool hasOffsetX = reader.flag(0);
      bool hasOffsetY = reader.flag(1);
      bool hasBlurX = reader.flag(2);
      bool hasBlurY = reader.flag(3);
      bool hasColor = reader.flag(4);
      filter->shadowOnly = reader.flag(5);
      tagStream->alignWithBytes();
      if (hasOffsetX) filter->offsetX = tagStream->readFloat();
      if (hasOffsetY) filter->offsetY = tagStream->readFloat();
      if (hasBlurX) filter->blurX = tagStream->readFloat();
      if (hasBlurY) filter->blurY = tagStream->readFloat();
      if (hasColor) filter->color = ReadPAGColor(tagStream);
      return filter;
    }
    case TagCode::BlendFilter: {
      auto* filter = context->document->makeNode<BlendFilter>();
      PAGAttributeBlockReader reader(tagStream, 2);
      bool hasColor = reader.flag(0);
      bool hasBlend = reader.flag(1);
      tagStream->alignWithBytes();
      if (hasColor) filter->color = ReadPAGColor(tagStream);
      if (hasBlend) filter->blendMode = static_cast<BlendMode>(tagStream->readUint8());
      return filter;
    }
    case TagCode::ColorMatrixFilter: {
      auto* filter = context->document->makeNode<ColorMatrixFilter>();
      PAGAttributeBlockReader reader(tagStream, 1);
      bool hasMatrix = reader.flag(0);
      tagStream->alignWithBytes();
      if (hasMatrix) {
        for (int i = 0; i < 20; i++) {
          filter->matrix[i] = tagStream->readFloat();
        }
      }
      return filter;
    }
    case TagCode::NoiseFilter: {
      auto* filter = context->document->makeNode<NoiseFilter>();
      PAGAttributeBlockReader reader(tagStream, 9);
      bool hasMode = reader.flag(0);
      bool hasSize = reader.flag(1);
      bool hasDensity = reader.flag(2);
      bool hasSeed = reader.flag(3);
      bool hasBlend = reader.flag(4);
      bool hasColor = reader.flag(5);
      bool hasFirstColor = reader.flag(6);
      bool hasSecondColor = reader.flag(7);
      bool hasOpacity = reader.flag(8);
      tagStream->alignWithBytes();
      if (hasMode) filter->mode = static_cast<NoiseMode>(tagStream->readUint8());
      if (hasSize) filter->size = tagStream->readFloat();
      if (hasDensity) filter->density = tagStream->readFloat();
      if (hasSeed) filter->seed = tagStream->readFloat();
      if (hasBlend) filter->blendMode = static_cast<BlendMode>(tagStream->readUint8());
      if (hasColor) filter->color = ReadPAGColor(tagStream);
      if (hasFirstColor) filter->firstColor = ReadPAGColor(tagStream);
      if (hasSecondColor) filter->secondColor = ReadPAGColor(tagStream);
      if (hasOpacity) {
        filter->opacity = static_cast<float>(tagStream->readUint8()) / 255.0f;
      }
      return filter;
    }
    default:
      return nullptr;
  }
}

Element* ReadElementTag(pag::DecodeStream* tagStream, TagCode code, DecodeContext* context);

// TextBoxBlock tag (spec §9.2 #431/#432): 19 exist bits, then child elements, End.
Element* ReadTextBoxTag(pag::DecodeStream* tagStream, DecodeContext* context) {
  auto* textBox = context->document->makeNode<TextBox>();
  while (true) {
    auto header = ReadPAGTagHeader(tagStream);
    if (header.code == TagCode::End) break;
    auto tagBytes = tagStream->readBytes(header.length);
    if (header.code == TagCode::TextBoxAttributes) {
      PAGAttributeBlockReader reader(&tagBytes, 19);
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
      bool hasPadding = reader.flag(12);
      bool hasTextAlign = reader.flag(13);
      bool hasParagraphAlign = reader.flag(14);
      bool hasWritingMode = reader.flag(15);
      bool hasLineHeight = reader.flag(16);
      bool notWordWrap = reader.flag(17);
      bool hasOverflow = reader.flag(18);
      tagBytes.alignWithBytes();
      if (hasAnchorX) textBox->anchor.x = tagBytes.readFloat();
      if (hasAnchorY) textBox->anchor.y = tagBytes.readFloat();
      if (hasPosX) textBox->position.x = tagBytes.readFloat();
      if (hasPosY) textBox->position.y = tagBytes.readFloat();
      if (hasRotation) textBox->rotation = tagBytes.readFloat();
      if (hasScaleX) textBox->scale.x = tagBytes.readFloat();
      if (hasScaleY) textBox->scale.y = tagBytes.readFloat();
      if (hasSkew) textBox->skew = tagBytes.readFloat();
      if (hasSkewAxis) textBox->skewAxis = tagBytes.readFloat();
      if (hasAlpha) {
        textBox->alpha = static_cast<float>(tagBytes.readUint8()) / 255.0f;
      }
      if (hasWidth) textBox->width = tagBytes.readFloat();
      if (hasHeight) textBox->height = tagBytes.readFloat();
      if (hasPadding) {
        textBox->padding.top = tagBytes.readFloat();
        textBox->padding.right = tagBytes.readFloat();
        textBox->padding.bottom = tagBytes.readFloat();
        textBox->padding.left = tagBytes.readFloat();
      }
      if (hasTextAlign) {
        textBox->textAlign = static_cast<TextAlign>(tagBytes.readUint8());
      }
      if (hasParagraphAlign) {
        textBox->paragraphAlign = static_cast<ParagraphAlign>(tagBytes.readUint8());
      }
      if (hasWritingMode) {
        textBox->writingMode = static_cast<WritingMode>(tagBytes.readUint8());
      }
      if (hasLineHeight) textBox->lineHeight = tagBytes.readFloat();
      if (notWordWrap) textBox->wordWrap = tagBytes.readBoolean();
      if (hasOverflow) {
        textBox->overflow = static_cast<Overflow>(tagBytes.readUint8());
      }
    } else {
      auto* element = ReadElementTag(&tagBytes, header.code, context);
      if (element != nullptr) {
        textBox->elements.push_back(element);
      }
    }
  }
  return textBox;
}

Element* ReadRepeaterTag(pag::DecodeStream* tagStream, DecodeContext* context) {
  auto* repeater = context->document->makeNode<Repeater>();
  PAGAttributeBlockReader reader(tagStream, 12);
  bool hasCopies = reader.flag(0);
  bool hasOffset = reader.flag(1);
  bool hasOrder = reader.flag(2);
  bool hasAnchorX = reader.flag(3);
  bool hasAnchorY = reader.flag(4);
  bool hasPosX = reader.flag(5);
  bool hasPosY = reader.flag(6);
  bool hasRotation = reader.flag(7);
  bool hasScaleX = reader.flag(8);
  bool hasScaleY = reader.flag(9);
  bool hasStartAlpha = reader.flag(10);
  bool hasEndAlpha = reader.flag(11);
  tagStream->alignWithBytes();
  if (hasCopies) repeater->copies = tagStream->readFloat();
  if (hasOffset) repeater->offset = tagStream->readFloat();
  if (hasOrder) repeater->order = static_cast<RepeaterOrder>(tagStream->readUint8());
  if (hasAnchorX) repeater->anchor.x = tagStream->readFloat();
  if (hasAnchorY) repeater->anchor.y = tagStream->readFloat();
  if (hasPosX) repeater->position.x = tagStream->readFloat();
  if (hasPosY) repeater->position.y = tagStream->readFloat();
  if (hasRotation) repeater->rotation = tagStream->readFloat();
  if (hasScaleX) repeater->scale.x = tagStream->readFloat();
  if (hasScaleY) repeater->scale.y = tagStream->readFloat();
  if (hasStartAlpha) {
    repeater->startAlpha = static_cast<float>(tagStream->readUint8()) / 255.0f;
  }
  if (hasEndAlpha) {
    repeater->endAlpha = static_cast<float>(tagStream->readUint8()) / 255.0f;
  }
  return repeater;
}

Element* ReadFillTag(pag::DecodeStream* tagStream, DecodeContext* context) {
  auto* fill = context->document->makeNode<Fill>();
  PAGAttributeBlockReader reader(tagStream, 5);
  bool hasColor = reader.flag(0);
  bool hasAlpha = reader.flag(1);
  bool hasBlend = reader.flag(2);
  bool hasFillRule = reader.flag(3);
  bool hasPlacement = reader.flag(4);
  // The color reference is a value-area entry; the inline color source sub-tag it may point
  // to sits at the value-area END (spec §9.0), so it is read after all values below.
  uint32_t colorRef = 0;
  if (hasColor) {
    colorRef = tagStream->readEncodedUint32();
  }
  if (hasAlpha) fill->alpha = static_cast<float>(tagStream->readUint8()) / 255.0f;
  if (hasBlend) fill->blendMode = static_cast<BlendMode>(tagStream->readUint8());
  if (hasFillRule) fill->fillRule = static_cast<FillRule>(tagStream->readUint8());
  if (hasPlacement) fill->placement = static_cast<LayerPlacement>(tagStream->readUint8());
  if (hasColor && colorRef == 0) {
    auto csHeader = ReadPAGTagHeader(tagStream);
    auto csBytes = tagStream->readBytes(csHeader.length);
    fill->color = ReadColorSourceTag(&csBytes, csHeader.code, context);
  }
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
  uint32_t colorRef = 0;
  if (hasColor) {
    colorRef = tagStream->readEncodedUint32();
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
  // The inline color source sub-tag sits at the value-area END (spec §9.0).
  if (hasColor && colorRef == 0) {
    auto csHeader = ReadPAGTagHeader(tagStream);
    auto csBytes = tagStream->readBytes(csHeader.length);
    stroke->color = ReadColorSourceTag(&csBytes, csHeader.code, context);
  }
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
    case TagCode::Polystar:
      return ReadPolystarTag(tagStream, context);
    case TagCode::TrimPath:
      return ReadTrimPathTag(tagStream, context);
    case TagCode::RoundCorner:
      return ReadRoundCornerTag(tagStream, context);
    case TagCode::MergePath:
      return ReadMergePathTag(tagStream, context);
    case TagCode::TextPath:
      return ReadTextPathTag(tagStream, context);
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
    case TagCode::TextBoxBlock:
      return ReadTextBoxTag(tagStream, context);
    case TagCode::Repeater:
      return ReadRepeaterTag(tagStream, context);
    default:
      return nullptr;
  }
}

void ReadLayerBlockContent(pag::DecodeStream* tagStream, Layer* layer, DecodeContext* context,
                           std::vector<std::pair<Layer*, uint32_t>>* subDocRefs,
                           std::vector<std::pair<Layer*, uint32_t>>* pendingMaskRefsTarget) {
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
      bool hasComposition = reader.flag(17);
      bool hasSubDocument = reader.flag(18);
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
      if (hasMask) {
        pendingMaskRefsTarget->push_back({layer, tagBytes.readEncodedUint32()});
      }
      if (hasComposition) {
        context->pendingCompositionRefs.push_back({layer, tagBytes.readEncodedUint32()});
      }
      if (hasMaskType) layer->maskType = static_cast<MaskType>(tagBytes.readUint8());
      if (hasSubDocument) subDocRefs->push_back({layer, tagBytes.readEncodedUint32()});
      if (hasVmContext) layer->vmContext = context->getString(tagBytes.readEncodedUint32());
    } else if (header.code == TagCode::LayerBlock) {
      auto* child = context->document->makeNode<Layer>();
      ReadLayerBlockContent(&tagBytes, child, context, subDocRefs, pendingMaskRefsTarget);
      layer->children.push_back(child);
    } else if (header.code == TagCode::CompositionBlock) {
      layer->composition = ReadCompositionBlock(&tagBytes, context);
    } else if (header.code == TagCode::DropShadowStyle ||
               header.code == TagCode::InnerShadowStyle ||
               header.code == TagCode::BackgroundBlurStyle || header.code == TagCode::NoiseStyle ||
               header.code == TagCode::GlassStyle) {
      auto* style = ReadLayerStyleTag(&tagBytes, header.code, context);
      if (style != nullptr) {
        layer->styles.push_back(style);
      }
    } else if (header.code == TagCode::BlurFilter || header.code == TagCode::DropShadowFilter ||
               header.code == TagCode::InnerShadowFilter || header.code == TagCode::BlendFilter ||
               header.code == TagCode::ColorMatrixFilter || header.code == TagCode::NoiseFilter) {
      auto* filter = ReadLayerFilterTag(&tagBytes, header.code, context);
      if (filter != nullptr) {
        layer->filters.push_back(filter);
      }
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
      ReadLayerBlockContent(&tagBytes, layer, context, &context->pendingSubDocRefs,
                            &context->pendingMaskRefs);
      doc->layers.push_back(layer);
    }
  }
}

// Builds one independent instance of the table entry (spec §6.5: every referencing layer
// decodes its own copy so edits and runtime state stay isolated). Nested references recurse
// with `chain` guarding entry cycles; the wrapper composition attached through
// AttachExternalDocument matches the XML load path exactly.
std::shared_ptr<PAGXDocument> MakeSubDocumentInstance(uint32_t index,
                                                      std::unordered_set<uint32_t>* chain,
                                                      PAGXDocument* rootDocument,
                                                      DecodeContext* context, int* instanceCount,
                                                      int* nodeCount) {
  if (index >= context->subDocuments.size() || chain->count(index) > 0 || *instanceCount >= 1024 ||
      *nodeCount >= 1048576) {
    if (index < context->subDocuments.size() && chain->count(index) > 0) {
      rootDocument->errors.push_back("Cyclic external composition reference detected: '" +
                                     context->subDocuments[index].name + "'.");
    }
    return nullptr;
  }
  auto& entry = context->subDocuments[index];
  auto instance = PAGXImporter::FromXML(PAGXExporter::ToXML(*entry.document));
  if (instance == nullptr) {
    return nullptr;
  }
  (*instanceCount)++;
  *nodeCount += static_cast<int>(instance->nodes.size());
  instance->diagnosticName = entry.name;
  chain->insert(index);
  for (auto& pending : entry.pendingRefs) {
    if (pending.first >= instance->layers.size()) {
      continue;
    }
    auto nested = MakeSubDocumentInstance(pending.second, chain, rootDocument, context,
                                          instanceCount, nodeCount);
    if (nested != nullptr) {
      PAGXDocument::AttachExternalDocument(instance.get(), instance->layers[pending.first], nested);
    }
  }
  chain->erase(index);
  return instance;
}

// SubDocument content (spec §6.5): varuint diagnosticName, DocumentAttributes(263),
// [RetainedIdTable(264)], the sub-document's layers, End. Decoded once into a template.
void ReadSubDocument(pag::DecodeStream* tagStream, DecodeContext* context) {
  if (context->subDocuments.size() >= 256) {
    PAGThrowError(tagStream->context, "sub document table count exceeds the spec limit");
    return;
  }
  DecodeContext::SubDocumentEntry entry = {};
  entry.name = context->getString(tagStream->readEncodedUint32());
  entry.document = PAGXDocument::Make(0, 0);
  std::vector<std::pair<Layer*, uint32_t>> pendingRefs = {};
  std::vector<std::pair<Layer*, uint32_t>> pendingMaskRefs = {};
  auto savedDocument = context->document;
  context->document = entry.document.get();
  while (true) {
    auto header = ReadPAGTagHeader(tagStream);
    if (header.code == TagCode::End) break;
    auto tagBytes = tagStream->readBytes(header.length);
    if (header.code == TagCode::DocumentAttributes) {
      PAGAttributeBlockReader reader(&tagBytes, 4);
      bool hasWidth = reader.flag(0);
      bool hasHeight = reader.flag(1);
      tagBytes.alignWithBytes();
      if (hasWidth) entry.document->width = tagBytes.readFloat();
      if (hasHeight) entry.document->height = tagBytes.readFloat();
    } else if (header.code == TagCode::RetainedIdTable) {
      auto count = tagBytes.readEncodedUint32();
      for (uint32_t i = 0; i < count; i++) {
        tagBytes.readEncodedUint32();
        tagBytes.readEncodedUint32();
      }
    } else if (header.code == TagCode::LayerBlock) {
      auto* layer = context->document->makeNode<Layer>();
      ReadLayerBlockContent(&tagBytes, layer, context, &pendingRefs, &pendingMaskRefs);
      entry.document->layers.push_back(layer);
    }
  }
  context->document = savedDocument;
  // Backfill the sub-document's own mask refs against its node table (its own §5.1 namespace).
  for (auto& pending : pendingMaskRefs) {
    if (pending.second == 0 || pending.second > entry.document->nodes.size()) {
      continue;
    }
    auto* node = entry.document->nodes[pending.second - 1].get();
    if (node->nodeType() == NodeType::Layer) {
      pending.first->mask = static_cast<Layer*>(node);
    }
  }
  // The pending refs record template layers; store their top-level indices.
  for (auto& pending : pendingRefs) {
    size_t layerIndex = entry.document->layers.size();
    for (size_t i = 0; i < entry.document->layers.size(); i++) {
      if (entry.document->layers[i] == pending.first) {
        layerIndex = i;
        break;
      }
    }
    if (layerIndex < entry.document->layers.size()) {
      entry.pendingRefs.push_back({layerIndex, pending.second});
    }
  }
  context->subDocuments.push_back(std::move(entry));
}

// SubDocumentTable content (spec §6.5): SubDocument(261) tags in first-reference order, End.
void ReadSubDocumentTable(pag::DecodeStream* tagStream, DecodeContext* context) {
  while (true) {
    auto header = ReadPAGTagHeader(tagStream);
    if (header.code == TagCode::End) break;
    auto tagBytes = tagStream->readBytes(header.length);
    if (header.code == TagCode::SubDocument) {
      ReadSubDocument(&tagBytes, context);
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
      case TagCode::SubDocumentTable:
        ReadSubDocumentTable(&tagBytes, &context);
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

  // Attach sub-document instances: every referencing root layer decodes an independent copy
  // (spec §6.5) with the same wrapper composition the XML load path builds. Expansion caps
  // follow spec §11; cycles mirror the loader's message so PAGScene::Make refuses them.
  int subDocInstanceCount = 0;
  int subDocNodeCount = static_cast<int>(doc->nodes.size());
  for (auto& pending : context.pendingSubDocRefs) {
    std::unordered_set<uint32_t> chain = {};
    auto instance = MakeSubDocumentInstance(pending.second, &chain, doc.get(), &context,
                                            &subDocInstanceCount, &subDocNodeCount);
    if (instance != nullptr) {
      PAGXDocument::AttachExternalDocument(doc.get(), pending.first, instance);
    }
  }
  if (subDocInstanceCount > 1024 || subDocNodeCount > 1048576) {
    if (result != nullptr) {
      result->success = false;
      result->error = "sub-document expansion exceeds the spec limits";
    }
    return nullptr;
  }

  // Backfill layer composition refs: resource-block entries resolve against
  // resourceCompositions (the ResourceBlock precedes the DocumentBlock in the body).
  for (auto& pending : context.pendingCompositionRefs) {
    if (pending.second > 0 && pending.second <= context.resourceCompositions.size()) {
      pending.first->composition = context.resourceCompositions[pending.second - 1];
    }
  }

  // Backfill layer mask NodeRefs against the root document's node table (creation order =
  // emission order, spec §5.1). Wrapper compositions appended by sub-document attach sit after
  // every indexed node, so indices stay valid.
  for (auto& pending : context.pendingMaskRefs) {
    if (pending.second == 0 || pending.second > doc->nodes.size()) {
      continue;
    }
    auto* node = doc->nodes[pending.second - 1].get();
    if (node->nodeType() == NodeType::Layer) {
      pending.first->mask = static_cast<Layer*>(node);
    }
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
