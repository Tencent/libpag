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
#include <cstdio>
#include <fstream>
#include <unordered_map>
#include "pagx/FontRegistration.h"
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
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/MergePath.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/Rectangle.h"
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

// SubDocumentTable entry (spec §6.5): a working-copy document plus its diagnostic name.
struct SubDocumentEntry {
  std::string name = {};
  std::shared_ptr<PAGXDocument> document = nullptr;
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
  // SubDocumentTable entries (spec §6.5) in first-reference order. Content is the identity
  // (the instance's deterministic XML), so multiple references — and a decode -> re-encode
  // hop — map to one entry and the bytes stay stable.
  std::vector<SubDocumentEntry> subDocuments;
  std::unordered_map<std::string, uint32_t> subDocIndexByKey;
  // Marks layers that reference a sub-document entry (root clone layers and template layers
  // alike) with the table index to write into LayerAttributes' hasSubDocument/subDocRef.
  std::unordered_map<const Layer*, uint32_t> layerSubDocRefs;
  // Expansion accounting (spec §11): every layer that references a sub-document counts as one
  // runtime instance and adds the instance's node count.
  int subDocInstanceCount = 0;
  int subDocNodeCount = 0;
  // Spec §5.1 node indices in emission order, backing NodeRef fields (Layer.mask).
  std::unordered_map<Node*, uint32_t> nodeIndices;
  // Shared-composition placement (spec §5.3): compositions referenced by two or more layers go
  // into the ResourceBlock in first-reference order; single-use compositions are inlined as a
  // CompositionBlock right after the referencing layer's LayerAttributes.
  std::vector<Composition*> sharedCompositionOrder;
  std::unordered_map<const Composition*, uint32_t> compositionResourceSlots;

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

// Collects composition references from layers (spec §5.3 first-reference order). Every
// reference is appended, including repeats.
void CollectCompositionReferencesFromLayer(Layer* layer, std::vector<Composition*>* references) {
  if (layer->composition != nullptr) {
    references->push_back(layer->composition);
  }
  for (auto* child : layer->children) {
    CollectCompositionReferencesFromLayer(child, references);
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

// Polystar tag (spec §9.3 #418): 10 exist bits, position defaults NaN.
void WritePolystarTag(pag::EncodeStream* stream, Polystar* polystar, EncodeState* state) {
  PAGAttributeBlockWriter writer(&state->streamContext);
  auto hasPosX = !std::isnan(polystar->position.x);
  auto hasPosY = !std::isnan(polystar->position.y);
  writer.writeFlag(hasPosX);
  writer.writeFlag(hasPosY);
  writer.writeFlag(polystar->type != PolystarType::Star);
  writer.writeFlag(polystar->pointCount != 5.0f);
  writer.writeFlag(polystar->outerRadius != 100.0f);
  writer.writeFlag(polystar->innerRadius != 50.0f);
  writer.writeFlag(polystar->rotation != 0.0f);
  writer.writeFlag(polystar->outerRoundness != 0.0f);
  writer.writeFlag(polystar->innerRoundness != 0.0f);
  writer.writeFlag(polystar->reversed);
  auto* v = writer.valueStream();
  if (hasPosX) v->writeFloat(polystar->position.x);
  if (hasPosY) v->writeFloat(polystar->position.y);
  if (polystar->type != PolystarType::Star) {
    v->writeUint8(static_cast<uint8_t>(polystar->type));
  }
  if (polystar->pointCount != 5.0f) v->writeFloat(polystar->pointCount);
  if (polystar->outerRadius != 100.0f) v->writeFloat(polystar->outerRadius);
  if (polystar->innerRadius != 50.0f) v->writeFloat(polystar->innerRadius);
  if (polystar->rotation != 0.0f) v->writeFloat(polystar->rotation);
  if (polystar->outerRoundness != 0.0f) v->writeFloat(polystar->outerRoundness);
  if (polystar->innerRoundness != 0.0f) v->writeFloat(polystar->innerRoundness);
  auto block = writer.release();
  WritePAGTagHeader(stream, TagCode::Polystar, static_cast<uint32_t>(block->length()));
  stream->writeBytes(const_cast<uint8_t*>(block->data()), static_cast<uint32_t>(block->length()));
}

// TrimPath tag (spec §9.3 #425).
void WriteTrimPathTag(pag::EncodeStream* stream, TrimPath* trim, EncodeState* state) {
  PAGAttributeBlockWriter writer(&state->streamContext);
  writer.writeFlag(trim->start != 0.0f);
  writer.writeFlag(trim->end != 1.0f);
  writer.writeFlag(trim->offset != 0.0f);
  writer.writeFlag(trim->type != TrimType::Separate);
  auto* v = writer.valueStream();
  if (trim->start != 0.0f) v->writeFloat(trim->start);
  if (trim->end != 1.0f) v->writeFloat(trim->end);
  if (trim->offset != 0.0f) v->writeFloat(trim->offset);
  if (trim->type != TrimType::Separate) v->writeUint8(static_cast<uint8_t>(trim->type));
  auto block = writer.release();
  WritePAGTagHeader(stream, TagCode::TrimPath, static_cast<uint32_t>(block->length()));
  stream->writeBytes(const_cast<uint8_t*>(block->data()), static_cast<uint32_t>(block->length()));
}

// RoundCorner tag (spec §9.3 #426).
void WriteRoundCornerTag(pag::EncodeStream* stream, RoundCorner* round, EncodeState* state) {
  PAGAttributeBlockWriter writer(&state->streamContext);
  writer.writeFlag(round->radius != 10.0f);
  auto* v = writer.valueStream();
  if (round->radius != 10.0f) v->writeFloat(round->radius);
  auto block = writer.release();
  WritePAGTagHeader(stream, TagCode::RoundCorner, static_cast<uint32_t>(block->length()));
  stream->writeBytes(const_cast<uint8_t*>(block->data()), static_cast<uint32_t>(block->length()));
}

// MergePath tag (spec §9.3 #427).
void WriteMergePathTag(pag::EncodeStream* stream, MergePath* merge, EncodeState* state) {
  PAGAttributeBlockWriter writer(&state->streamContext);
  writer.writeFlag(merge->mode != MergePathMode::Append);
  auto* v = writer.valueStream();
  if (merge->mode != MergePathMode::Append) {
    v->writeUint8(static_cast<uint8_t>(merge->mode));
  }
  auto block = writer.release();
  WritePAGTagHeader(stream, TagCode::MergePath, static_cast<uint32_t>(block->length()));
  stream->writeBytes(const_cast<uint8_t*>(block->data()), static_cast<uint32_t>(block->length()));
}

// TextPath tag (spec §9.3 #430): 9 exist bits; the inline PathData sub-tag sits at the
// value-area end.
void WriteTextPathTag(pag::EncodeStream* stream, TextPath* textPath, EncodeState* state) {
  PAGAttributeBlockWriter writer(&state->streamContext);
  auto hasPath = textPath->path != nullptr;
  writer.writeFlag(hasPath);
  writer.writeFlag(textPath->baselineOrigin.x != 0.0f);
  writer.writeFlag(textPath->baselineOrigin.y != 0.0f);
  writer.writeFlag(textPath->baselineAngle != 0.0f);
  writer.writeFlag(textPath->firstMargin != 0.0f);
  writer.writeFlag(textPath->lastMargin != 0.0f);
  writer.writeFlag(!textPath->perpendicular);
  writer.writeFlag(textPath->reversed);
  writer.writeFlag(textPath->forceAlignment);
  auto* v = writer.valueStream();
  if (hasPath) {
    v->writeEncodedUint32(0);  // 0 = inline path data
  }
  if (textPath->baselineOrigin.x != 0.0f) v->writeFloat(textPath->baselineOrigin.x);
  if (textPath->baselineOrigin.y != 0.0f) v->writeFloat(textPath->baselineOrigin.y);
  if (textPath->baselineAngle != 0.0f) v->writeFloat(textPath->baselineAngle);
  if (textPath->firstMargin != 0.0f) v->writeFloat(textPath->firstMargin);
  if (textPath->lastMargin != 0.0f) v->writeFloat(textPath->lastMargin);
  if (!textPath->perpendicular) v->writeBoolean(textPath->perpendicular);
  auto block = writer.release();
  pag::EncodeStream content(&state->streamContext);
  content.writeBytes(const_cast<uint8_t*>(block->data()), static_cast<uint32_t>(block->length()));
  if (hasPath) {
    pag::EncodeStream pathTag(&state->streamContext);
    WritePAGPath(&pathTag, textPath->path, 0);
    WriteTag(&content, TagCode::PathData, &pathTag);
  }
  WriteTag(stream, TagCode::TextPath, &content);
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
    case NodeType::Polystar:
      WritePolystarTag(stream, static_cast<Polystar*>(element), state);
      break;
    case NodeType::TrimPath:
      WriteTrimPathTag(stream, static_cast<TrimPath*>(element), state);
      break;
    case NodeType::RoundCorner:
      WriteRoundCornerTag(stream, static_cast<RoundCorner*>(element), state);
      break;
    case NodeType::MergePath:
      WriteMergePathTag(stream, static_cast<MergePath*>(element), state);
      break;
    case NodeType::TextPath:
      WriteTextPathTag(stream, static_cast<TextPath*>(element), state);
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

void WriteLayerTag(pag::EncodeStream* stream, Layer* layer, EncodeState* state);

// CompositionBlock (spec §9.1 #297): CompositionAttributes (298: width/height) followed by
// the composition's layer tags, End-terminated.
void WriteCompositionBlock(pag::EncodeStream* stream, Composition* composition,
                           EncodeState* state) {
  pag::EncodeStream content(&state->streamContext);
  {
    PAGAttributeBlockWriter writer(&state->streamContext);
    writer.writeFlag(true);   // width
    writer.writeFlag(true);   // height
    writer.writeFlag(false);  // phase-2 fields
    writer.writeFlag(false);
    auto* v = writer.valueStream();
    v->writeFloat(composition->width);
    v->writeFloat(composition->height);
    auto block = writer.release();
    WritePAGTagHeader(&content, TagCode::DocumentAttributes,
                      static_cast<uint32_t>(block->length()));
    content.writeBytes(const_cast<uint8_t*>(block->data()), static_cast<uint32_t>(block->length()));
  }
  for (auto* compLayer : composition->layers) {
    WriteLayerTag(&content, compLayer, state);
  }
  WritePAGTagHeader(&content, TagCode::End, 0);
  WriteTag(stream, TagCode::CompositionBlock, &content);
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
    writer.writeFlag(layer->composition != nullptr);
    auto subDocIt = state->layerSubDocRefs.find(layer);
    auto hasSubDocument = subDocIt != state->layerSubDocRefs.end();
    writer.writeFlag(hasSubDocument);
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
    if (hasMask) {
      // NodeRef (spec §5.2): k = node index k-1 in this document's emission order.
      uint32_t maskRef = 0;
      auto maskIt = state->nodeIndices.find(layer->mask);
      if (maskIt != state->nodeIndices.end()) {
        maskRef = maskIt->second + 1;
      }
      v->writeEncodedUint32(maskRef);
    }
    if (layer->composition != nullptr) {
      // ResourceRef: 0 = inline CompositionBlock after LayerAttributes, k = ResourceBlock k-1.
      uint32_t compositionRef = 0;
      auto compIt = state->compositionResourceSlots.find(layer->composition);
      if (compIt != state->compositionResourceSlots.end()) {
        compositionRef = compIt->second + 1;
      }
      v->writeEncodedUint32(compositionRef);
    }
    if (hasMaskType) v->writeUint8(static_cast<uint8_t>(layer->maskType));
    if (hasSubDocument) v->writeEncodedUint32(subDocIt->second);
    if (hasVmContext) v->writeEncodedUint32(state->intern(layer->vmContext));
    auto block = writer.release();
    WritePAGTagHeader(&content, TagCode::LayerAttributes, static_cast<uint32_t>(block->length()));
    content.writeBytes(const_cast<uint8_t*>(block->data()), static_cast<uint32_t>(block->length()));
  }
  if (layer->composition != nullptr && state->compositionResourceSlots.find(layer->composition) ==
                                           state->compositionResourceSlots.end()) {
    WriteCompositionBlock(&content, layer->composition, state);
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

// True when any Font node carries a file declaration, meaning export-time text measurement
// should register them before laying out.
bool DocumentDeclaresFontFiles(PAGXDocument* document) {
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Font && !static_cast<Font*>(node.get())->file.empty()) {
      return true;
    }
  }
  return false;
}

// Counts the layers of the given document that reference a sub-document entry.
int CountSubDocumentReferences(PAGXDocument* document,
                               const std::unordered_map<const Layer*, uint32_t>& layerRefs) {
  int count = 0;
  for (auto* layer : document->layers) {
    if (layerRefs.count(layer) > 0) {
      count++;
    }
  }
  return count;
}

// Counts the Image nodes owned by the document.
int CountImageNodes(PAGXDocument* document) {
  int count = 0;
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Image) {
      count++;
    }
  }
  return count;
}

std::string JsonEscape(const std::string& text) {
  std::string escaped = {};
  escaped.reserve(text.size());
  for (char ch : text) {
    if (ch == '"' || ch == '\\') {
      escaped += '\\';
      escaped += ch;
    } else if (static_cast<unsigned char>(ch) < 0x20) {
      char buffer[8] = {};
      snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned char>(ch));
      escaped += buffer;
    } else {
      escaped += ch;
    }
  }
  return escaped;
}

