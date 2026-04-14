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
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LayoutNode.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"

namespace pagx {

// ============================================================================
// Predicate helpers (mirroring CommandVerify.cpp detection logic)
// ============================================================================

static bool IsEmptyLayer(const Layer* layer, bool parentHasLayout) {
  if (!layer->contents.empty() || !layer->children.empty() || layer->composition != nullptr) {
    return false;
  }
  if (!std::isnan(layer->width) || !std::isnan(layer->height)) {
    return false;
  }
  bool hasSizeDependentConstraint = (!std::isnan(layer->right) || !std::isnan(layer->bottom) ||
                                     !std::isnan(layer->centerX) || !std::isnan(layer->centerY));
  if (hasSizeDependentConstraint) {
    return false;
  }
  return !(parentHasLayout && layer->includeInLayout);
}

static bool IsEmptyGroup(const Group* group) {
  if (!group->elements.empty()) {
    return false;
  }
  if (!std::isnan(group->width) || !std::isnan(group->height)) {
    return false;
  }
  bool hasSizeDependentConstraint = (!std::isnan(group->right) || !std::isnan(group->bottom) ||
                                     !std::isnan(group->centerX) || !std::isnan(group->centerY));
  return !hasSizeDependentConstraint;
}

static bool CanDowngradeLayerToGroup(const Layer* layer) {
  if (!layer->children.empty() || !layer->styles.empty() || !layer->filters.empty()) {
    return false;
  }
  if (layer->mask != nullptr || layer->composition != nullptr) {
    return false;
  }
  if (layer->blendMode != BlendMode::Normal || !layer->visible || layer->hasScrollRect) {
    return false;
  }
  if (!layer->matrix.isIdentity() || !layer->matrix3D.isIdentity()) {
    return false;
  }
  if (layer->preserve3D || !layer->groupOpacity || !layer->passThroughBackground ||
      !layer->antiAlias) {
    return false;
  }
  if (!layer->id.empty() || !layer->name.empty()) {
    return false;
  }
  if (layer->layout != LayoutMode::None || layer->flex > 0) {
    return false;
  }
  if (!std::isnan(layer->width) || !std::isnan(layer->height)) {
    return false;
  }
  if (!std::isnan(layer->left) || !std::isnan(layer->right) || !std::isnan(layer->top) ||
      !std::isnan(layer->bottom) || !std::isnan(layer->centerX) || !std::isnan(layer->centerY)) {
    return false;
  }
  return layer->includeInLayout;
}

static bool HasDefaultGroupTransform(const Group* group) {
  return group->anchor.x == 0 && group->anchor.y == 0 && group->position.x == 0 &&
         group->position.y == 0 && group->rotation == 0 && group->scale.x == 1 &&
         group->scale.y == 1 && group->skew == 0 && group->alpha == 1 && std::isnan(group->width) &&
         std::isnan(group->height);
}

static bool CanUnwrapFirstChildGroup(const Group* group) {
  if (group->alpha != 1.0f || group->position.x != 0 || group->position.y != 0) {
    return false;
  }
  if (group->anchor.x != 0 || group->anchor.y != 0 || group->rotation != 0) {
    return false;
  }
  if (group->scale.x != 1 || group->scale.y != 1 || group->skew != 0) {
    return false;
  }
  if (!std::isnan(group->width) || !std::isnan(group->height)) {
    return false;
  }
  if (!std::isnan(group->left) || !std::isnan(group->right) || !std::isnan(group->top) ||
      !std::isnan(group->bottom) || !std::isnan(group->centerX) || !std::isnan(group->centerY)) {
    return false;
  }
  for (auto* child : group->elements) {
    auto* layoutNode = LayoutNode::AsLayoutNode(child);
    if (layoutNode == nullptr) {
      continue;
    }
    if (!std::isnan(layoutNode->right) || !std::isnan(layoutNode->bottom) ||
        !std::isnan(layoutNode->centerX) || !std::isnan(layoutNode->centerY)) {
      return false;
    }
  }
  return true;
}

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
  // For non-solid colors (gradients, patterns), only equal if same object.
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
// Path-to-Primitive detection (from CommandVerify.cpp)
// ============================================================================

enum class PathPrimitive { None, Rectangle, Ellipse };

struct RectInfo {
  float cx = 0;
  float cy = 0;
  float w = 0;
  float h = 0;
};

struct EllipseInfo {
  float cx = 0;
  float cy = 0;
  float w = 0;
  float h = 0;
};

static PathPrimitive DetectPathPrimitive(const Path* path, RectInfo* rectOut,
                                         EllipseInfo* ellipseOut) {
  if (path->data == nullptr || path->data->isEmpty()) {
    return PathPrimitive::None;
  }
  if (!std::isnan(path->left) || !std::isnan(path->right) || !std::isnan(path->top) ||
      !std::isnan(path->bottom) || !std::isnan(path->centerX) || !std::isnan(path->centerY)) {
    return PathPrimitive::None;
  }
  auto& verbs = path->data->verbs();
  auto& pts = path->data->points();

  // Check for axis-aligned rectangle (4 or 5 line segments + close).
  if (verbs.size() == 5 || verbs.size() == 6) {
    bool allLine = true;
    for (size_t i = 1; i < verbs.size() - 1; i++) {
      if (verbs[i] != PathVerb::Line) {
        allLine = false;
        break;
      }
    }
    if (allLine && verbs[0] == PathVerb::Move && verbs.back() == PathVerb::Close) {
      bool axisAligned = true;
      size_t pointCount = pts.size();
      for (size_t i = 0; i < pointCount && axisAligned; i++) {
        auto& p1 = pts[i];
        auto& p2 = pts[(i + 1) % pointCount];
        float dx = std::abs(p2.x - p1.x);
        float dy = std::abs(p2.y - p1.y);
        if (dx > 0.01f && dy > 0.01f) {
          axisAligned = false;
        }
      }
      if (axisAligned && rectOut != nullptr) {
        float minX = pts[0].x, maxX = pts[0].x;
        float minY = pts[0].y, maxY = pts[0].y;
        for (size_t i = 1; i < pointCount; i++) {
          minX = std::min(minX, pts[i].x);
          maxX = std::max(maxX, pts[i].x);
          minY = std::min(minY, pts[i].y);
          maxY = std::max(maxY, pts[i].y);
        }
        rectOut->w = maxX - minX;
        rectOut->h = maxY - minY;
        rectOut->cx = (minX + maxX) / 2.0f;
        rectOut->cy = (minY + maxY) / 2.0f;
        return PathPrimitive::Rectangle;
      }
    }
  }

  // Check for ellipse (4 cubic segments + close).
  if (verbs.size() == 6 && verbs[0] == PathVerb::Move && verbs[1] == PathVerb::Cubic &&
      verbs[2] == PathVerb::Cubic && verbs[3] == PathVerb::Cubic && verbs[4] == PathVerb::Cubic &&
      verbs[5] == PathVerb::Close && pts.size() >= 13) {
    Point onCurve[4];
    onCurve[0] = pts[0];
    onCurve[1] = pts[3];
    onCurve[2] = pts[6];
    onCurve[3] = pts[9];
    float minX = onCurve[0].x, maxX = onCurve[0].x;
    float minY = onCurve[0].y, maxY = onCurve[0].y;
    for (int i = 1; i < 4; i++) {
      minX = std::min(minX, onCurve[i].x);
      maxX = std::max(maxX, onCurve[i].x);
      minY = std::min(minY, onCurve[i].y);
      maxY = std::max(maxY, onCurve[i].y);
    }
    float cx = (minX + maxX) / 2.0f;
    float cy = (minY + maxY) / 2.0f;
    float rx = (maxX - minX) / 2.0f;
    float ry = (maxY - minY) / 2.0f;
    bool isEllipse = rx >= 0.01f && ry >= 0.01f;
    if (isEllipse) {
      bool foundTop = false, foundBottom = false, foundLeft = false, foundRight = false;
      static constexpr float TOLERANCE = 1.0f;
      for (int i = 0; i < 4; i++) {
        float dx = std::abs(onCurve[i].x - cx);
        float dy = std::abs(onCurve[i].y - cy);
        if (dx < TOLERANCE && std::abs(dy - ry) < TOLERANCE) {
          if (onCurve[i].y < cy) {
            foundTop = true;
          } else {
            foundBottom = true;
          }
        } else if (dy < TOLERANCE && std::abs(dx - rx) < TOLERANCE) {
          if (onCurve[i].x < cx) {
            foundLeft = true;
          } else {
            foundRight = true;
          }
        }
      }
      isEllipse = foundTop && foundBottom && foundLeft && foundRight;
    }
    if (isEllipse) {
      static constexpr float KAPPA = 0.5522847f;
      static constexpr float CP_TOLERANCE = 2.0f;
      for (int seg = 0; seg < 4 && isEllipse; seg++) {
        Point cp1 = pts[1 + seg * 3];
        Point cp2 = pts[2 + seg * 3];
        Point segStart = (seg == 0) ? pts[0] : pts[seg * 3];
        Point segEnd = pts[3 + seg * 3];
        float cp1DistX = std::abs(cp1.x - segStart.x);
        float cp1DistY = std::abs(cp1.y - segStart.y);
        float cp2DistX = std::abs(cp2.x - segEnd.x);
        float cp2DistY = std::abs(cp2.y - segEnd.y);
        float expectedCpOffsetX = rx * KAPPA;
        float expectedCpOffsetY = ry * KAPPA;
        bool cp1Valid =
            (cp1DistX < CP_TOLERANCE && std::abs(cp1DistY - expectedCpOffsetY) < CP_TOLERANCE) ||
            (cp1DistY < CP_TOLERANCE && std::abs(cp1DistX - expectedCpOffsetX) < CP_TOLERANCE);
        bool cp2Valid =
            (cp2DistX < CP_TOLERANCE && std::abs(cp2DistY - expectedCpOffsetY) < CP_TOLERANCE) ||
            (cp2DistY < CP_TOLERANCE && std::abs(cp2DistX - expectedCpOffsetX) < CP_TOLERANCE);
        isEllipse = cp1Valid && cp2Valid;
      }
    }
    if (isEllipse && ellipseOut != nullptr) {
      ellipseOut->cx = cx;
      ellipseOut->cy = cy;
      ellipseOut->w = rx * 2.0f;
      ellipseOut->h = ry * 2.0f;
      return PathPrimitive::Ellipse;
    }
  }
  return PathPrimitive::None;
}

// ============================================================================
// Optimization passes — element-level
// ============================================================================

static bool RemoveEmptyGroups(std::vector<Element*>& elements);
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
      if (IsEmptyGroup(group)) {
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
  if (!CanUnwrapFirstChildGroup(group)) {
    return false;
  }
  std::vector<Element*> newElements;
  newElements.reserve(group->elements.size() + elements.size() - 1);
  for (auto* child : group->elements) {
    newElements.push_back(child);
  }
  for (size_t i = 1; i < elements.size(); i++) {
    newElements.push_back(elements[i]);
  }
  elements = std::move(newElements);
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
    if (!HasDefaultGroupTransform(current)) {
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
      if (!HasDefaultGroupTransform(next)) {
        break;
      }
      if (!HaveSamePainterSuffix(current, next)) {
        break;
      }
      size_t nextPainterStart = FindPainterSuffixStart(next->elements);
      for (size_t k = 0; k < nextPainterStart; k++) {
        current->elements.insert(current->elements.begin() + static_cast<long>(painterStart),
                                 next->elements[k]);
        painterStart++;
      }
      elements.erase(elements.begin() + static_cast<long>(j));
      changed = true;
    }
    i = j;
  }
  return changed;
}

