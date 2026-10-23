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
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Polystar.h"
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
    for (auto& [key, value] : pattern->customData) {
      state->intern(key);
      state->intern(value);
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

// Writes a color source as an inline sub-tag inside the parent tag's value area (§9.0).
void WriteColorSourceInline(pag::EncodeStream* content, ColorSource* source, EncodeState* state) {
  if (source == nullptr) {
    return;
  }
  switch (source->nodeType()) {
    case NodeType::SolidColor: {
      auto* solid = static_cast<SolidColor*>(source);
      pag::EncodeStream tag(&state->streamContext);
      tag.writeUBits(1, 1);  // color always exists
      WritePAGColor(&tag, solid->color);
      WriteTag(content, TagCode::SolidColor, &tag);
      break;
    }
    case NodeType::LinearGradient: {
      auto* grad = static_cast<LinearGradient*>(source);
      pag::EncodeStream tag(&state->streamContext);
      tag.writeUBits(1, 1);  // startPoint
      tag.writeUBits(1, 1);  // endPoint
      tag.writeUBits(0, 1);  // fitsToGeometry (default true, not written)
      tag.writeUBits(0, 1);  // matrix (identity)
      tag.writeUBits(1, 1);  // stopCount exists (always)
      tag.alignWithBytes();
      tag.writeFloat(grad->startPoint.x);
      tag.writeFloat(grad->startPoint.y);
      tag.writeFloat(grad->endPoint.x);
      tag.writeFloat(grad->endPoint.y);
      tag.writeEncodedUint32(static_cast<uint32_t>(grad->colorStops.size()));
      for (auto* stop : grad->colorStops) {
        tag.writeFloatList(&stop->offset, 1, 0.00002f);
      }
      for (auto* stop : grad->colorStops) {
        WritePAGColor(&tag, stop->color);
      }
      WriteTag(content, TagCode::LinearGradient, &tag);
      break;
    }
    default:
      // Other color sources (radial/conic/diamond gradients, image patterns) follow the
      // same pattern; added incrementally.
      break;
  }
}

// Writes a painter (Fill or Stroke) tag. The color source reference (varuint) goes in the
// value area; the inline color source sub-tag follows at the end (§9.0).
void WriteFillTag(pag::EncodeStream* stream, Fill* fill, EncodeState* state) {
  PAGAttributeBlockWriter writer(&state->streamContext);
  writer.writeFlag(fill->color != nullptr);
  writer.writeFlag(fill->alpha != 1.0f);
  writer.writeFlag(fill->blendMode != BlendMode::Normal);
  writer.writeFlag(fill->fillRule != FillRule::Winding);
  writer.writeFlag(fill->placement != LayerPlacement::Background);
  auto* v = writer.valueStream();
  if (fill->color != nullptr) {
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
  WritePAGTagHeader(stream, TagCode::Fill, static_cast<uint32_t>(block->length()));
  stream->writeByteData(block.get());
  if (fill->color != nullptr) {
    WriteColorSourceInline(stream, fill->color, state);
  }
}

void WriteStrokeTag(pag::EncodeStream* stream, Stroke* stroke, EncodeState* state) {
  PAGAttributeBlockWriter writer(&state->streamContext);
  writer.writeFlag(stroke->color != nullptr);
  writer.writeFlag(stroke->alpha != 1.0f);
  writer.writeFlag(stroke->blendMode != BlendMode::Normal);
  writer.writeFlag(stroke->width != 1.0f);
  writer.writeFlag(stroke->cap != LineCap::Butt);
  writer.writeFlag(stroke->join != LineJoin::Miter);
  writer.writeFlag(stroke->miterLimit != 4.0f);
  writer.writeFlag(!stroke->dashes.empty());
  writer.writeFlag(stroke->dashOffset != 0.0f);
  writer.writeFlag(stroke->dashAdaptive);
  writer.writeFlag(stroke->align != StrokeAlign::Center);
  writer.writeFlag(stroke->placement != LayerPlacement::Background);
  auto* v = writer.valueStream();
  if (stroke->color != nullptr) {
    v->writeEncodedUint32(0);
  }
  if (stroke->alpha != 1.0f) {
    v->writeUint8(static_cast<uint8_t>(lroundf(stroke->alpha * 255.0f)));
  }
  if (stroke->blendMode != BlendMode::Normal) {
    v->writeUint8(static_cast<uint8_t>(stroke->blendMode));
  }
  if (stroke->width != 1.0f) {
    v->writeFloat(stroke->width);
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
  if (stroke->dashAdaptive) {
    v->writeBoolean(true);
  }
  if (stroke->align != StrokeAlign::Center) {
    v->writeUint8(static_cast<uint8_t>(stroke->align));
  }
  if (stroke->placement != LayerPlacement::Background) {
    v->writeUint8(static_cast<uint8_t>(stroke->placement));
  }
  auto block = writer.release();
  WritePAGTagHeader(stream, TagCode::Stroke, static_cast<uint32_t>(block->length()));
  stream->writeByteData(block.get());
  if (stroke->color != nullptr) {
    WriteColorSourceInline(stream, stroke->color, state);
  }
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
  stream->writeByteData(block.get());
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
  stream->writeByteData(block.get());
}

void WritePathTag(pag::EncodeStream* stream, Path* path, EncodeState* state) {
  PAGAttributeBlockWriter writer(&state->streamContext);
  writer.writeFlag(path->data != nullptr);
  writer.writeFlag(path->position.x != 0.0f);
  writer.writeFlag(path->position.y != 0.0f);
  writer.writeFlag(path->reversed);
  auto* v = writer.valueStream();
  if (path->data != nullptr) {
    v->writeEncodedUint32(0);
  }
  if (path->position.x != 0.0f) v->writeFloat(path->position.x);
  if (path->position.y != 0.0f) v->writeFloat(path->position.y);
  auto block = writer.release();
  WritePAGTagHeader(stream, TagCode::Path, static_cast<uint32_t>(block->length()));
  stream->writeByteData(block.get());
  if (path->data != nullptr) {
    pag::EncodeStream pathTag(&state->streamContext);
    WritePAGPath(&pathTag, path->data, 0);
    WriteTag(stream, TagCode::PathData, &pathTag);
  }
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
    content.writeByteData(block.get());
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
    content.writeByteData(block.get());
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
    auto notVisible = !layer->visible;
    auto hasAlpha = layer->alpha != 1.0f;
    auto hasBlend = layer->blendMode != BlendMode::Normal;
    auto hasMatrix = !layer->matrix.isIdentity();
    auto hasMatrix3D = !layer->matrix3D.isIdentity();
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
    writer.writeFlag(!layer->preserve3D);
    writer.writeFlag(!layer->antiAlias);
    writer.writeFlag(!layer->groupOpacity);
    writer.writeFlag(!layer->passThroughBackground);
    writer.writeFlag(false);
    writer.writeFlag(false);
    writer.writeFlag(hasScrollRect);
    writer.writeFlag(hasMask);
    writer.writeFlag(hasMaskType);
    writer.writeFlag(false);
    writer.writeFlag(false);
    writer.writeFlag(hasVmContext);
    auto* v = writer.valueStream();
    if (hasName) v->writeEncodedUint32(state->intern(layer->name));
    if (hasX) v->writeFloat(layer->x);
    if (hasY) v->writeFloat(layer->y);
    if (notVisible) v->writeBoolean(false);
    if (hasAlpha) v->writeUint8(static_cast<uint8_t>(lroundf(layer->alpha * 255.0f)));
    if (hasBlend) v->writeUint8(static_cast<uint8_t>(layer->blendMode));
    if (hasMatrix) WritePAGMatrix(v, layer->matrix);
    if (hasMatrix3D) WritePAGMatrix3D(v, layer->matrix3D);
    if (hasScrollRect) {
      v->writeFloat(layer->scrollRect.x);
      v->writeFloat(layer->scrollRect.y);
      v->writeFloat(layer->scrollRect.width);
      v->writeFloat(layer->scrollRect.height);
    }
    if (hasMask) v->writeEncodedUint32(0);
    if (hasMaskType) v->writeUint8(static_cast<uint8_t>(layer->maskType));
    if (hasVmContext) v->writeEncodedUint32(state->intern(layer->vmContext));
    auto block = writer.release();
    WritePAGTagHeader(&content, TagCode::LayerAttributes, static_cast<uint32_t>(block->length()));
    content.writeByteData(block.get());
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
  auto xml = PAGXExporter::ToXML(*document);
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
      docContent.writeByteData(block.get());
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
