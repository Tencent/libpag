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
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/PAGXOptimizer.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include "pagx/PAGXAnalyzer.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LayoutNode.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"

namespace pagx {

// ============================================================================
// Painter comparison helpers (Optimizer-specific, not shared with CommandVerify)
// ============================================================================

static bool IsPainter(NodeType type) {
  return type == NodeType::Fill || type == NodeType::Stroke;
}

static bool SameColorSource(const ColorSource* a, const ColorSource* b) {
  if (a == b) {
    return true;
  }
  if (a == nullptr || b == nullptr) {
    return false;
  }
  if (a->nodeType() != b->nodeType()) {
    return false;
  }
  if (a->nodeType() == NodeType::SolidColor) {
    return static_cast<const SolidColor*>(a)->color == static_cast<const SolidColor*>(b)->color;
  }
  return false;
}

static bool SameFillSignature(const Fill* a, const Fill* b) {
  return a->alpha == b->alpha && a->blendMode == b->blendMode && a->fillRule == b->fillRule &&
         a->placement == b->placement && SameColorSource(a->color, b->color);
}

static bool SameStrokeSignature(const Stroke* a, const Stroke* b) {
  return a->width == b->width && a->alpha == b->alpha && a->blendMode == b->blendMode &&
         a->cap == b->cap && a->join == b->join && a->miterLimit == b->miterLimit &&
         a->dashes == b->dashes && a->dashOffset == b->dashOffset &&
         a->dashAdaptive == b->dashAdaptive && a->align == b->align &&
         a->placement == b->placement && SameColorSource(a->color, b->color);
}

static size_t FindPainterSuffixStart(const std::vector<Element*>& elements) {
  size_t suffixStart = elements.size();
  for (size_t i = elements.size(); i > 0; --i) {
    if (IsPainter(elements[i - 1]->nodeType())) {
      suffixStart = i - 1;
    } else {
      break;
    }
  }
  return suffixStart;
}

static bool HaveSamePainterSuffix(const Group* a, const Group* b) {
  size_t aStart = FindPainterSuffixStart(a->elements);
  size_t bStart = FindPainterSuffixStart(b->elements);
  size_t aPainterCount = a->elements.size() - aStart;
  size_t bPainterCount = b->elements.size() - bStart;
  if (aPainterCount == 0 || aPainterCount != bPainterCount) {
    return false;
  }
  for (size_t i = 0; i < aPainterCount; i++) {
    auto* ea = a->elements[aStart + i];
    auto* eb = b->elements[bStart + i];
    if (ea->nodeType() != eb->nodeType()) {
      return false;
    }
    if (ea->nodeType() == NodeType::Fill) {
      if (!SameFillSignature(static_cast<const Fill*>(ea), static_cast<const Fill*>(eb))) {
        return false;
      }
    } else {
      if (!SameStrokeSignature(static_cast<const Stroke*>(ea), static_cast<const Stroke*>(eb))) {
        return false;
      }
    }
  }
  return true;
}

// ============================================================================
// Optimization passes — element-level
// ============================================================================

static bool RemoveEmptyGroups(std::vector<Element*>& elements);
static bool RemoveZeroWidthStrokes(std::vector<Element*>& elements);
static bool UnwrapRedundantFirstChildGroup(std::vector<Element*>& elements);
static bool MergeConsecutiveGroups(std::vector<Element*>& elements);
static bool ConvertPathsToPrimitives(std::vector<Element*>& elements, PAGXDocument* doc);

static void OptimizeElements(std::vector<Element*>& elements, PAGXDocument* doc, bool& changed) {
  for (auto* element : elements) {
    auto type = element->nodeType();
    if (type == NodeType::Group || type == NodeType::TextBox) {
      auto* group = static_cast<Group*>(element);
      OptimizeElements(group->elements, doc, changed);
    }
  }
  if (RemoveEmptyGroups(elements)) {
    changed = true;
  }
  if (RemoveZeroWidthStrokes(elements)) {
    changed = true;
  }
  if (UnwrapRedundantFirstChildGroup(elements)) {
    changed = true;
  }
  if (MergeConsecutiveGroups(elements)) {
    changed = true;
  }
  if (ConvertPathsToPrimitives(elements, doc)) {
    changed = true;
  }
}

static bool RemoveEmptyGroups(std::vector<Element*>& elements) {
  bool changed = false;
  auto it = elements.begin();
  while (it != elements.end()) {
    if ((*it)->nodeType() == NodeType::Group) {
      auto* group = static_cast<Group*>(*it);
      if (PAGXAnalyzer::IsEmptyGroup(group)) {
        it = elements.erase(it);
        changed = true;
        continue;
      }
    }
    ++it;
  }
  return changed;
}

static bool RemoveZeroWidthStrokes(std::vector<Element*>& elements) {
  bool changed = false;
  auto it = elements.begin();
  while (it != elements.end()) {
    if ((*it)->nodeType() == NodeType::Stroke) {
      auto* stroke = static_cast<Stroke*>(*it);
      if (stroke->width <= 0.0f) {
        it = elements.erase(it);
        changed = true;
        continue;
      }
    }
    ++it;
  }
  return changed;
}

static bool UnwrapRedundantFirstChildGroup(std::vector<Element*>& elements) {
  if (elements.empty() || elements[0]->nodeType() != NodeType::Group) {
    return false;
  }
  auto* group = static_cast<Group*>(elements[0]);
  if (!PAGXAnalyzer::CanUnwrapFirstChildGroup(group)) {
    return false;
  }
  auto children = std::move(group->elements);
  elements.erase(elements.begin());
  elements.insert(elements.begin(), children.begin(), children.end());
  return true;
}

static bool MergeConsecutiveGroups(std::vector<Element*>& elements) {
  bool changed = false;
  size_t i = 0;
  while (i < elements.size()) {
    if (elements[i]->nodeType() != NodeType::Group) {
      i++;
      continue;
    }
    auto* current = static_cast<Group*>(elements[i]);
    if (!PAGXAnalyzer::HasDefaultGroupTransform(current)) {
      i++;
      continue;
    }
    size_t painterStart = FindPainterSuffixStart(current->elements);
    if (painterStart == current->elements.size()) {
      i++;
      continue;
    }
    size_t j = i + 1;
    while (j < elements.size()) {
      if (elements[j]->nodeType() != NodeType::Group) {
        break;
      }
      auto* next = static_cast<Group*>(elements[j]);
      if (!PAGXAnalyzer::HasDefaultGroupTransform(next)) {
        break;
      }
      if (!HaveSamePainterSuffix(current, next)) {
        break;
      }
      size_t nextPainterStart = FindPainterSuffixStart(next->elements);
      current->elements.insert(current->elements.begin() + static_cast<long>(painterStart),
                               next->elements.begin(),
                               next->elements.begin() + static_cast<long>(nextPainterStart));
      painterStart += nextPainterStart;
      elements.erase(elements.begin() + static_cast<long>(j));
      changed = true;
    }
    i = j;
  }
  return changed;
}

static bool ConvertPathsToPrimitives(std::vector<Element*>& elements, PAGXDocument* doc) {
  for (auto* element : elements) {
    auto type = element->nodeType();
    if (type == NodeType::TrimPath || type == NodeType::MergePath) {
      return false;
    }
  }
  bool changed = false;
  for (size_t i = 0; i < elements.size(); i++) {
    if (elements[i]->nodeType() != NodeType::Path) {
      continue;
    }
    auto* path = static_cast<Path*>(elements[i]);
    PrimitiveInfo info;
    auto primitive = PAGXAnalyzer::DetectPathPrimitive(path, &info);
    if (primitive == PathPrimitive::Rectangle) {
      auto* rect = doc->makeNode<Rectangle>();
      rect->position = {info.cx + path->position.x, info.cy + path->position.y};
      rect->size = {info.w, info.h};
      rect->reversed = path->reversed;
      rect->customData = std::move(path->customData);
      elements[i] = rect;
      changed = true;
    } else if (primitive == PathPrimitive::Ellipse) {
      auto* ellipse = doc->makeNode<Ellipse>();
      ellipse->position = {info.cx + path->position.x, info.cy + path->position.y};
      ellipse->size = {info.w, info.h};
      ellipse->reversed = path->reversed;
      ellipse->customData = std::move(path->customData);
      elements[i] = ellipse;
      changed = true;
    }
  }
  return changed;
}

// ============================================================================
// Optimization passes — layer-level
// ============================================================================

static bool MergeAdjacentShellLayers(std::vector<Layer*>& layers, PAGXDocument* doc) {
  if (layers.size() < 2) {
    return false;
  }
  bool changed = false;
  size_t i = 0;
  while (i < layers.size()) {
    if (!PAGXAnalyzer::CanDowngradeLayerToGroup(layers[i])) {
      i++;
      continue;
    }
    size_t runEnd = i + 1;
    while (runEnd < layers.size() && PAGXAnalyzer::CanDowngradeLayerToGroup(layers[runEnd])) {
      runEnd++;
    }
    if (runEnd - i >= 2) {
      auto* merged = doc->makeNode<Layer>();
      for (size_t j = i; j < runEnd; j++) {
        auto* group = doc->makeNode<Group>();
        group->elements = std::move(layers[j]->contents);
        merged->contents.push_back(group);
      }
      bool dummy = false;
      OptimizeElements(merged->contents, doc, dummy);
      layers.erase(layers.begin() + static_cast<long>(i),
                   layers.begin() + static_cast<long>(runEnd));
      layers.insert(layers.begin() + static_cast<long>(i), merged);
      changed = true;
      i++;
    } else {
      i = runEnd;
    }
  }
  return changed;
}

static bool DowngradeChildLayersToGroups(Layer* parentLayer, PAGXDocument* doc) {
  if (parentLayer->layout != LayoutMode::None || parentLayer->children.empty()) {
    return false;
  }
  if (!parentLayer->contents.empty()) {
    return false;
  }
  for (auto* child : parentLayer->children) {
    if (!PAGXAnalyzer::CanDowngradeLayerToGroup(child)) {
      return false;
    }
  }
  for (auto* child : parentLayer->children) {
    auto* group = doc->makeNode<Group>();
    group->elements = std::move(child->contents);
    group->customData = std::move(child->customData);
    parentLayer->contents.push_back(group);
  }
  parentLayer->children.clear();
  return true;
}

static bool RemoveFullCanvasClipMask(Layer* layer, float canvasWidth, float canvasHeight) {
  if (layer->mask == nullptr) {
    return false;
  }
  auto* maskLayer = layer->mask;
  if (maskLayer->x != 0 || maskLayer->y != 0) {
    return false;
  }
  if (!maskLayer->matrix.isIdentity()) {
    return false;
  }
  if (maskLayer->contents.size() != 1) {
    return false;
  }
  if (maskLayer->contents[0]->nodeType() != NodeType::Rectangle) {
    return false;
  }
  auto* rect = static_cast<Rectangle*>(maskLayer->contents[0]);
  float left = rect->position.x - rect->size.width * 0.5f;
  float top = rect->position.y - rect->size.height * 0.5f;
  float right = rect->position.x + rect->size.width * 0.5f;
  float bottom = rect->position.y + rect->size.height * 0.5f;
  if (left <= 0 && top <= 0 && right >= canvasWidth && bottom >= canvasHeight) {
    layer->mask = nullptr;
    return true;
  }
  return false;
}

static bool ConvertRectMaskToClipToBounds(Layer* layer) {
  if (layer->mask == nullptr || layer->maskType != MaskType::Alpha) {
    return false;
  }
  auto* maskLayer = layer->mask;
  if (maskLayer->contents.size() != 2) {
    return false;
  }
  if (maskLayer->contents[0]->nodeType() != NodeType::Rectangle ||
      maskLayer->contents[1]->nodeType() != NodeType::Fill) {
    return false;
  }
  auto* fill = static_cast<Fill*>(maskLayer->contents[1]);
  if (fill->alpha < 0.999f) {
    return false;
  }
  if (!maskLayer->matrix.isIdentity()) {
    return false;
  }
  auto* rect = static_cast<Rectangle*>(maskLayer->contents[0]);
  if (rect->roundness > 0.0f) {
    return false;
  }
  float maskW = rect->size.width;
  float maskH = rect->size.height;
  float maskCx = rect->position.x;
  float maskCy = rect->position.y;
  float maskLeft = maskCx - maskW * 0.5f;
  float maskTop = maskCy - maskH * 0.5f;

  bool sizeMatchesLayer = false;
  if (!std::isnan(layer->width) && !std::isnan(layer->height)) {
    sizeMatchesLayer = std::abs(maskW - layer->width) < 0.5f &&
                       std::abs(maskH - layer->height) < 0.5f && std::abs(maskLeft) < 0.5f &&
                       std::abs(maskTop) < 0.5f;
  }
  if (!sizeMatchesLayer) {
    if (std::abs(maskLeft) < 0.5f && std::abs(maskTop) < 0.5f && std::isnan(layer->width) &&
        std::isnan(layer->height)) {
      layer->width = maskW;
      layer->height = maskH;
      sizeMatchesLayer = true;
    }
  }
  if (!sizeMatchesLayer) {
    return false;
  }
  layer->clipToBounds = true;
  layer->mask = nullptr;
  return true;
}

static void OptimizeLayerRecursive(Layer* layer, PAGXDocument* doc, float canvasWidth,
                                   float canvasHeight, bool& changed) {
  bool thisHasLayout = layer->layout != LayoutMode::None;
  auto it = layer->children.begin();
  while (it != layer->children.end()) {
    auto* child = *it;
    OptimizeLayerRecursive(child, doc, canvasWidth, canvasHeight, changed);
    if (PAGXAnalyzer::IsEmptyLayer(child, thisHasLayout)) {
      it = layer->children.erase(it);
      changed = true;
      continue;
    }
    ++it;
  }

  if (DowngradeChildLayersToGroups(layer, doc)) {
    changed = true;
  }

  if (MergeAdjacentShellLayers(layer->children, doc)) {
    changed = true;
  }

  if (RemoveFullCanvasClipMask(layer, canvasWidth, canvasHeight)) {
    changed = true;
  }

  if (ConvertRectMaskToClipToBounds(layer)) {
    changed = true;
  }

  OptimizeElements(layer->contents, doc, changed);
}

// ============================================================================
// Duplicate PathData deduplication
// ============================================================================

static bool PathDataEquals(const PathData* a, const PathData* b) {
  if (a->verbs() != b->verbs()) {
    return false;
  }
  auto& ap = a->points();
  auto& bp = b->points();
  if (ap.size() != bp.size()) {
    return false;
  }
  for (size_t i = 0; i < ap.size(); i++) {
    if (std::abs(ap[i].x - bp[i].x) > 0.001f || std::abs(ap[i].y - bp[i].y) > 0.001f) {
      return false;
    }
  }
  return true;
}

static void ReplacePathDataInElements(
    const std::vector<Element*>& elements,
    const std::unordered_map<PathData*, PathData*>& replacements) {
  for (auto* element : elements) {
    if (element->nodeType() == NodeType::Path) {
      auto* path = static_cast<Path*>(element);
      auto it = replacements.find(path->data);
      if (it != replacements.end()) {
        path->data = it->second;
      }
    } else if (element->nodeType() == NodeType::Group || element->nodeType() == NodeType::TextBox) {
      auto* group = static_cast<Group*>(element);
      ReplacePathDataInElements(group->elements, replacements);
    }
  }
}

static void ReplacePathDataInLayer(Layer* layer,
                                   const std::unordered_map<PathData*, PathData*>& replacements) {
  ReplacePathDataInElements(layer->contents, replacements);
  for (auto* child : layer->children) {
    ReplacePathDataInLayer(child, replacements);
  }
  if (layer->mask != nullptr) {
    ReplacePathDataInLayer(layer->mask, replacements);
  }
}

static bool DeduplicatePathData(PAGXDocument* document) {
  std::vector<PathData*> allPathData;
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::PathData) {
      allPathData.push_back(static_cast<PathData*>(node.get()));
    }
  }
  if (allPathData.size() < 2) {
    return false;
  }

  std::unordered_map<PathData*, PathData*> replacements;
  for (size_t i = 0; i < allPathData.size(); i++) {
    if (replacements.count(allPathData[i])) {
      continue;
    }
    for (size_t j = i + 1; j < allPathData.size(); j++) {
      if (replacements.count(allPathData[j])) {
        continue;
      }
      if (PathDataEquals(allPathData[i], allPathData[j])) {
        replacements[allPathData[j]] = allPathData[i];
      }
    }
  }
  if (replacements.empty()) {
    return false;
  }

  for (auto* layer : document->layers) {
    ReplacePathDataInLayer(layer, replacements);
  }
  return true;
}

