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
//  license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/PAGExporter.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <unordered_map>
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/DiamondGradient.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Gradient.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/pag/PAGAttributeIO.h"
#include "pagx/pag/PAGDataTypes.h"
#include "pagx/pag/PAGFileHeader.h"
#include "pagx/pag/PAGPathIO.h"
#include "pagx/pag/PAGTagHeader.h"
#include "pagx/pag/TagCode.h"
#include "pagx/pag/exporter/ImageProcessor.h"
#include "pagx/pag/exporter/LayoutFlattener.h"
#include "pagx/pag/exporter/Stripper.h"

namespace pagx {
namespace {

// Interns strings in first-use order for the StringTable (spec §6.2).
struct StringTableBuilder {
  std::vector<std::string> strings;
  std::unordered_map<std::string, uint32_t> indices;

  uint32_t intern(const std::string& text) {
    auto it = indices.find(text);
    if (it != indices.end()) {
      return it->second;
    }
    auto index = static_cast<uint32_t>(strings.size());
    strings.push_back(text);
    indices[text] = index;
    return index;
  }
};

// Shared encoding state threaded through all tag writers.
struct EncodeState {
  StringTableBuilder strings;
  pag::StreamContext streamContext;
  std::vector<PAGExportIssue> warnings;
  std::vector<ImageTableEntry> imageEntries;
  std::unordered_map<const Image*, uint32_t> imageIndices;
  // Shared-image placement (spec §5.3, §12): sharing is content-based — patterns referencing
  // the same ImageTable entry (T4 already dedups identical bytes into one entry) share one
  // ResourceBlock slot in first-reference order (sharedImageOrder holds the representative
  // node per entry, entryResourceSlots maps the entry index to the 0-based slot); single-use
  // entries are inlined at the referencing pattern's value-area end. Content-based counting
  // keeps decode/re-encode byte-identical when the internal XML round trip splits a shared
  // node into per-pattern nodes with equal bytes.
  std::vector<Image*> sharedImageOrder;
  std::unordered_map<uint32_t, uint32_t> entryResourceSlots;
  // Shared-font placement (spec §5.3): fonts referenced by two or more glyph runs go into the
  // ResourceBlock in first-reference order (sharedFontOrder), single-use fonts are inlined at
  // the referencing glyph run's value-area end. Fonts keep node identity through the XML round
  // trip (resource @id), so node-based counting is stable.
  std::vector<Font*> sharedFontOrder;
  std::unordered_map<const Font*, uint32_t> fontResourceSlots;

  uint32_t intern(const std::string& text) {
    return strings.intern(text);
  }