std::string WarningCodeName(PAGExportWarning code) {
  switch (code) {
    case PAGExportWarning::MissingFont:
      return "MissingFont";
    case PAGExportWarning::MissingImage:
      return "MissingImage";
    case PAGExportWarning::MissingSubDocument:
      return "MissingSubDocument";
    case PAGExportWarning::UnsupportedURL:
      return "UnsupportedURL";
    case PAGExportWarning::AnimatedContentSkipped:
      return "AnimatedContentSkipped";
    case PAGExportWarning::LayoutChannelDropped:
      return "LayoutChannelDropped";
    case PAGExportWarning::MixedEmbeddedText:
      return "MixedEmbeddedText";
    case PAGExportWarning::EmbeddedTextBound:
      return "EmbeddedTextBound";
    case PAGExportWarning::InvalidAnimationTiming:
      return "InvalidAnimationTiming";
    case PAGExportWarning::NonFiniteValue:
      return "NonFiniteValue";
    case PAGExportWarning::ImageScaledDown:
      return "ImageScaledDown";
    case PAGExportWarning::UnsupportedColorSource:
      return "UnsupportedColorSource";
    case PAGExportWarning::SubDocumentExpansionLimit:
      return "SubDocumentExpansionLimit";
  }
  return "Unknown";
}