// ============================================================================
// Unreferenced resource cleanup
// ============================================================================

static void CollectRefsFromColorSource(const ColorSource* color, std::unordered_set<Node*>& refs) {
  if (color == nullptr) {
    return;
  }
  refs.insert(const_cast<ColorSource*>(color));
  if (color->nodeType() == NodeType::ImagePattern) {
    auto* pattern = static_cast<const ImagePattern*>(color);
    if (pattern->image != nullptr) {
      refs.insert(pattern->image);
    }
  }
}

static void CollectRefsFromFont(const Font* font, std::unordered_set<Node*>& refs) {
  if (font == nullptr) {
    return;
  }
  refs.insert(const_cast<Font*>(font));
  for (auto* glyph : font->glyphs) {
    if (glyph->path != nullptr) {
      refs.insert(glyph->path);
    }
    if (glyph->image != nullptr) {
      refs.insert(glyph->image);
    }
  }
}

static void CollectRefsFromElements(const std::vector<Element*>& elements,
                                    std::unordered_set<Node*>& refs) {
  for (auto* element : elements) {
    auto type = element->nodeType();
    if (type == NodeType::Path) {
      auto* path = static_cast<Path*>(element);
      if (path->data != nullptr) {
        refs.insert(path->data);
      }
    } else if (type == NodeType::Group || type == NodeType::TextBox) {
      auto* group = static_cast<Group*>(element);
      CollectRefsFromElements(group->elements, refs);
    } else if (type == NodeType::Fill) {
      auto* fill = static_cast<Fill*>(element);
      CollectRefsFromColorSource(fill->color, refs);
    } else if (type == NodeType::Stroke) {
      auto* stroke = static_cast<Stroke*>(element);
      CollectRefsFromColorSource(stroke->color, refs);
    } else if (type == NodeType::Text) {
      auto* text = static_cast<Text*>(element);
      for (auto* glyphRun : text->glyphRuns) {
        refs.insert(glyphRun);
        CollectRefsFromFont(glyphRun->font, refs);
      }
    }
  }
}