  void warn(PAGExportWarning code, const std::string& message) {
    PAGExportIssue issue = {};
    issue.code = code;
    issue.message = message;
    warnings.push_back(issue);
  }
};

void WriteTag(pag::EncodeStream* stream, TagCode code, pag::EncodeStream* content) {
  content->alignWithBytes();
  WritePAGTagHeader(stream, code, content->length());
  stream->writeBytes(content);
}

// ---------------------------------------------------------------------------
// Pass 1: string collection (pre-order, mirroring the emission traversal).
// ---------------------------------------------------------------------------

void CollectStringsFromColorSource(ColorSource* source, EncodeState* state) {
  if (source == nullptr) {
    return;
  }
  if (source->nodeType() == NodeType::ImagePattern) {
    auto* pattern = static_cast<ImagePattern*>(source);
    // Sorted by key: the map iteration order is insertion-dependent, so both passes sort to
    // keep string indices (and therefore bytes) deterministic across decode/re-encode (§12).
    std::vector<std::string> keys = {};
    keys.reserve(pattern->customData.size());
    for (auto& [key, value] : pattern->customData) {
      keys.push_back(key);
    }
    std::sort(keys.begin(), keys.end());
    for (auto& key : keys) {
      state->intern(key);
      state->intern(pattern->customData[key]);
    }
  }
}

void CollectStringsFromElement(Element* element, EncodeState* state) {
  if (element == nullptr) {
    return;
  }
  switch (element->nodeType()) {
    case NodeType::Text: {
      auto* text = static_cast<Text*>(element);
      state->intern(text->text);
      if (!text->fontFamily.empty()) {
        state->intern(text->fontFamily);
      }
      if (!text->fontStyle.empty()) {
        state->intern(text->fontStyle);
      }
      break;
    }
    case NodeType::Fill:
      CollectStringsFromColorSource(static_cast<Fill*>(element)->color, state);
      break;
    case NodeType::Stroke:
      CollectStringsFromColorSource(static_cast<Stroke*>(element)->color, state);
      break;
    case NodeType::Group:
      for (auto* child : static_cast<Group*>(element)->elements) {
        CollectStringsFromElement(child, state);
      }
      break;
    case NodeType::TextBox:
      for (auto* child : static_cast<TextBox*>(element)->elements) {
        CollectStringsFromElement(child, state);
      }
      break;
    default:
      break;
  }
}

void CollectStringsFromLayer(Layer* layer, EncodeState* state) {
  if (!layer->name.empty()) {
    state->intern(layer->name);
  }
  if (!layer->id.empty()) {
    state->intern(layer->id);
  }
  for (auto* element : layer->contents) {
    CollectStringsFromElement(element, state);
  }
  for (auto* style : layer->styles) {
    (void)style;
  }
  for (auto* filter : layer->filters) {
    (void)filter;
  }
  for (auto* child : layer->children) {
    CollectStringsFromLayer(child, state);
  }
}

// ---------------------------------------------------------------------------
// Pass 2: tag writing.
// ---------------------------------------------------------------------------

// Collects ImagePattern image references in traversal order (spec §5.3 first-reference
// order). Every reference is appended, including repeats of the same node.
void CollectImageReference(Element* element, std::vector<Image*>* references) {
  if (element == nullptr) {
    return;
  }
  ColorSource* source = nullptr;
  if (element->nodeType() == NodeType::Fill) {
    source = static_cast<Fill*>(element)->color;
  } else if (element->nodeType() == NodeType::Stroke) {
    source = static_cast<Stroke*>(element)->color;
  } else if (element->nodeType() == NodeType::Group) {
    for (auto* child : static_cast<Group*>(element)->elements) {
      CollectImageReference(child, references);
    }
  } else if (element->nodeType() == NodeType::TextBox) {
    for (auto* child : static_cast<TextBox*>(element)->elements) {
      CollectImageReference(child, references);
    }
  }
  if (source != nullptr && source->nodeType() == NodeType::ImagePattern) {
    auto* image = static_cast<ImagePattern*>(source)->image;
    if (image != nullptr) {
      references->push_back(image);
    }
  }
}

void CollectImageReferencesFromLayer(Layer* layer, std::vector<Image*>* references) {
  for (auto* element : layer->contents) {
    CollectImageReference(element, references);
  }
  for (auto* child : layer->children) {
    CollectImageReferencesFromLayer(child, references);
  }
}

// Collects Font references from glyph runs in traversal order (spec §5.3 first-reference
// order). Every reference is appended, including repeats of the same node.
void CollectFontReference(Element* element, std::vector<Font*>* references) {
  if (element == nullptr) {
    return;
  }
  if (element->nodeType() == NodeType::Text) {
    for (auto* run : static_cast<Text*>(element)->glyphRuns) {
      if (run->font != nullptr) {
        references->push_back(run->font);
      }
    }
    return;
  }
  if (element->nodeType() == NodeType::Group) {
    for (auto* child : static_cast<Group*>(element)->elements) {
      CollectFontReference(child, references);
    }
    return;
  }
  if (element->nodeType() == NodeType::TextBox) {
    for (auto* child : static_cast<TextBox*>(element)->elements) {
      CollectFontReference(child, references);
    }
  }
}

void CollectFontReferencesFromLayer(Layer* layer, std::vector<Font*>* references) {
  for (auto* element : layer->contents) {
    CollectFontReference(element, references);
  }
  for (auto* child : layer->children) {
    CollectFontReferencesFromLayer(child, references);
  }
}

// Image tag (spec §9.1 #288): a single Fixed varuint — the ImageTable reference (1-based;
// 0 = no data after a missing-image export warning). No exist bits.
void WriteImageTag(pag::EncodeStream* stream, Image* image, EncodeState* state) {
  pag::EncodeStream content(&state->streamContext);
  uint32_t ref = 0;
  auto it = state->imageIndices.find(image);
  if (it != state->imageIndices.end()) {
    ref = it->second + 1;
  }
  content.writeEncodedUint32(ref);
  WriteTag(stream, TagCode::Image, &content);
}

// Writes the columnar gradient stop tail (spec §9.1): stopCount varuint, one packed FloatList
// of offsets (GRADIENT_PRECISION 0.00002), then byte-aligned colors.
void WriteGradientStops(pag::EncodeStream* tag, Gradient* gradient) {
  auto stopCount = static_cast<uint32_t>(gradient->colorStops.size());
  tag->writeEncodedUint32(stopCount);
  std::vector<float> offsets;
  offsets.reserve(stopCount);
  for (auto* stop : gradient->colorStops) {
    offsets.push_back(stop->offset);
  }
  tag->writeFloatList(offsets.data(), stopCount, 0.00002f);
  tag->alignWithBytes();
  for (auto* stop : gradient->colorStops) {
    WritePAGColor(tag, stop->color);
  }
}

// Gradient common exist bits (matrix, fitsToGeometry), written after the type-specific ones.
void WriteGradientCommonFlags(PAGAttributeBlockWriter* writer, Gradient* gradient) {
  writer->writeFlag(!gradient->matrix.isIdentity());
  writer->writeFlag(!gradient->fitsToGeometry);
}

// Gradient common values, written after the type-specific ones to match flag order.
void WriteGradientCommonValues(pag::EncodeStream* v, Gradient* gradient) {
  if (!gradient->matrix.isIdentity()) {
    WritePAGMatrix(v, gradient->matrix);
  }
  if (!gradient->fitsToGeometry) {
    v->writeBoolean(gradient->fitsToGeometry);
  }
}

// Writes a color source as an inline sub-tag appended to the parent tag's value area (§9.0).
// Returns false when the source type has no phase-1 encoding yet; the caller must then clear
// the owning reference flag and emit an UnsupportedColorSource warning.
bool WriteColorSourceInline(pag::EncodeStream* content, ColorSource* source, EncodeState* state) {
  if (source == nullptr) {
    return false;
  }
  pag::EncodeStream tag(&state->streamContext);
  switch (source->nodeType()) {
    case NodeType::SolidColor: {
      auto* solid = static_cast<SolidColor*>(source);
      PAGAttributeBlockWriter writer(&state->streamContext);
      writer.writeFlag(true);  // color always exists
      WritePAGColor(writer.valueStream(), solid->color);
      auto block = writer.release();
      tag.writeBytes(const_cast<uint8_t*>(block->data()), static_cast<uint32_t>(block->length()));
      WriteTag(content, TagCode::SolidColor, &tag);
      return true;
    }
    case NodeType::LinearGradient: {
      auto* grad = static_cast<LinearGradient*>(source);
      PAGAttributeBlockWriter writer(&state->streamContext);
      auto hasStart = grad->startPoint.x != 0.0f || grad->startPoint.y != 0.0f;
      auto hasEnd = grad->endPoint.x != 1.0f || grad->endPoint.y != 0.0f;
      writer.writeFlag(hasStart);
      writer.writeFlag(hasEnd);
      WriteGradientCommonFlags(&writer, grad);
      auto* v = writer.valueStream();
      if (hasStart) {
        v->writeFloat(grad->startPoint.x);
        v->writeFloat(grad->startPoint.y);
      }
      if (hasEnd) {
        v->writeFloat(grad->endPoint.x);
        v->writeFloat(grad->endPoint.y);
      }
      WriteGradientCommonValues(v, grad);
      auto block = writer.release();
      tag.writeBytes(const_cast<uint8_t*>(block->data()), static_cast<uint32_t>(block->length()));
      WriteGradientStops(&tag, grad);
      WriteTag(content, TagCode::LinearGradient, &tag);
      return true;
    }
    case NodeType::RadialGradient: {
      auto* grad = static_cast<RadialGradient*>(source);
      PAGAttributeBlockWriter writer(&state->streamContext);
      auto hasCenter = grad->center.x != 0.5f || grad->center.y != 0.5f;
      auto hasRadius = grad->radius != 0.5f;
      writer.writeFlag(hasCenter);
      writer.writeFlag(hasRadius);
      WriteGradientCommonFlags(&writer, grad);
      auto* v = writer.valueStream();
      if (hasCenter) {
        v->writeFloat(grad->center.x);
        v->writeFloat(grad->center.y);
      }
      if (hasRadius) {
        v->writeFloat(grad->radius);
      }
      WriteGradientCommonValues(v, grad);
      auto block = writer.release();
      tag.writeBytes(const_cast<uint8_t*>(block->data()), static_cast<uint32_t>(block->length()));
      WriteGradientStops(&tag, grad);
      WriteTag(content, TagCode::RadialGradient, &tag);
      return true;
    }
    case NodeType::ConicGradient: {
      auto* grad = static_cast<ConicGradient*>(source);
      PAGAttributeBlockWriter writer(&state->streamContext);
      auto hasCenter = grad->center.x != 0.5f || grad->center.y != 0.5f;
      auto hasStart = grad->startAngle != 0.0f;
      auto hasEnd = grad->endAngle != 360.0f;
      writer.writeFlag(hasCenter);
      writer.writeFlag(hasStart);
      writer.writeFlag(hasEnd);
      WriteGradientCommonFlags(&writer, grad);
      auto* v = writer.valueStream();
      if (hasCenter) {
        v->writeFloat(grad->center.x);
        v->writeFloat(grad->center.y);
      }
      if (hasStart) {
        v->writeFloat(grad->startAngle);
      }
      if (hasEnd) {
        v->writeFloat(grad->endAngle);
      }
      WriteGradientCommonValues(v, grad);
      auto block = writer.release();
      tag.writeBytes(const_cast<uint8_t*>(block->data()), static_cast<uint32_t>(block->length()));
      WriteGradientStops(&tag, grad);
      WriteTag(content, TagCode::ConicGradient, &tag);
      return true;
    }
    case NodeType::DiamondGradient: {
      auto* grad = static_cast<DiamondGradient*>(source);
      PAGAttributeBlockWriter writer(&state->streamContext);
      auto hasCenter = grad->center.x != 0.5f || grad->center.y != 0.5f;
      auto hasRadius = grad->radius != 0.5f;
      writer.writeFlag(hasCenter);
      writer.writeFlag(hasRadius);
      WriteGradientCommonFlags(&writer, grad);
      auto* v = writer.valueStream();
      if (hasCenter) {
        v->writeFloat(grad->center.x);
        v->writeFloat(grad->center.y);
      }
      if (hasRadius) {
        v->writeFloat(grad->radius);
      }
      WriteGradientCommonValues(v, grad);
      auto block = writer.release();
      tag.writeBytes(const_cast<uint8_t*>(block->data()), static_cast<uint32_t>(block->length()));
      WriteGradientStops(&tag, grad);
      WriteTag(content, TagCode::DiamondGradient, &tag);
      return true;
    }
    case NodeType::ImagePattern: {
      auto* pattern = static_cast<ImagePattern*>(source);
      PAGAttributeBlockWriter writer(&state->streamContext);
      auto hasImage = pattern->image != nullptr;
      writer.writeFlag(hasImage);
      writer.writeFlag(pattern->tileModeX != TileMode::Decal);
      writer.writeFlag(pattern->tileModeY != TileMode::Decal);
      writer.writeFlag(pattern->filterMode != FilterMode::Linear);
      writer.writeFlag(pattern->mipmapMode != MipmapMode::Linear);
      writer.writeFlag(!pattern->matrix.isIdentity());
      writer.writeFlag(pattern->scaleMode != ScaleMode::LetterBox);
      auto hasData = !pattern->customData.empty();
      writer.writeFlag(hasData);
      writer.writeFlag(hasData);
      auto* v = writer.valueStream();
      uint32_t imageRef = 0;
      if (hasImage) {
        auto entryIt = state->imageIndices.find(pattern->image);
        if (entryIt != state->imageIndices.end()) {
          auto slotIt = state->entryResourceSlots.find(entryIt->second);
          if (slotIt != state->entryResourceSlots.end()) {
            imageRef = slotIt->second + 1;
          }
        }
        // imageRef 0 stays = the image is written inline at the value-area end below.
        v->writeEncodedUint32(imageRef);
      }
      if (pattern->tileModeX != TileMode::Decal) {
        v->writeUint8(static_cast<uint8_t>(pattern->tileModeX));
      }
      if (pattern->tileModeY != TileMode::Decal) {
        v->writeUint8(static_cast<uint8_t>(pattern->tileModeY));
      }
      if (pattern->filterMode != FilterMode::Linear) {
        v->writeUint8(static_cast<uint8_t>(pattern->filterMode));
      }
      if (pattern->mipmapMode != MipmapMode::Linear) {
        v->writeUint8(static_cast<uint8_t>(pattern->mipmapMode));
      }
      if (!pattern->matrix.isIdentity()) {
        WritePAGMatrix(v, pattern->matrix);
      }
      if (pattern->scaleMode != ScaleMode::LetterBox) {
        v->writeUint8(static_cast<uint8_t>(pattern->scaleMode));
      }
      if (hasData) {
        // Sorted by key so decode/re-encode stays byte-identical (§12 determinism).
        std::vector<std::string> keys = {};
        keys.reserve(pattern->customData.size());
        for (auto& [key, value] : pattern->customData) {
          keys.push_back(key);
        }
        std::sort(keys.begin(), keys.end());
        v->writeEncodedUint32(static_cast<uint32_t>(keys.size()));
        for (auto& key : keys) {
          v->writeEncodedUint32(state->intern(key));
        }
        v->writeEncodedUint32(static_cast<uint32_t>(keys.size()));
        for (auto& key : keys) {
          v->writeEncodedUint32(state->intern(pattern->customData[key]));
        }
      }
      auto block = writer.release();
      tag.writeBytes(const_cast<uint8_t*>(block->data()), static_cast<uint32_t>(block->length()));
      if (hasImage && imageRef == 0) {
        WriteImageTag(&tag, pattern->image, state);
      }
      WriteTag(content, TagCode::ImagePattern, &tag);
      return true;
    }
    default:
      // Color source types without a phase-1 encoding; callers warn and drop the reference.
      return false;
  }
}

// Writes a Fill tag. The content is the attribute block (colorRef varuint in the value area)
// followed by the inline color source sub-tag at the value-area end (spec §10.3), so the
// declared tag length covers both.
void WriteFillTag(pag::EncodeStream* stream, Fill* fill, EncodeState* state) {
  pag::EncodeStream colorTag(&state->streamContext);
  auto hasColor = WriteColorSourceInline(&colorTag, fill->color, state);
  if (fill->color != nullptr && !hasColor) {
    state->warn(PAGExportWarning::UnsupportedColorSource,
                "color source has no binary encoding yet");
  }
  PAGAttributeBlockWriter writer(&state->streamContext);
  writer.writeFlag(hasColor);
  writer.writeFlag(fill->alpha != 1.0f);
  writer.writeFlag(fill->blendMode != BlendMode::Normal);
  writer.writeFlag(fill->fillRule != FillRule::Winding);
  writer.writeFlag(fill->placement != LayerPlacement::Background);
  auto* v = writer.valueStream();
  if (hasColor) {
    v->writeEncodedUint32(0);  // 0 = inline color source
  }
  if (fill->alpha != 1.0f) {
    v->writeUint8(static_cast<uint8_t>(lroundf(fill->alpha * 255.0f)));
  }
  if (fill->blendMode != BlendMode::Normal) {
    v->writeUint8(static_cast<uint8_t>(fill->blendMode));
  }
  if (fill->fillRule != FillRule::Winding) {
    v->writeUint8(static_cast<uint8_t>(fill->fillRule));
  }
  if (fill->placement != LayerPlacement::Background) {
    v->writeUint8(static_cast<uint8_t>(fill->placement));
  }
  auto block = writer.release();
  pag::EncodeStream content(&state->streamContext);
  content.writeBytes(const_cast<uint8_t*>(block->data()), static_cast<uint32_t>(block->length()));
  if (hasColor) {
    content.writeBytes(&colorTag);
  }
  WriteTag(stream, TagCode::Fill, &content);
}

// Writes a Stroke tag (spec §9.3 field order: color, width, alpha, blendMode, cap, join,
// miterLimit, dashes, dashOffset, dashAdaptive, align, placement). The inline color source
// sub-tag sits at the value-area end inside the declared length.
void WriteStrokeTag(pag::EncodeStream* stream, Stroke* stroke, EncodeState* state) {
  pag::EncodeStream colorTag(&state->streamContext);
  auto hasColor = WriteColorSourceInline(&colorTag, stroke->color, state);
  if (stroke->color != nullptr && !hasColor) {
    state->warn(PAGExportWarning::UnsupportedColorSource,
                "color source has no binary encoding yet");
  }
  PAGAttributeBlockWriter writer(&state->streamContext);
  writer.writeFlag(hasColor);
  writer.writeFlag(stroke->width != 1.0f);
  writer.writeFlag(stroke->alpha != 1.0f);
  writer.writeFlag(stroke->blendMode != BlendMode::Normal);
  writer.writeFlag(stroke->cap != LineCap::Butt);
  writer.writeFlag(stroke->join != LineJoin::Miter);
  writer.writeFlag(stroke->miterLimit != 4.0f);
  writer.writeFlag(!stroke->dashes.empty());
  writer.writeFlag(stroke->dashOffset != 0.0f);
  writer.writeFlag(stroke->dashAdaptive);
  writer.writeFlag(stroke->align != StrokeAlign::Center);
  writer.writeFlag(stroke->placement != LayerPlacement::Background);
  auto* v = writer.valueStream();
  if (hasColor) {
    v->writeEncodedUint32(0);  // 0 = inline color source
  }
  if (stroke->width != 1.0f) {
    v->writeFloat(stroke->width);
  }
  if (stroke->alpha != 1.0f) {
    v->writeUint8(static_cast<uint8_t>(lroundf(stroke->alpha * 255.0f)));
  }
  if (stroke->blendMode != BlendMode::Normal) {
    v->writeUint8(static_cast<uint8_t>(stroke->blendMode));
  }
  if (stroke->cap != LineCap::Butt) {
    v->writeUint8(static_cast<uint8_t>(stroke->cap));
  }
  if (stroke->join != LineJoin::Miter) {
    v->writeUint8(static_cast<uint8_t>(stroke->join));
  }
  if (stroke->miterLimit != 4.0f) {
    v->writeFloat(stroke->miterLimit);
  }
  if (!stroke->dashes.empty()) {
    v->writeEncodedUint32(static_cast<uint32_t>(stroke->dashes.size()));
    for (auto dash : stroke->dashes) {
      v->writeFloat(dash);
    }
  }
  if (stroke->dashOffset != 0.0f) {
    v->writeFloat(stroke->dashOffset);
  }
  if (stroke->align != StrokeAlign::Center) {
    v->writeUint8(static_cast<uint8_t>(stroke->align));
  }
  if (stroke->placement != LayerPlacement::Background) {
    v->writeUint8(static_cast<uint8_t>(stroke->placement));
  }
  auto block = writer.release();
  pag::EncodeStream content(&state->streamContext);
  content.writeBytes(const_cast<uint8_t*>(block->data()), static_cast<uint32_t>(block->length()));
  if (hasColor) {
    content.writeBytes(&colorTag);
  }
  WriteTag(stream, TagCode::Stroke, &content);
}

void WriteRectangleTag(pag::EncodeStream* stream, Rectangle* rect, EncodeState* state) {
  PAGAttributeBlockWriter writer(&state->streamContext);
  auto hasPosX = !std::isnan(rect->position.x);
  auto hasPosY = !std::isnan(rect->position.y);
  auto hasRoundness = rect->roundness != 0.0f;
  writer.writeFlag(hasPosX);
  writer.writeFlag(hasPosY);
  writer.writeFlag(true);
  writer.writeFlag(true);
  writer.writeFlag(hasRoundness);
  writer.writeFlag(rect->reversed);
  auto* v = writer.valueStream();
  if (hasPosX) v->writeFloat(rect->position.x);
  if (hasPosY) v->writeFloat(rect->position.y);
  v->writeFloat(rect->size.width);
  v->writeFloat(rect->size.height);
  if (hasRoundness) v->writeFloat(rect->roundness);
  auto block = writer.release();
  WritePAGTagHeader(stream, TagCode::Rectangle, static_cast<uint32_t>(block->length()));
  stream->writeBytes(const_cast<uint8_t*>(block->data()), static_cast<uint32_t>(block->length()));
}

void WriteEllipseTag(pag::EncodeStream* stream, Ellipse* ellipse, EncodeState* state) {
  PAGAttributeBlockWriter writer(&state->streamContext);
  auto hasPosX = !std::isnan(ellipse->position.x);
  auto hasPosY = !std::isnan(ellipse->position.y);
  writer.writeFlag(hasPosX);
  writer.writeFlag(hasPosY);
  writer.writeFlag(true);
  writer.writeFlag(true);
  writer.writeFlag(ellipse->reversed);
  auto* v = writer.valueStream();
  if (hasPosX) v->writeFloat(ellipse->position.x);
  if (hasPosY) v->writeFloat(ellipse->position.y);
  v->writeFloat(ellipse->size.width);
  v->writeFloat(ellipse->size.height);
  auto block = writer.release();
  WritePAGTagHeader(stream, TagCode::Ellipse, static_cast<uint32_t>(block->length()));
  stream->writeBytes(const_cast<uint8_t*>(block->data()), static_cast<uint32_t>(block->length()));
}

// Writes a Path tag (spec §10.5): the inline PathData sub-tag sits at the value-area end
// inside the declared tag length.
void WritePathTag(pag::EncodeStream* stream, Path* path, EncodeState* state) {
  PAGAttributeBlockWriter writer(&state->streamContext);
  writer.writeFlag(path->data != nullptr);
  writer.writeFlag(path->position.x != 0.0f);
  writer.writeFlag(path->position.y != 0.0f);
  writer.writeFlag(path->reversed);
  auto* v = writer.valueStream();
  if (path->data != nullptr) {
    v->writeEncodedUint32(0);  // 0 = inline path data
  }
  if (path->position.x != 0.0f) v->writeFloat(path->position.x);
  if (path->position.y != 0.0f) v->writeFloat(path->position.y);
  auto block = writer.release();
  pag::EncodeStream content(&state->streamContext);
  content.writeBytes(const_cast<uint8_t*>(block->data()), static_cast<uint32_t>(block->length()));
  if (path->data != nullptr) {
    pag::EncodeStream pathTag(&state->streamContext);
    WritePAGPath(&pathTag, path->data, 0);
    WriteTag(&content, TagCode::PathData, &pathTag);
  }
  WriteTag(stream, TagCode::Path, &content);
}

// Font tag (spec §9.1 #296): unitsPerEm exist bit (default 1000), glyphCount (Fixed), then
// the glyph entries. Each entry: u8 flags (bit0 hasPath, bit1 hasImage), advance/offset f32s,
// the image table ref for bitmap glyphs, and the §7 path layout for vector glyphs (font units,
// stepExponent fixed 0).
void WriteFontTag(pag::EncodeStream* stream, Font* font, EncodeState* state) {
  pag::EncodeStream content(&state->streamContext);
  PAGAttributeBlockWriter writer(&state->streamContext);
  auto hasUnitsPerEm = font->unitsPerEm != 1000;
  writer.writeFlag(hasUnitsPerEm);
  auto* v = writer.valueStream();
  if (hasUnitsPerEm) {
    v->writeEncodedUint32(static_cast<uint32_t>(font->unitsPerEm));
  }
  auto block = writer.release();
  content.writeBytes(const_cast<uint8_t*>(block->data()), static_cast<uint32_t>(block->length()));
  content.writeEncodedUint32(static_cast<uint32_t>(font->glyphs.size()));
  for (auto* glyph : font->glyphs) {
    auto hasPath = glyph->path != nullptr;
    auto hasImage = glyph->image != nullptr;
    uint8_t flags = static_cast<uint8_t>((hasPath ? 1 : 0) | (hasImage ? 2 : 0));
    content.writeUint8(flags);
    content.writeFloat(glyph->advance);
    content.writeFloat(glyph->offset.x);
    content.writeFloat(glyph->offset.y);
    if (hasImage) {
      uint32_t imageRef = 0;
      auto it = state->imageIndices.find(glyph->image);
      if (it != state->imageIndices.end()) {
        imageRef = it->second + 1;
      }
      content.writeEncodedUint32(imageRef);
    }
    if (hasPath) {
      WritePAGPath(&content, glyph->path, 0);
    }
  }
  WriteTag(stream, TagCode::Font, &content);
}

// GlyphRun tag (spec §9.2 #422, 11 exist bits + two Fixed columns). Glyph ids are bit-packed
// with a 5-bit numBits prefix (§3.4) and byte-aligned before the following values; the inline
// Font sub-tag sits at the value-area end (§9.0).
void WriteGlyphRunTag(pag::EncodeStream* stream, GlyphRun* run, EncodeState* state) {
  pag::EncodeStream content(&state->streamContext);
  auto hasFont = run->font != nullptr;
  uint32_t fontRef = 0;
  if (hasFont) {
    auto slot = state->fontResourceSlots.find(run->font);
    if (slot != state->fontResourceSlots.end()) {
      fontRef = slot->second + 1;
    }
  }
  auto glyphCount = static_cast<uint32_t>(run->glyphs.size());
  auto hasFontSize = run->fontSize != 12.0f;
  auto hasX = run->x != 0.0f;
  auto hasY = run->y != 0.0f;
  auto hasXOffsets = !run->xOffsets.empty();
  auto hasPositions = !run->positions.empty();
  auto hasAnchors = !run->anchors.empty();
  auto hasScales = !run->scales.empty();
  auto hasRotations = !run->rotations.empty();
  auto hasSkews = !run->skews.empty();
  auto hasBounds = run->bounds.width > 0 || run->bounds.height > 0;
  PAGAttributeBlockWriter writer(&state->streamContext);
  writer.writeFlag(hasFont);
  writer.writeFlag(hasFontSize);
  writer.writeFlag(hasX);
  writer.writeFlag(hasY);
  writer.writeFlag(hasXOffsets);
  writer.writeFlag(hasPositions);
  writer.writeFlag(hasAnchors);
  writer.writeFlag(hasScales);
  writer.writeFlag(hasRotations);
  writer.writeFlag(hasSkews);
  writer.writeFlag(hasBounds);
  auto* v = writer.valueStream();
  if (hasFont) {
    v->writeEncodedUint32(fontRef);
  }
  if (hasFontSize) {
    v->writeFloat(run->fontSize);
  }
  v->writeEncodedUint32(glyphCount);
  uint32_t numBits = 1;
  for (auto id : run->glyphs) {
    uint32_t length = 1;
    auto value = static_cast<uint32_t>(id);
    while ((value >>= 1) != 0) {
      length++;
    }
    if (length > numBits) {
      numBits = length;
    }
  }
  v->writeUBits(numBits - 1, 5);
  for (auto id : run->glyphs) {
    v->writeUBits(id, static_cast<uint8_t>(numBits));
  }
  v->alignWithBytes();
  if (hasX) {
    v->writeFloat(run->x);
  }
  if (hasY) {
    v->writeFloat(run->y);
  }
  if (hasXOffsets) {
    v->writeFloatList(run->xOffsets.data(), glyphCount, 0.05f);
    v->alignWithBytes();
  }
  if (hasPositions) {
    std::vector<pag::Point> points(glyphCount);
    for (uint32_t i = 0; i < glyphCount; i++) {
      points[i].x = run->positions[i].x;
      points[i].y = run->positions[i].y;
    }
    v->writePoint2DList(points.data(), glyphCount, 0.05f);
    v->alignWithBytes();
  }
  if (hasAnchors) {
    std::vector<pag::Point> points(glyphCount);
    for (uint32_t i = 0; i < glyphCount; i++) {
      points[i].x = run->anchors[i].x;
      points[i].y = run->anchors[i].y;
    }
    v->writePoint2DList(points.data(), glyphCount, 0.05f);
    v->alignWithBytes();
  }
  if (hasScales) {
    v->writeEncodedUint32(static_cast<uint32_t>(run->scales.size()));
    for (auto& scale : run->scales) {
      v->writeFloat(scale.x);
      v->writeFloat(scale.y);
    }
  }
  if (hasRotations) {
    v->writeEncodedUint32(static_cast<uint32_t>(run->rotations.size()));
    for (auto rotation : run->rotations) {
      v->writeFloat(rotation);
    }
  }
  if (hasSkews) {
    v->writeEncodedUint32(static_cast<uint32_t>(run->skews.size()));
    for (auto skew : run->skews) {
      v->writeFloat(skew);
    }
  }
  if (hasBounds) {
    v->writeFloat(run->bounds.x);
    v->writeFloat(run->bounds.y);
    v->writeFloat(run->bounds.width);
    v->writeFloat(run->bounds.height);
  }
  auto block = writer.release();
  content.writeBytes(const_cast<uint8_t*>(block->data()), static_cast<uint32_t>(block->length()));
  if (hasFont && fontRef == 0) {
    WriteFontTag(&content, run->font, state);
  }
  WriteTag(stream, TagCode::GlyphRun, &content);
}

void WriteTextTag(pag::EncodeStream* stream, Text* text, EncodeState* state) {
  pag::EncodeStream content(&state->streamContext);
  {
    PAGAttributeBlockWriter writer(&state->streamContext);
    writer.writeFlag(text->position.x != 0.0f);
    writer.writeFlag(text->position.y != 0.0f);
    writer.writeFlag(!std::isnan(text->width));
    writer.writeFlag(!std::isnan(text->height));
    writer.writeFlag(!text->text.empty());
    writer.writeFlag(!text->fontFamily.empty());
    writer.writeFlag(!text->fontStyle.empty());
    writer.writeFlag(text->fontSize != 12.0f);
    writer.writeFlag(text->letterSpacing != 0.0f);
    writer.writeFlag(text->fauxBold);
    writer.writeFlag(text->fauxItalic);
    writer.writeFlag(text->textAnchor != TextAnchor::Start);
    writer.writeFlag(text->baseline != TextBaseline::LineBox);
    auto* v = writer.valueStream();
    if (text->position.x != 0.0f) v->writeFloat(text->position.x);
    if (text->position.y != 0.0f) v->writeFloat(text->position.y);
    if (!std::isnan(text->width)) v->writeFloat(text->width);
    if (!std::isnan(text->height)) v->writeFloat(text->height);
    if (!text->text.empty()) v->writeEncodedUint32(state->intern(text->text));
    if (!text->fontFamily.empty()) v->writeEncodedUint32(state->intern(text->fontFamily));
    if (!text->fontStyle.empty()) v->writeEncodedUint32(state->intern(text->fontStyle));
    if (text->fontSize != 12.0f) v->writeFloat(text->fontSize);
    if (text->letterSpacing != 0.0f) v->writeFloat(text->letterSpacing);
    if (text->textAnchor != TextAnchor::Start) {
      v->writeUint8(static_cast<uint8_t>(text->textAnchor));
    }
    if (text->baseline != TextBaseline::LineBox) {
      v->writeUint8(static_cast<uint8_t>(text->baseline));
    }
    auto block = writer.release();
    WritePAGTagHeader(&content, TagCode::TextAttributes, static_cast<uint32_t>(block->length()));
    content.writeBytes(const_cast<uint8_t*>(block->data()), static_cast<uint32_t>(block->length()));
  }
  for (auto* run : text->glyphRuns) {
    WriteGlyphRunTag(&content, run, state);
  }
  WritePAGTagHeader(&content, TagCode::End, 0);
  WriteTag(stream, TagCode::TextBlock, &content);
}

void WriteElementTag(pag::EncodeStream* stream, Element* element, EncodeState* state);

void WriteGroupTag(pag::EncodeStream* stream, Group* group, EncodeState* state) {
  pag::EncodeStream content(&state->streamContext);
  {
    PAGAttributeBlockWriter writer(&state->streamContext);
    writer.writeFlag(group->anchor.x != 0.0f);
    writer.writeFlag(group->anchor.y != 0.0f);
    writer.writeFlag(group->position.x != 0.0f);
    writer.writeFlag(group->position.y != 0.0f);
    writer.writeFlag(group->rotation != 0.0f);
    writer.writeFlag(group->scale.x != 1.0f);
    writer.writeFlag(group->scale.y != 1.0f);
    writer.writeFlag(group->skew != 0.0f);
    writer.writeFlag(group->skewAxis != 0.0f);
    writer.writeFlag(group->alpha != 1.0f);
    writer.writeFlag(!std::isnan(group->width));
    writer.writeFlag(!std::isnan(group->height));
    auto* v = writer.valueStream();
    if (group->anchor.x != 0.0f) v->writeFloat(group->anchor.x);
    if (group->anchor.y != 0.0f) v->writeFloat(group->anchor.y);
    if (group->position.x != 0.0f) v->writeFloat(group->position.x);
    if (group->position.y != 0.0f) v->writeFloat(group->position.y);
    if (group->rotation != 0.0f) v->writeFloat(group->rotation);
    if (group->scale.x != 1.0f) v->writeFloat(group->scale.x);
    if (group->scale.y != 1.0f) v->writeFloat(group->scale.y);
    if (group->skew != 0.0f) v->writeFloat(group->skew);
    if (group->skewAxis != 0.0f) v->writeFloat(group->skewAxis);
    if (group->alpha != 1.0f) {
      v->writeUint8(static_cast<uint8_t>(lroundf(group->alpha * 255.0f)));
    }
    if (!std::isnan(group->width)) v->writeFloat(group->width);
    if (!std::isnan(group->height)) v->writeFloat(group->height);
    auto block = writer.release();
    WritePAGTagHeader(&content, TagCode::GroupAttributes, static_cast<uint32_t>(block->length()));
    content.writeBytes(const_cast<uint8_t*>(block->data()), static_cast<uint32_t>(block->length()));
  }
  for (auto* element : group->elements) {
    WriteElementTag(&content, element, state);
  }
  WritePAGTagHeader(&content, TagCode::End, 0);
  WriteTag(stream, TagCode::GroupBlock, &content);
}

void WriteElementTag(pag::EncodeStream* stream, Element* element, EncodeState* state) {
  if (element == nullptr) {
    return;
  }
  switch (element->nodeType()) {
    case NodeType::Rectangle:
      WriteRectangleTag(stream, static_cast<Rectangle*>(element), state);
      break;
    case NodeType::Ellipse:
      WriteEllipseTag(stream, static_cast<Ellipse*>(element), state);
      break;
    case NodeType::Path:
      WritePathTag(stream, static_cast<Path*>(element), state);
      break;
    case NodeType::Text:
      WriteTextTag(stream, static_cast<Text*>(element), state);
      break;
    case NodeType::Fill:
      WriteFillTag(stream, static_cast<Fill*>(element), state);
      break;
    case NodeType::Stroke:
      WriteStrokeTag(stream, static_cast<Stroke*>(element), state);
      break;
    case NodeType::Group:
      WriteGroupTag(stream, static_cast<Group*>(element), state);
      break;
    default:
      break;
  }
}

void WriteLayerTag(pag::EncodeStream* stream, Layer* layer, EncodeState* state) {
  pag::EncodeStream content(&state->streamContext);
  {
    PAGAttributeBlockWriter writer(&state->streamContext);
    auto hasName = !layer->name.empty();
    auto hasX = layer->x != 0.0f;
    auto hasY = layer->y != 0.0f;
    auto notVisible = layer->visible != true;
    auto hasAlpha = layer->alpha != 1.0f;
    auto hasBlend = layer->blendMode != BlendMode::Normal;
    auto hasMatrix = !layer->matrix.isIdentity();
    auto hasMatrix3D = !layer->matrix3D.isIdentity();
    auto hasAntiAlias = layer->antiAlias != true;
    auto hasGroupOpacity = layer->groupOpacity != true;
    auto hasPassThrough = layer->passThroughBackground != true;
    auto hasWidth = !std::isnan(layer->width);
    auto hasHeight = !std::isnan(layer->height);
    auto hasScrollRect = layer->scrollRect.width > 0 || layer->scrollRect.height > 0;
    auto hasMask = layer->mask != nullptr;
    auto hasMaskType = layer->maskType != MaskType::Alpha;
    auto hasVmContext = !layer->vmContext.empty();
    writer.writeFlag(hasName);
    writer.writeFlag(hasX);
    writer.writeFlag(hasY);
    writer.writeFlag(notVisible);
    writer.writeFlag(hasAlpha);
    writer.writeFlag(hasBlend);
    writer.writeFlag(hasMatrix);
    writer.writeFlag(hasMatrix3D);
    writer.writeFlag(layer->preserve3D);  // BitFlag (default false): the bit is the value.
    writer.writeFlag(hasAntiAlias);
    writer.writeFlag(hasGroupOpacity);
    writer.writeFlag(hasPassThrough);
    writer.writeFlag(hasWidth);
    writer.writeFlag(hasHeight);
    writer.writeFlag(hasScrollRect);
    writer.writeFlag(hasMask);
    writer.writeFlag(hasMaskType);
    writer.writeFlag(false);  // hasComposition, phase 2
    writer.writeFlag(false);  // hasSubDocument, phase 2
    writer.writeFlag(hasVmContext);
    auto* v = writer.valueStream();
    if (hasName) v->writeEncodedUint32(state->intern(layer->name));
    if (hasX) v->writeFloat(layer->x);
    if (hasY) v->writeFloat(layer->y);
    if (notVisible) v->writeBoolean(layer->visible);
    if (hasAlpha) v->writeUint8(static_cast<uint8_t>(lroundf(layer->alpha * 255.0f)));
    if (hasBlend) v->writeUint8(static_cast<uint8_t>(layer->blendMode));
    if (hasMatrix) WritePAGMatrix(v, layer->matrix);
    if (hasMatrix3D) WritePAGMatrix3D(v, layer->matrix3D);
    if (hasAntiAlias) v->writeBoolean(layer->antiAlias);
    if (hasGroupOpacity) v->writeBoolean(layer->groupOpacity);
    if (hasPassThrough) v->writeBoolean(layer->passThroughBackground);
    if (hasWidth) v->writeFloat(layer->width);
    if (hasHeight) v->writeFloat(layer->height);
    if (hasScrollRect) {
      v->writeFloat(layer->scrollRect.x);
      v->writeFloat(layer->scrollRect.y);
      v->writeFloat(layer->scrollRect.width);
      v->writeFloat(layer->scrollRect.height);
    }
    if (hasMask) v->writeEncodedUint32(0);  // NodeRef, phase 1 always none
    if (hasMaskType) v->writeUint8(static_cast<uint8_t>(layer->maskType));
    if (hasVmContext) v->writeEncodedUint32(state->intern(layer->vmContext));
    auto block = writer.release();
    WritePAGTagHeader(&content, TagCode::LayerAttributes, static_cast<uint32_t>(block->length()));
    content.writeBytes(const_cast<uint8_t*>(block->data()), static_cast<uint32_t>(block->length()));
  }
  for (auto* element : layer->contents) {
    WriteElementTag(&content, element, state);
  }
  for (auto* child : layer->children) {
    WriteLayerTag(&content, child, state);
  }
  WritePAGTagHeader(&content, TagCode::End, 0);
  WriteTag(stream, TagCode::LayerBlock, &content);
}

}  // namespace

PAGExportResult PAGExporter::ExportToPAG(std::shared_ptr<PAGXDocument> document,
                                         const std::string& filePath,
                                         const PAGExportOptions& options) {
  PAGExportResult result = {};
  if (document == nullptr) {
    return result;
  }
  // The internal XML round trip links GlyphRun -> Font by resource id, but documents decoded
  // from the v4 binary carry no ids (the binary keeps node identity by reference). Synthesize
  // deterministic ids on id-less fonts so the XML hop preserves the linkage, and restore the
  // original ids afterwards; StripIds clears them in the working copy anyway, so the written
  // bytes do not depend on the synthesized names.
  std::vector<std::pair<Font*, std::string>> restoredFontIds = {};
  std::unordered_set<std::string> takenIds = {};
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Font) {
      auto* font = static_cast<Font*>(node.get());
      if (!font->id.empty()) {
        takenIds.insert(font->id);
      }
    }
  }
  int synthIndex = 0;
  for (auto& node : document->nodes) {
    if (node->nodeType() != NodeType::Font) {
      continue;
    }
    auto* font = static_cast<Font*>(node.get());
    if (!font->id.empty()) {
      continue;
    }
    restoredFontIds.push_back({font, font->id});
    do {
      font->id = "pag-v4-font-" + std::to_string(synthIndex++);
    } while (takenIds.count(font->id) > 0);
    takenIds.insert(font->id);
  }
  auto xml = PAGXExporter::ToXML(*document);
  for (auto& restore : restoredFontIds) {
    restore.first->id = restore.second;
  }
  auto doc = PAGXImporter::FromXML(xml);
  if (doc == nullptr) {
    return result;
  }
  doc->applyLayout();
  LayoutFlattener::Flatten(doc.get());
  auto retainedIds = Stripper::CollectRetainedIds(doc.get());
  Stripper::StripMetadata(doc.get());
  Stripper::StripIds(doc.get(), retainedIds);
  Stripper::RemoveDeadNodes(doc.get(), retainedIds);
  auto imageResult = ImageProcessor::Process(doc.get());
  result.warnings.insert(result.warnings.end(), imageResult.warnings.begin(),
                         imageResult.warnings.end());
  if (!doc->animations.empty() || !doc->dataBinds.empty() || doc->viewModel != nullptr) {
    PAGExportIssue issue = {};
    issue.code = PAGExportWarning::AnimatedContentSkipped;
    issue.message = "animated content skipped in phase 1";
    result.warnings.push_back(issue);
  }
  auto state = std::make_unique<EncodeState>();
  state->imageEntries = std::move(imageResult.entries);
  state->imageIndices = std::move(imageResult.imageIndices);
  // Shared-image placement (spec §5.3): count references per ImageTable entry; entries
  // referenced by two or more patterns go into the ResourceBlock (first-reference order),
  // single-use entries are inlined at the referencing pattern's value-area end.
  {
    std::vector<Image*> imageRefs = {};
    for (auto* layer : doc->layers) {
      CollectImageReferencesFromLayer(layer, &imageRefs);
    }
    std::vector<Image*> entryFirstNode = {};
    std::vector<int> entryRefCount = {};
    for (auto* image : imageRefs) {
      auto it = state->imageIndices.find(image);
      if (it == state->imageIndices.end()) {
        continue;
      }
      if (it->second >= entryFirstNode.size()) {
        entryFirstNode.resize(it->second + 1, nullptr);
        entryRefCount.resize(it->second + 1, 0);
      }
      if (entryFirstNode[it->second] == nullptr) {
        entryFirstNode[it->second] = image;
      }
      entryRefCount[it->second]++;
    }
    for (auto* image : imageRefs) {
      auto it = state->imageIndices.find(image);
      if (it == state->imageIndices.end()) {
        continue;
      }
      if (entryRefCount[it->second] >= 2 &&
          state->entryResourceSlots.find(it->second) == state->entryResourceSlots.end()) {
        state->entryResourceSlots[it->second] =
            static_cast<uint32_t>(state->sharedImageOrder.size());
        state->sharedImageOrder.push_back(entryFirstNode[it->second]);
      }
    }
  }
  // Shared-font placement (spec §5.3): count glyph-run references per font node; fonts
  // referenced by two or more runs go into the ResourceBlock (first-reference order), single-use
  // fonts are inlined at the referencing run's value-area end.
  {
    std::vector<Font*> fontRefs = {};
    for (auto* layer : doc->layers) {
      CollectFontReferencesFromLayer(layer, &fontRefs);
    }
    std::unordered_map<const Font*, int> fontCounts = {};
    for (auto* font : fontRefs) {
      fontCounts[font]++;
    }
    for (auto* font : fontRefs) {
      if (fontCounts[font] >= 2 &&
          state->fontResourceSlots.find(font) == state->fontResourceSlots.end()) {
        state->fontResourceSlots[font] = static_cast<uint32_t>(state->sharedFontOrder.size());
        state->sharedFontOrder.push_back(font);
      }
    }
  }
  for (auto* layer : doc->layers) {
    CollectStringsFromLayer(layer, state.get());
  }
  pag::EncodeStream body(&state->streamContext);
  WritePAGTagHeader(&body, TagCode::FileAttributes, 0);
  {
    pag::EncodeStream strContent(&state->streamContext);
    WritePAGStringTable(&strContent, state->strings.strings);
    WriteTag(&body, TagCode::StringTable, &strContent);
  }
  {
    pag::EncodeStream resContent(&state->streamContext);
    for (auto* image : state->sharedImageOrder) {
      WriteImageTag(&resContent, image, state.get());
    }
    for (auto* font : state->sharedFontOrder) {
      WriteFontTag(&resContent, font, state.get());
    }
    WritePAGTagHeader(&resContent, TagCode::End, 0);
    WriteTag(&body, TagCode::ResourceBlock, &resContent);
  }
  {
    pag::EncodeStream docContent(&state->streamContext);
    {
      PAGAttributeBlockWriter writer(&state->streamContext);
      writer.writeFlag(true);
      writer.writeFlag(true);
      writer.writeFlag(false);
      writer.writeFlag(false);
      auto* v = writer.valueStream();
      v->writeFloat(doc->width);
      v->writeFloat(doc->height);
      auto block = writer.release();
      WritePAGTagHeader(&docContent, TagCode::DocumentAttributes,
                        static_cast<uint32_t>(block->length()));
      docContent.writeBytes(const_cast<uint8_t*>(block->data()),
                            static_cast<uint32_t>(block->length()));
    }
    {
      pag::EncodeStream idContent(&state->streamContext);
      idContent.writeEncodedUint32(0);
      WriteTag(&docContent, TagCode::RetainedIdTable, &idContent);
    }
    for (auto* layer : doc->layers) {
      WriteLayerTag(&docContent, layer, state.get());
    }
    WritePAGTagHeader(&docContent, TagCode::End, 0);
    WriteTag(&body, TagCode::DocumentBlock, &docContent);
  }
  {
    pag::EncodeStream imgContent(&state->streamContext);
    imgContent.writeEncodedUint32(static_cast<uint32_t>(state->imageEntries.size()));
    for (auto& entry : state->imageEntries) {
      imgContent.writeUint8(entry.mediaType);
      if (entry.data != nullptr && !entry.data->empty()) {
        imgContent.writeEncodedUint32(static_cast<uint32_t>(entry.data->size()));
        imgContent.writeBytes(const_cast<uint8_t*>(entry.data->bytes()),
                              static_cast<uint32_t>(entry.data->size()));
      } else {
        imgContent.writeEncodedUint32(0);
      }
      imgContent.writeUint32(entry.logicalWidth);
      imgContent.writeUint32(entry.logicalHeight);
      imgContent.writeFloat(entry.scaleFactor);
      imgContent.writeFloat(entry.offsetX);
      imgContent.writeFloat(entry.offsetY);
    }
    WriteTag(&body, TagCode::ImageTable, &imgContent);
  }
  WritePAGTagHeader(&body, TagCode::End, 0);
  auto bodyData = body.release();
  auto bodyLength = static_cast<uint32_t>(bodyData->length());
  if (bodyLength == 0) {
    return result;
  }
  pag::EncodeStream file(&state->streamContext);
  WritePAGFileHeader(&file, bodyLength);
  file.writeBytes(const_cast<uint8_t*>(static_cast<const uint8_t*>(bodyData->data())), bodyLength);
  auto fileData = file.release();
  std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    return result;
  }
  out.write(reinterpret_cast<const char*>(fileData->data()),
            static_cast<std::streamsize>(fileData->length()));
  out.close();
  result.success = !options.strict || result.warnings.empty();
  result.byteSize = fileData->length();
  return result;
}

}  // namespace pagx