// Writes the deterministic export report (interface draft §4). No timestamps or other
// machine-varying values: two exports of the same input produce identical bytes.
void WriteExportReport(const PAGExportOptions& options, const PAGExportResult& result,
                       PAGXDocument* clone, const EncodeState& state, const std::string& xml,
                       const std::string& outputFilePath) {
  std::string json = "{\n";
  json += "  \"input\": \"" + JsonEscape(options.inputPath) + "\",\n";
  json += "  \"output\": \"" + JsonEscape(outputFilePath) + "\",\n";
  json += std::string("  \"success\": ") + (result.success ? "true" : "false") + ",\n";
  json += "  \"byteSize\": " + std::to_string(result.byteSize) + ",\n";
  json += "  \"xmlSize\": " + std::to_string(xml.size()) + ",\n";
  char ratio[32] = {};
  snprintf(
      ratio, sizeof(ratio), "%.3f",
      xml.empty() ? 0.0 : static_cast<double>(result.byteSize) / static_cast<double>(xml.size()));
  json += std::string("  \"compressionRatio\": ") + ratio + ",\n";
  json += "  \"documents\": [\n";
  std::vector<std::string> documentLines = {};
  {
    char line[512] = {};
    snprintf(line, sizeof(line),
             "    { \"name\": \"main\", \"layers\": %zu, \"nodes\": %zu, \"images\": %d, "
             "\"subDocuments\": %d }",
             clone->layers.size(), clone->nodes.size(), CountImageNodes(clone),
             CountSubDocumentReferences(clone, state.layerSubDocRefs));
    documentLines.push_back(line);
  }
  for (auto& sub : state.subDocuments) {
    char line[512] = {};
    snprintf(line, sizeof(line),
             "    { \"name\": \"%s\", \"layers\": %zu, \"nodes\": %zu, \"images\": %d, "
             "\"subDocuments\": %d }",
             JsonEscape(sub.name).c_str(), sub.document->layers.size(), sub.document->nodes.size(),
             CountImageNodes(sub.document.get()),
             CountSubDocumentReferences(sub.document.get(), state.layerSubDocRefs));
    documentLines.push_back(line);
  }
  for (size_t i = 0; i < documentLines.size(); i++) {
    json += documentLines[i];
    json += (i + 1 < documentLines.size()) ? ",\n" : "\n";
  }
  json += "  ],\n";
  json += "  \"warnings\": [\n";
  for (size_t i = 0; i < result.warnings.size(); i++) {
    auto& issue = result.warnings[i];
    json += "    { \"code\": \"" + WarningCodeName(issue.code) + "\", \"scope\": \"main\", ";
    json += "\"nodePath\": \"\", \"message\": \"" + JsonEscape(issue.message) + "\" }";
    json += (i + 1 < result.warnings.size()) ? ",\n" : "\n";
  }
  json += "  ],\n";
  // Required fonts, sorted for determinism. A font is "embedded" when every text using it
  // carries pre-shaped glyph runs (rendering no longer depends on the host's fonts).
  auto requiredFonts = clone->getRequiredFonts();
  std::sort(requiredFonts.begin(), requiredFonts.end());
  json += "  \"fonts\": [\n";
  std::vector<std::string> fontLines = {};
  for (auto& font : requiredFonts) {
    bool embedded = false;
    bool anyText = false;
    for (auto& node : clone->nodes) {
      if (node->nodeType() != NodeType::Text) {
        continue;
      }
      auto* text = static_cast<Text*>(node.get());
      if (text->fontFamily != font.fontFamily) {
        continue;
      }
      anyText = true;
      if (text->glyphRuns.empty()) {
        embedded = false;
        break;
      }
      embedded = true;
    }
    if (!anyText) {
      embedded = false;
    }
    fontLines.push_back("    { \"family\": \"" + JsonEscape(font.fontFamily) + "\", \"style\": \"" +
                        JsonEscape(font.fontStyle) + "\", \"source\": \"" +
                        (embedded ? "embedded" : "external") + "\" }");
  }
  for (size_t i = 0; i < fontLines.size(); i++) {
    json += fontLines[i];
    json += (i + 1 < fontLines.size()) ? ",\n" : "\n";
  }
  json += "  ],\n";
  json += "  \"skipped\": []\n";
  json += "}\n";
  std::ofstream out(options.reportPath, std::ios::binary | std::ios::trunc);
  if (out.is_open()) {
    out.write(json.data(), static_cast<std::streamsize>(json.size()));
    out.close();
  }
}