static void CollectRefsFromLayer(Layer* layer, std::unordered_set<Node*>& refs) {
  CollectRefsFromElements(layer->contents, refs);
  for (auto* child : layer->children) {
    refs.insert(child);
    CollectRefsFromLayer(child, refs);
  }
  if (layer->mask != nullptr) {
    refs.insert(layer->mask);
    CollectRefsFromLayer(layer->mask, refs);
  }
  if (layer->composition != nullptr) {
    refs.insert(layer->composition);
  }
}

static bool RemoveUnreferencedResources(PAGXDocument* document) {
  std::unordered_set<Node*> refs;
  for (auto* layer : document->layers) {
    refs.insert(layer);
    CollectRefsFromLayer(layer, refs);
  }

  bool changed = false;
  auto it = document->nodes.begin();
  while (it != document->nodes.end()) {
    auto* node = it->get();
    auto type = node->nodeType();
    if (type == NodeType::Layer || type == NodeType::Document) {
      ++it;
      continue;
    }
    if (node->id.empty()) {
      ++it;
      continue;
    }
    if (refs.find(node) == refs.end()) {
      it = document->nodes.erase(it);
      changed = true;
    } else {
      ++it;
    }
  }
  return changed;
}

// ============================================================================
// Public API
// ============================================================================

bool PAGXOptimizer::Optimize(PAGXDocument* document) {
  if (document == nullptr) {
    return false;
  }

  bool changed = false;

  float canvasWidth = static_cast<float>(document->width);
  float canvasHeight = static_cast<float>(document->height);

  // Pass 1: Recursive layer optimization (bottom-up).
  for (auto* layer : document->layers) {
    OptimizeLayerRecursive(layer, document, canvasWidth, canvasHeight, changed);
  }

  // Pass 2: Merge adjacent shell layers at root level.
  if (MergeAdjacentShellLayers(document->layers, document)) {
    changed = true;
  }

  // Pass 3: Remove top-level empty layers.
  auto it = document->layers.begin();
  while (it != document->layers.end()) {
    if (PAGXAnalyzer::IsEmptyLayer(*it, false)) {
      it = document->layers.erase(it);
      changed = true;
    } else {
      ++it;
    }
  }

  // Pass 4: Deduplicate PathData references.
  if (DeduplicatePathData(document)) {
    changed = true;
  }

  // Pass 5: Remove unreferenced resources orphaned by earlier passes.
  if (RemoveUnreferencedResources(document)) {
    changed = true;
  }

  return changed;
}

}  // namespace pagx
