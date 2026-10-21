/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/PAGXDocument.h"
#include "pagx/html/importer/HTMLIdAllocator.h"
#include "pagx/html/importer/HTMLLayerBuilder.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"

namespace pagx {
namespace {
template <typename T>
T* Copy(PAGXDocument* doc, const T* source) {
  auto* result = doc->makeNode<T>();
  *result = *source;
  result->id.clear();
  return result;
}
// Text owns a private shaping cache. Copy authored inputs, never its cache pointer.
Text* CopyText(PAGXDocument* doc, const Text* source) {
  auto* result = doc->makeNode<Text>();
  *static_cast<LayoutNode*>(result) = *static_cast<const LayoutNode*>(source);
  result->id.clear();
  result->text = source->text;
  result->position = source->position;
  result->fontFamily = source->fontFamily;
  result->fontFamilyList = source->fontFamilyList;
  result->fontVariantEmoji = source->fontVariantEmoji;
  result->fontStyle = source->fontStyle;
  result->fontSize = source->fontSize;
  result->letterSpacing = source->letterSpacing;
  result->textOrientation = source->textOrientation;
  result->inlinePositions = source->inlinePositions;
  result->fauxBold = source->fauxBold;
  result->fauxItalic = source->fauxItalic;
  result->textAnchor = source->textAnchor;
  result->baseline = source->baseline;
  result->glyphRuns = source->glyphRuns;
  return result;
}
std::vector<Element*> TextCoverage(PAGXDocument* doc, const std::vector<Element*>& source) {
  std::vector<Element*> result;
  bool text = false;
  for (auto* element : source) {
    if (element->nodeType() == NodeType::Text) {
      result.push_back(CopyText(doc, static_cast<Text*>(element)));
      text = true;
    } else if (element->nodeType() == NodeType::Rectangle &&
               element->customData.count("html-text-decoration")) {
      result.push_back(Copy(doc, static_cast<Rectangle*>(element)));
      text = true;
    } else if (element->nodeType() == NodeType::Group || element->nodeType() == NodeType::TextBox) {
      auto* group = static_cast<Group*>(element);
      auto* copy = element->nodeType() == NodeType::TextBox
                       ? static_cast<Group*>(Copy(doc, static_cast<TextBox*>(element)))
                       : Copy(doc, group);
      copy->elements = TextCoverage(doc, group->elements);
      copy->alpha = 1;
      result.push_back(copy);
    }
  }
  if (text) {
    auto* fill = doc->makeNode<Fill>();
    auto* white = doc->makeNode<SolidColor>();
    white->color = {1, 1, 1, 1};
    fill->color = white;
    result.push_back(fill);
  }
  return result;
}
Layer* TextMask(PAGXDocument* doc, Layer* source, bool root = false) {
  auto* mask = doc->makeNode<Layer>();
  *static_cast<LayoutNode*>(mask) = *static_cast<LayoutNode*>(source);
  mask->id.clear();
  mask->clipToBounds = source->clipToBounds;
  mask->x = source->x;
  mask->y = source->y;
  mask->matrix = source->matrix;
  mask->matrix3D = source->matrix3D;
  mask->preserve3D = source->preserve3D;
  mask->layout = source->layout;
  mask->gap = source->gap;
  mask->flex = source->flex;
  mask->padding = source->padding;
  mask->alignment = source->alignment;
  mask->arrangement = source->arrangement;
  mask->includeInLayout = source->includeInLayout;
  auto eligibility = source->customData.find("pagx-text-clip");
  if (!source->visible) return mask;
  if (!root && (source->customData.count("html-text-shadow") ||
                (eligibility != source->customData.end() && eligibility->second == "exclude")))
    return mask;
  mask->contents = TextCoverage(doc, source->contents);
  for (auto* child : source->children) {
    if (child == source->mask) continue;
    mask->children.push_back(TextMask(doc, child));
  }
  return mask;
}
}  // namespace
void HTMLLayerBuilder::applyTextBackground(Layer* layer, const HTMLBoxAttributes& box) {
  // The pending paints retain their original slot in the background stack, including
  // mixed text/box clips and blend modes. Each gets its own mask in the owner's space.
  auto pending = _textBackgrounds.find(layer);
  if (pending == _textBackgrounds.end()) {
    // Box-shadow splitting may have introduced an outer wrapper around the paint owner.
    for (auto* child : layer->children) {
      if (_textBackgrounds.count(child)) applyTextBackground(child, box);
    }
    return;
  }
  auto paints = std::move(pending->second);
  _textBackgrounds.erase(pending);
  for (auto* paint : paints) {
    auto* mask = TextMask(_document, layer, true);
    mask->id = _idAllocator.generateUnique("textBackgroundMask");
    mask->left = mask->top = 0;
    mask->right = mask->bottom = mask->centerX = mask->centerY = NAN;
    mask->width = box.widthPx;
    mask->height = box.heightPx;
    mask->percentWidth = mask->percentHeight = NAN;
    mask->x = mask->y = 0;
    mask->matrix = {};
    mask->matrix3D = {};
    mask->includeInLayout = false;
    paint->mask = mask;
    paint->maskType = MaskType::Alpha;
    paint->visible = true;
    paint->children.push_back(mask);
  }
}
}  // namespace pagx