static bool ConvertPathsToPrimitives(std::vector<Element*>& elements, PAGXDocument* doc) {
  bool changed = false;
  for (size_t i = 0; i < elements.size(); i++) {
    if (elements[i]->nodeType() != NodeType::Path) {
      continue;
    }
    auto* path = static_cast<Path*>(elements[i]);
    RectInfo rectInfo;
    EllipseInfo ellipseInfo;
    auto primitive = DetectPathPrimitive(path, &rectInfo, &ellipseInfo);
    if (primitive == PathPrimitive::Rectangle) {
      auto* rect = doc->makeNode<Rectangle>();
      rect->position = {rectInfo.cx + path->position.x, rectInfo.cy + path->position.y};
      rect->size = {rectInfo.w, rectInfo.h};
      rect->reversed = path->reversed;
      rect->left = path->left;
      rect->right = path->right;
      rect->top = path->top;
      rect->bottom = path->bottom;
      rect->centerX = path->centerX;
      rect->centerY = path->centerY;
      rect->customData = path->customData;
      elements[i] = rect;
      changed = true;
    } else if (primitive == PathPrimitive::Ellipse) {
      auto* ellipse = doc->makeNode<Ellipse>();
      ellipse->position = {ellipseInfo.cx + path->position.x, ellipseInfo.cy + path->position.y};
      ellipse->size = {ellipseInfo.w, ellipseInfo.h};
      ellipse->reversed = path->reversed;
      ellipse->left = path->left;
      ellipse->right = path->right;
      ellipse->top = path->top;
      ellipse->bottom = path->bottom;
      ellipse->centerX = path->centerX;
      ellipse->centerY = path->centerY;
      ellipse->customData = path->customData;
      elements[i] = ellipse;
      changed = true;
    }
  }
  return changed;
}