// Interns one external sub-document instance into the table. Nested references inside the
// instance are interned depth-first so the table keeps first-reference order. refererName is
// the referencing layer's composition path (XML source) or the instance's own diagnostic name
// (v4 source); the v4 name wins when present so re-export writes the same bytes.
uint32_t InternSubDocument(const std::shared_ptr<PAGXDocument>& instance,
                           const std::string& refererName, EncodeState* state,
                           PAGExportResult* result) {
  auto xmlKey = PAGXExporter::ToXML(*instance);
  auto existing = state->subDocIndexByKey.find(xmlKey);
  if (existing != state->subDocIndexByKey.end()) {
    return existing->second;
  }
  auto templateDoc = PAGXImporter::FromXML(xmlKey);
  if (templateDoc == nullptr) {
    // Unreachable in practice (the instance serializes fine by construction); keep the table
    // aligned with a stub entry so later indices stay correct.
    PAGExportIssue issue = {};
    issue.code = PAGExportWarning::MissingSubDocument;
    issue.message = "sub-document working copy failed to parse";
    result->warnings.push_back(issue);
    templateDoc = PAGXDocument::Make(0, 0);
  }
  auto index = static_cast<uint32_t>(state->subDocuments.size());
  state->subDocIndexByKey[xmlKey] = index;
  SubDocumentEntry entry = {};
  entry.name = !instance->diagnosticName.empty() ? instance->diagnosticName : refererName;
  entry.document = templateDoc;
  state->subDocuments.push_back(std::move(entry));
  // The template's layers mirror the instance's layers in order, so zip-walk and intern the
  // instance's own nested references depth-first.
  auto count = std::min(instance->layers.size(), templateDoc->layers.size());
  for (size_t i = 0; i < count; i++) {
    auto* srcLayer = instance->layers[i];
    auto* templateLayer = templateDoc->layers[i];
    if (srcLayer->externalDoc == nullptr) {
      continue;
    }
    auto nested =
        InternSubDocument(srcLayer->externalDoc, srcLayer->compositionFilePath, state, result);
    if (nested == UINT32_MAX) {
      continue;
    }
    state->layerSubDocRefs[templateLayer] = nested;
    state->subDocInstanceCount++;
    state->subDocNodeCount += static_cast<int>(srcLayer->externalDoc->nodes.size());
    // Attach the nested template once so layout and image collection reach it through the
    // wrapper composition.
    if (templateLayer->externalDoc == nullptr) {
      PAGXDocument::AttachExternalDocument(templateDoc.get(), templateLayer,
                                           state->subDocuments[nested].document);
    }
  }
  return index;
}

// Collects the input document's loaded external sub-documents (design §4.8), marks the clone's
// layers with their table indices, and attaches the first referencing clone layer per entry so
// applyLayout and ImageProcessor reach the templates. Layers whose external file never loaded
// warn and stay in the runtime's empty "unloaded" state.
void CollectSubDocumentReferences(PAGXDocument* input, PAGXDocument* clone, EncodeState* state,
                                  PAGExportResult* result) {
  auto count = std::min(input->layers.size(), clone->layers.size());
  for (size_t i = 0; i < count; i++) {
    auto* src = input->layers[i];
    auto* dst = clone->layers[i];
    if (src->externalDoc != nullptr) {
      auto index = InternSubDocument(src->externalDoc, src->compositionFilePath, state, result);
      if (index == UINT32_MAX) {
        continue;
      }
      state->layerSubDocRefs[dst] = index;
      state->subDocInstanceCount++;
      state->subDocNodeCount += static_cast<int>(src->externalDoc->nodes.size());
      if (dst->externalDoc == nullptr) {
        PAGXDocument::AttachExternalDocument(clone, dst, state->subDocuments[index].document);
      }
    } else if (!src->compositionFilePath.empty()) {
      PAGExportIssue issue = {};
      issue.code = PAGExportWarning::MissingSubDocument;
      issue.message = "external composition not loaded: " + src->compositionFilePath;
      result->warnings.push_back(issue);
    }
  }
}