// ============================================================================
// Optimization passes — layer-level
// ============================================================================

static bool DowngradeChildLayersToGroups(Layer* parentLayer, PAGXDocument* doc) {
  if (parentLayer->layout != LayoutMode::None || parentLayer->children.empty()) {
    return false;
  }
  if (!parentLayer->contents.empty()) {
    return false;
  }
  for (auto* child : parentLayer->children) {
    if (!CanDowngradeLayerToGroup(child)) {
      return false;
    }
  }
  for (auto* child : parentLayer->children) {
    auto* group = doc->makeNode<Group>();
    group->elements = std::move(child->contents);
    group->alpha = child->alpha;
    group->customData = std::move(child->customData);
    parentLayer->contents.push_back(group);
  }
  parentLayer->children.clear();
  return true;
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
    // Cannot replace mask with clipToBounds if the mask size differs from the layer.
    // Set clipToBounds and the layer dimensions from the mask, but only if the layer has no
    // explicit size and the mask starts at origin.
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

static void OptimizeLayerRecursive(Layer* layer, PAGXDocument* doc, bool /*parentHasLayout*/,
                                   bool& changed) {
  // Recurse into children first (bottom-up).
  bool thisHasLayout = layer->layout != LayoutMode::None;
  auto it = layer->children.begin();
  while (it != layer->children.end()) {
    auto* child = *it;
    OptimizeLayerRecursive(child, doc, thisHasLayout, changed);
    if (IsEmptyLayer(child, thisHasLayout)) {
      it = layer->children.erase(it);
      changed = true;
      continue;
    }
    ++it;
  }

  // Try to downgrade all children to Groups.
  if (DowngradeChildLayersToGroups(layer, doc)) {
    changed = true;
  }

  // Convert rectangular mask to clipToBounds.
  if (ConvertRectMaskToClipToBounds(layer)) {
    changed = true;
  }

  // Optimize the contents element tree.
  OptimizeElements(layer->contents, doc, changed);
}

// ============================================================================
// Duplicate PathData deduplication
// ============================================================================

static bool DeduplicatePathData(PAGXDocument* document) {
  // Collect all PathData nodes and group by content.
  struct PathDataSig {
    size_t verbCount = 0;
    size_t pointCount = 0;
  };

  // Build a map: canonical PathData* -> list of duplicate PathData*.
  std::vector<PathData*> allPathData;
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::PathData) {
      allPathData.push_back(static_cast<PathData*>(node.get()));
    }
  }
  if (allPathData.size() < 2) {
    return false;
  }

  // Compare PathData by verbs and points.
  auto pathDataEquals = [](const PathData* a, const PathData* b) -> bool {
    auto& av = a->verbs();
    auto& bv = b->verbs();
    if (av.size() != bv.size()) {
      return false;
    }
    for (size_t i = 0; i < av.size(); i++) {
      if (av[i] != bv[i]) {
        return false;
      }
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
  };

  // Map each PathData to a canonical representative.
  std::unordered_map<PathData*, PathData*> replacements;
  for (size_t i = 0; i < allPathData.size(); i++) {
    if (replacements.count(allPathData[i])) {
      continue;
    }
    for (size_t j = i + 1; j < allPathData.size(); j++) {
      if (replacements.count(allPathData[j])) {
        continue;
      }
      if (pathDataEquals(allPathData[i], allPathData[j])) {
        replacements[allPathData[j]] = allPathData[i];
      }
    }
  }
  if (replacements.empty()) {
    return false;
  }

  // Replace references in all Path elements.
  std::function<void(Layer*)> replaceInLayer;
  std::function<void(const std::vector<Element*>&)> replaceInElements;
  replaceInElements = [&](const std::vector<Element*>& elements) {
    for (auto* element : elements) {
      if (element->nodeType() == NodeType::Path) {
        auto* path = static_cast<Path*>(element);
        auto it2 = replacements.find(path->data);
        if (it2 != replacements.end()) {
          path->data = it2->second;
        }
      } else if (element->nodeType() == NodeType::Group ||
                 element->nodeType() == NodeType::TextBox) {
        auto* group = static_cast<Group*>(element);
        replaceInElements(group->elements);
      }
    }
  };

  replaceInLayer = [&](Layer* layer) {
    replaceInElements(layer->contents);
    for (auto* child : layer->children) {
      replaceInLayer(child);
    }
    if (layer->mask != nullptr) {
      replaceInLayer(layer->mask);
    }
  };

  for (auto* layer : document->layers) {
    replaceInLayer(layer);
  }
  return true;
}

// ============================================================================
// Unreferenced resource cleanup
// ============================================================================

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
      if (fill->color != nullptr) {
        refs.insert(fill->color);
      }
    } else if (type == NodeType::Stroke) {
      auto* stroke = static_cast<Stroke*>(element);
      if (stroke->color != nullptr) {
        refs.insert(stroke->color);
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

  // Pass 1: Recursive layer optimization (bottom-up). Handles: empty node removal,
  // Layer→Group downgrade, mask→clipToBounds, element optimizations (unwrap, merge, path→primitive).
  for (auto* layer : document->layers) {
    OptimizeLayerRecursive(layer, document, false, changed);
  }

  // Pass 2: Remove top-level empty layers.
  auto it = document->layers.begin();
  while (it != document->layers.end()) {
    if (IsEmptyLayer(*it, false)) {
      it = document->layers.erase(it);
      changed = true;
    } else {
      ++it;
    }
  }

  // Pass 3: Deduplicate PathData references.
  if (DeduplicatePathData(document)) {
    changed = true;
  }

  // Pass 4: Remove unreferenced resources (PathData, gradients, etc. orphaned by earlier passes).
  if (RemoveUnreferencedResources(document)) {
    changed = true;
  }

  return changed;
}

}  // namespace pagx