// Assigns spec §5.1 node indices in emission order: shared resources first (images, then
// fonts with their glyph subtrees), then the document tree in pre-order (layer, contents with
// their inline subtrees, children recursively). Only nodes actually emitted get indices;
// nodes dropped from the output get none, and referencing them is invalid.
void AssignFontNodeIndices(Font* font, EncodeState* state, uint32_t* index) {
  if (state->nodeIndices.count(font) > 0) {
    return;
  }
  state->nodeIndices[font] = (*index)++;
  for (auto* glyph : font->glyphs) {
    if (state->nodeIndices.count(glyph) == 0) {
      state->nodeIndices[glyph] = (*index)++;
    }
    if (glyph->image != nullptr && state->nodeIndices.count(glyph->image) == 0) {
      state->nodeIndices[glyph->image] = (*index)++;
    }
    if (glyph->path != nullptr && state->nodeIndices.count(glyph->path) == 0) {
      state->nodeIndices[glyph->path] = (*index)++;
    }
  }
}

void AssignElementNodeIndices(Element* element, EncodeState* state, uint32_t* index) {
  if (element == nullptr || state->nodeIndices.count(element) > 0) {
    return;
  }
  state->nodeIndices[element] = (*index)++;
  switch (element->nodeType()) {
    case NodeType::Fill: {
      auto* color = static_cast<Fill*>(element)->color;
      if (color != nullptr && state->nodeIndices.count(color) == 0) {
        state->nodeIndices[color] = (*index)++;
      }
      break;
    }
    case NodeType::Stroke: {
      auto* color = static_cast<Stroke*>(element)->color;
      if (color != nullptr && state->nodeIndices.count(color) == 0) {
        state->nodeIndices[color] = (*index)++;
      }
      break;
    }
    case NodeType::Path: {
      auto* data = static_cast<Path*>(element)->data;
      if (data != nullptr && state->nodeIndices.count(data) == 0) {
        state->nodeIndices[data] = (*index)++;
      }
      break;
    }
    case NodeType::TextPath: {
      auto* data = static_cast<TextPath*>(element)->path;
      if (data != nullptr && state->nodeIndices.count(data) == 0) {
        state->nodeIndices[data] = (*index)++;
      }
      break;
    }
    case NodeType::Text: {
      auto* text = static_cast<Text*>(element);
      for (auto* run : text->glyphRuns) {
        if (state->nodeIndices.count(run) == 0) {
          state->nodeIndices[run] = (*index)++;
        }
        if (run->font != nullptr) {
          AssignFontNodeIndices(run->font, state, index);
        }
      }
      break;
    }
    case NodeType::Group:
      for (auto* child : static_cast<Group*>(element)->elements) {
        AssignElementNodeIndices(child, state, index);
      }
      break;
    default:
      break;
  }
}

void AssignLayerNodeIndices(Layer* layer, EncodeState* state, uint32_t* index) {
  if (state->nodeIndices.count(layer) > 0) {
    return;
  }
  state->nodeIndices[layer] = (*index)++;
  if (layer->composition != nullptr &&
      state->compositionResourceSlots.count(layer->composition) == 0) {
    // Inline composition: its subtree is numbered at this reference position.
    auto* composition = layer->composition;
    if (state->nodeIndices.count(composition) == 0) {
      state->nodeIndices[composition] = (*index)++;
      for (auto* compLayer : composition->layers) {
        AssignLayerNodeIndices(compLayer, state, index);
      }
    }
  }
  for (auto* element : layer->contents) {
    AssignElementNodeIndices(element, state, index);
  }
  for (auto* child : layer->children) {
    AssignLayerNodeIndices(child, state, index);
  }
}

PAGExportResult PAGExporter::ExportToPAG(std::shared_ptr<PAGXDocument> document,
                                         const std::string& filePath,
                                         const PAGExportOptions& options) {
  PAGExportResult result = {};
  if (document == nullptr) {
    return result;
  }
  // The internal XML round trip links GlyphRun -> Font and Layer -> Composition by resource
  // id, but documents decoded from the v4 binary carry no ids (the binary keeps node identity
  // by reference). Synthesize deterministic ids on id-less fonts and compositions so the XML
  // hop preserves the linkages, and restore the original ids afterwards; StripIds clears them
  // in the working copy anyway, so the written bytes do not depend on the synthesized names.
  std::vector<std::pair<Font*, std::string>> restoredFontIds = {};
  std::vector<std::pair<Composition*, std::string>> restoredCompositionIds = {};
  std::unordered_set<std::string> takenIds = {};
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Font) {
      auto* font = static_cast<Font*>(node.get());
      if (!font->id.empty()) {
        takenIds.insert(font->id);
      }
    } else if (node->nodeType() == NodeType::Composition) {
      auto* composition = static_cast<Composition*>(node.get());
      if (!composition->id.empty()) {
        takenIds.insert(composition->id);
      }
    }
  }
  int synthIndex = 0;
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Font) {
      auto* font = static_cast<Font*>(node.get());
      if (!font->id.empty()) {
        continue;
      }
      restoredFontIds.push_back({font, font->id});
      do {
        font->id = "pag-v4-font-" + std::to_string(synthIndex++);
      } while (takenIds.count(font->id) > 0);
      takenIds.insert(font->id);
    } else if (node->nodeType() == NodeType::Composition) {
      auto* composition = static_cast<Composition*>(node.get());
      if (!composition->id.empty()) {
        continue;
      }
      restoredCompositionIds.push_back({composition, composition->id});
      do {
        composition->id = "pag-v4-comp-" + std::to_string(synthIndex++);
      } while (takenIds.count(composition->id) > 0);
      takenIds.insert(composition->id);
    }
  }
  auto xml = PAGXExporter::ToXML(*document);
  for (auto& restore : restoredFontIds) {
    restore.first->id = restore.second;
  }
  for (auto& restore : restoredCompositionIds) {
    restore.first->id = restore.second;
  }
  auto doc = PAGXImporter::FromXML(xml);
  if (doc == nullptr) {
    return result;
  }
  // Export-time font setup (design doc §4.3): the document's own Font file declarations
  // register first, then the caller's fallback fonts supplement them. The resulting config
  // measures the clone's layout; without either source the document's config applies.
  auto fontConfig = std::unique_ptr<FontConfig>(nullptr);
  if (!options.fallbackFonts.empty() || DocumentDeclaresFontFiles(document.get())) {
    fontConfig = std::make_unique<FontConfig>(document->fontConfig());
    auto failedFiles = RegisterDocumentFontFiles(document.get(), fontConfig.get());
    for (const auto& fontFile : failedFiles) {
      PAGExportIssue issue = {};
      issue.code = PAGExportWarning::MissingFont;
      issue.message = "font source failed to load: " + fontFile;
      result.warnings.push_back(issue);
    }
    auto failedFallbacks = RegisterFallbackFonts(fontConfig.get(), options.fallbackFonts);
    for (const auto& fallback : failedFallbacks) {
      PAGExportIssue issue = {};
      issue.code = PAGExportWarning::MissingFont;
      issue.message = "fallback font not found: " + fallback;
      result.warnings.push_back(issue);
    }
  }
  auto state = std::make_unique<EncodeState>();
  CollectSubDocumentReferences(document.get(), doc.get(), state.get(), &result);
  if (state->subDocInstanceCount > 1024 || state->subDocNodeCount > 1048576 ||
      state->subDocuments.size() > 256) {
    PAGExportIssue issue = {};
    issue.code = PAGExportWarning::SubDocumentExpansionLimit;
    issue.message = "sub-document expansion exceeds the spec limits (instances " +
                    std::to_string(state->subDocInstanceCount) + ", nodes " +
                    std::to_string(state->subDocNodeCount) + ")";
    result.warnings.push_back(issue);
    result.success = false;
    return result;
  }
  doc->applyLayout(fontConfig.get());
  LayoutFlattener::Flatten(doc.get());
  for (auto& sub : state->subDocuments) {
    LayoutFlattener::Flatten(sub.document.get());
  }
  auto retainedIds = Stripper::CollectRetainedIds(doc.get());
  Stripper::StripMetadata(doc.get());
  Stripper::StripIds(doc.get(), retainedIds);
  Stripper::RemoveDeadNodes(doc.get(), retainedIds);
  for (auto& sub : state->subDocuments) {
    auto subRetainedIds = Stripper::CollectRetainedIds(sub.document.get());
    Stripper::StripMetadata(sub.document.get());
    Stripper::StripIds(sub.document.get(), subRetainedIds);
    Stripper::RemoveDeadNodes(sub.document.get(), subRetainedIds);
  }
  auto imageResult = ImageProcessor::Process(doc.get());
  result.warnings.insert(result.warnings.end(), imageResult.warnings.begin(),
                         imageResult.warnings.end());
  if (!doc->animations.empty() || !doc->dataBinds.empty() || doc->viewModel != nullptr) {
    PAGExportIssue issue = {};
    issue.code = PAGExportWarning::AnimatedContentSkipped;
    issue.message = "animated content skipped in phase 1";
    result.warnings.push_back(issue);
  }
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
    for (auto& node : doc->nodes) {
      if (node->nodeType() == NodeType::Composition) {
        for (auto* compLayer : static_cast<Composition*>(node.get())->layers) {
          CollectImageReferencesFromLayer(compLayer, &imageRefs);
        }
      }
    }
    for (auto& sub : state->subDocuments) {
      for (auto* layer : sub.document->layers) {
        CollectImageReferencesFromLayer(layer, &imageRefs);
      }
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
    for (auto& node : doc->nodes) {
      if (node->nodeType() == NodeType::Composition) {
        for (auto* compLayer : static_cast<Composition*>(node.get())->layers) {
          CollectFontReferencesFromLayer(compLayer, &fontRefs);
        }
      }
    }
    for (auto& sub : state->subDocuments) {
      for (auto* layer : sub.document->layers) {
        CollectFontReferencesFromLayer(layer, &fontRefs);
      }
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
  // Shared-composition placement (spec §5.3): count layer references per composition; two or
  // more references go into the ResourceBlock in first-reference order.
  {
    std::vector<Composition*> compositionRefs = {};
    for (auto* layer : doc->layers) {
      CollectCompositionReferencesFromLayer(layer, &compositionRefs);
    }
    std::unordered_map<const Composition*, int> compositionCounts = {};
    for (auto* composition : compositionRefs) {
      compositionCounts[composition]++;
    }
    for (auto* composition : compositionRefs) {
      if (compositionCounts[composition] >= 2 &&
          state->compositionResourceSlots.find(composition) ==
              state->compositionResourceSlots.end()) {
        state->compositionResourceSlots[composition] =
            static_cast<uint32_t>(state->sharedCompositionOrder.size());
        state->sharedCompositionOrder.push_back(composition);
      }
    }
  }
  for (auto* layer : doc->layers) {
    CollectStringsFromLayer(layer, state.get());
  }
  for (auto& node : doc->nodes) {
    if (node->nodeType() == NodeType::Composition) {
      auto* composition = static_cast<Composition*>(node.get());
      for (auto* compLayer : composition->layers) {
        CollectStringsFromLayer(compLayer, state.get());
      }
    }
  }
  for (auto& sub : state->subDocuments) {
    state->intern(sub.name);
    for (auto* layer : sub.document->layers) {
      CollectStringsFromLayer(layer, state.get());
    }
  }
  // Node indices (spec §5.1) back the NodeRef fields (Layer.mask): shared resources first,
  // then the document tree in emission pre-order.
  {
    uint32_t nextIndex = 0;
    for (auto* image : state->sharedImageOrder) {
      auto* imageNode = static_cast<Node*>(image);
      if (state->nodeIndices.count(imageNode) == 0) {
        state->nodeIndices[imageNode] = nextIndex++;
      }
    }
    for (auto* font : state->sharedFontOrder) {
      AssignFontNodeIndices(font, state.get(), &nextIndex);
    }
    for (auto* composition : state->sharedCompositionOrder) {
      if (state->nodeIndices.count(composition) == 0) {
        state->nodeIndices[composition] = nextIndex++;
      }
      for (auto* compLayer : composition->layers) {
        AssignLayerNodeIndices(compLayer, state.get(), &nextIndex);
      }
    }
    for (auto* layer : doc->layers) {
      AssignLayerNodeIndices(layer, state.get(), &nextIndex);
    }
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
    for (auto* composition : state->sharedCompositionOrder) {
      WriteCompositionBlock(&resContent, composition, state.get());
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
  if (!state->subDocuments.empty()) {
    pag::EncodeStream tableContent(&state->streamContext);
    for (auto& sub : state->subDocuments) {
      pag::EncodeStream subContent(&state->streamContext);
      // varuint diagnosticName, then the sub-document's own block (spec §6.5).
      subContent.writeEncodedUint32(state->intern(sub.name));
      {
        PAGAttributeBlockWriter writer(&state->streamContext);
        writer.writeFlag(true);   // width
        writer.writeFlag(true);   // height
        writer.writeFlag(false);  // dataKeys, phase 2
        writer.writeFlag(false);  // dataValues, phase 2
        auto* v = writer.valueStream();
        v->writeFloat(sub.document->width);
        v->writeFloat(sub.document->height);
        auto block = writer.release();
        WritePAGTagHeader(&subContent, TagCode::DocumentAttributes,
                          static_cast<uint32_t>(block->length()));
        subContent.writeBytes(const_cast<uint8_t*>(block->data()),
                              static_cast<uint32_t>(block->length()));
      }
      {
        pag::EncodeStream idContent(&state->streamContext);
        idContent.writeEncodedUint32(0);
        WriteTag(&subContent, TagCode::RetainedIdTable, &idContent);
      }
      for (auto* layer : sub.document->layers) {
        WriteLayerTag(&subContent, layer, state.get());
      }
      WritePAGTagHeader(&subContent, TagCode::End, 0);
      WriteTag(&tableContent, TagCode::SubDocument, &subContent);
    }
    WritePAGTagHeader(&tableContent, TagCode::End, 0);
    WriteTag(&body, TagCode::SubDocumentTable, &tableContent);
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
  if (!options.reportPath.empty()) {
    WriteExportReport(options, result, doc.get(), *state, xml, filePath);
  }
  return result;
}

}  // namespace pagx
