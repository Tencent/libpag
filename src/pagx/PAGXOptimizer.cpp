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
#include <cstring>
#include <unordered_map>
#include <unordered_set>
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
#include "pagx/types/PathVerb.h"
#include "pagx/utils/VerifyUtils.h"

namespace pagx {

// ============================================================================
// Local helpers. `HasLayerOnlyFeatures`, `IsLayerShell`, `IsPainter`, and
// `ContainsPainter` are the shared definitions from pagx/utils/VerifyUtils.h
// (same semantics used by the cli verify / resolve commands) — keeping them in
// one place avoids silent drift when new Layer attributes are introduced.
// ============================================================================

namespace {

bool HasUnresolvedImport(const Layer* layer) {
  return !layer->importDirective.source.empty() || !layer->importDirective.content.empty();
}

bool LayerNeedsKeeping(const Layer* layer, const std::unordered_set<const Layer*>& maskRefs) {
  if (maskRefs.find(layer) != maskRefs.end()) return true;
  return false;
}

bool LayoutNodeHasConstraints(const LayoutNode* node) {
  if (node == nullptr) return false;
  return !std::isnan(node->left) || !std::isnan(node->right) || !std::isnan(node->top) ||
         !std::isnan(node->bottom) || !std::isnan(node->centerX) || !std::isnan(node->centerY);
}

bool ElementHasConstraints(Element* element) {
  return LayoutNodeHasConstraints(LayoutNode::AsLayoutNode(element));
}

// Group is a no-op container: identity transform, default alpha, no layout,
// no constraint frame, no padding.
bool IsDefaultTransformGroup(const Group* group) {
  if (group->alpha != 1.0f) return false;
  if (group->position.x != 0 || group->position.y != 0) return false;
  if (group->anchor.x != 0 || group->anchor.y != 0) return false;
  if (group->rotation != 0) return false;
  if (group->scale.x != 1 || group->scale.y != 1) return false;
  if (group->skew != 0) return false;
  if (group->skewAxis != 0) return false;
  if (!std::isnan(group->width) || !std::isnan(group->height)) return false;
  if (!group->padding.isZero()) return false;
  if (!std::isnan(group->left) || !std::isnan(group->right) || !std::isnan(group->top) ||
      !std::isnan(group->bottom) || !std::isnan(group->centerX) || !std::isnan(group->centerY)) {
    return false;
  }
  return true;
}

void CollectMaskRefs(const std::vector<Layer*>& layers, std::unordered_set<const Layer*>& refs);

void CollectMaskRefsFromLayer(const Layer* layer, std::unordered_set<const Layer*>& refs) {
  if (layer == nullptr) return;
  if (layer->mask != nullptr) {
    refs.insert(layer->mask);
    CollectMaskRefsFromLayer(layer->mask, refs);
  }
  CollectMaskRefs(layer->children, refs);
}

void CollectMaskRefs(const std::vector<Layer*>& layers, std::unordered_set<const Layer*>& refs) {
  for (auto* layer : layers) {
    CollectMaskRefsFromLayer(layer, refs);
  }
}

// ----------------------------------------------------------------------------
// Layer -> Group conversion
// ----------------------------------------------------------------------------

// Wraps a downgradable shell Layer's contents in a new Group, transferring customData. The
// caller is responsible for releasing the old Layer reference. Returns nullptr if the layer
// is not safely downgradable (callers must check IsLayerShell + children.empty() first).
Group* WrapShellLayerAsGroup(PAGXDocument* doc, Layer* layer) {
  auto* group = doc->makeNode<Group>();
  group->elements = std::move(layer->contents);
  layer->contents.clear();
  // Transfer customData and sourceLine so verify diagnostics still point to the right place.
  group->customData = std::move(layer->customData);
  layer->customData.clear();
  if (group->sourceLine == -1) {
    group->sourceLine = layer->sourceLine;
  }
  return group;
}

// ----------------------------------------------------------------------------
// Path canonicalization (mirrors verify's DetectPathToPrimitives).
// ----------------------------------------------------------------------------

bool TryReadAxisAlignedRect(const PathData* data, float& cx, float& cy, float& w, float& h) {
  auto& verbs = data->verbs();
  if (verbs.size() != 5 && verbs.size() != 6) return false;
  if (verbs[0] != PathVerb::Move) return false;
  for (size_t i = 1; i + 1 < verbs.size(); i++) {
    if (verbs[i] != PathVerb::Line) return false;
  }
  if (verbs.back() != PathVerb::Close) return false;
  auto& pts = data->points();
  size_t pointCount = pts.size();
  if (pointCount < 4) return false;
  for (size_t i = 0; i < pointCount; i++) {
    auto& p1 = pts[i];
    auto& p2 = pts[(i + 1) % pointCount];
    float dx = std::abs(p2.x - p1.x);
    float dy = std::abs(p2.y - p1.y);
    if (dx > 0.01f && dy > 0.01f) return false;
  }
  float minX = pts[0].x, maxX = pts[0].x;
  float minY = pts[0].y, maxY = pts[0].y;
  for (size_t i = 1; i < pointCount; i++) {
    minX = std::min(minX, pts[i].x);
    maxX = std::max(maxX, pts[i].x);
    minY = std::min(minY, pts[i].y);
    maxY = std::max(maxY, pts[i].y);
  }
  w = maxX - minX;
  h = maxY - minY;
  if (w < 0.01f || h < 0.01f) return false;
  cx = (minX + maxX) * 0.5f;
  cy = (minY + maxY) * 0.5f;
  return true;
}

bool TryReadEllipse(const PathData* data, float& cx, float& cy, float& w, float& h) {
  auto& verbs = data->verbs();
  if (verbs.size() != 6) return false;
  if (verbs[0] != PathVerb::Move) return false;
  for (int i = 1; i <= 4; i++) {
    if (verbs[i] != PathVerb::Cubic) return false;
  }
  if (verbs[5] != PathVerb::Close) return false;
  auto& pts = data->points();
  if (pts.size() < 13) return false;

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
  cx = (minX + maxX) * 0.5f;
  cy = (minY + maxY) * 0.5f;
  float rx = (maxX - minX) * 0.5f;
  float ry = (maxY - minY) * 0.5f;
  if (rx < 0.01f || ry < 0.01f) return false;

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
  if (!(foundTop && foundBottom && foundLeft && foundRight)) return false;

  static constexpr float KAPPA = 0.5522847f;
  static constexpr float CP_TOLERANCE = 2.0f;
  float expectedCpOffsetX = rx * KAPPA;
  float expectedCpOffsetY = ry * KAPPA;
  for (int seg = 0; seg < 4; seg++) {
    Point cp1 = pts[1 + seg * 3];
    Point cp2 = pts[2 + seg * 3];
    Point segStart = (seg == 0) ? pts[0] : pts[seg * 3];
    Point segEnd = pts[3 + seg * 3];
    float cp1DistX = std::abs(cp1.x - segStart.x);
    float cp1DistY = std::abs(cp1.y - segStart.y);
    float cp2DistX = std::abs(cp2.x - segEnd.x);
    float cp2DistY = std::abs(cp2.y - segEnd.y);
    bool cp1Valid =
        (cp1DistX < CP_TOLERANCE && std::abs(cp1DistY - expectedCpOffsetY) < CP_TOLERANCE) ||
        (cp1DistY < CP_TOLERANCE && std::abs(cp1DistX - expectedCpOffsetX) < CP_TOLERANCE);
    bool cp2Valid =
        (cp2DistX < CP_TOLERANCE && std::abs(cp2DistY - expectedCpOffsetY) < CP_TOLERANCE) ||
        (cp2DistY < CP_TOLERANCE && std::abs(cp2DistX - expectedCpOffsetX) < CP_TOLERANCE);
    if (!cp1Valid || !cp2Valid) return false;
  }
  w = rx * 2.0f;
  h = ry * 2.0f;
  return true;
}

// Replace `path` in-place with a Rectangle/Ellipse if eligible. Returns the new element to
// substitute (which may be the same path if no rewrite happened).
Element* TryCanonicalizePath(PAGXDocument* doc, Path* path) {
  if (path->data == nullptr || path->data->isEmpty()) return path;
  if (path->reversed) return path;
  if (ElementHasConstraints(path)) return path;
  if (!path->customData.empty()) return path;

  float cx = 0, cy = 0, w = 0, h = 0;
  if (TryReadAxisAlignedRect(path->data, cx, cy, w, h)) {
    auto* rect = doc->makeNode<Rectangle>();
    rect->position = {path->position.x + cx, path->position.y + cy};
    rect->size = {w, h};
    rect->sourceLine = path->sourceLine;
    return rect;
  }
  if (TryReadEllipse(path->data, cx, cy, w, h)) {
    auto* ellipse = doc->makeNode<Ellipse>();
    ellipse->position = {path->position.x + cx, path->position.y + cy};
    ellipse->size = {w, h};
    ellipse->sourceLine = path->sourceLine;
    return ellipse;
  }
  return path;
}

// ----------------------------------------------------------------------------
// Rectangular alpha mask -> scrollRect rewrite.
//
// An alpha mask whose only contents are a single axis-aligned Rectangle painted with an opaque
// Fill is exactly equivalent to clipping the parent layer with that same rectangle. We express
// the result via scrollRect (which lives in the layer's local coordinate space) rather than
// clipToBounds, because clipToBounds requires resolved layer bounds and would shift coordinates
// when the layer doesn't already declare a frame.
// ----------------------------------------------------------------------------

bool TryConvertRectMaskToScrollRect(Layer* layer) {
  if (layer->mask == nullptr) return false;
  if (layer->maskType != MaskType::Alpha) return false;
  if (layer->hasScrollRect) return false;
  auto* m = layer->mask;
  if (m->x != 0 || m->y != 0) return false;
  if (!m->matrix.isIdentity()) return false;
  if (!m->matrix3D.isIdentity()) return false;
  if (m->alpha != 1.0f) return false;
  if (m->blendMode != BlendMode::Normal) return false;
  if (!m->styles.empty() || !m->filters.empty()) return false;
  // `id` / `name` on the mask layer come from SVG <clipPath>'s identifier and don't carry
  // user-meaningful state — the mask layer itself stays in doc->layers (hidden) after we
  // detach the mask reference, so any other consumer can still resolve it. Only customData
  // truly blocks the rewrite.
  if (!m->customData.empty()) return false;
  if (m->contents.size() != 2) return false;
  if (m->contents[0]->nodeType() != NodeType::Rectangle) return false;
  if (m->contents[1]->nodeType() != NodeType::Fill) return false;
  auto* rect = static_cast<Rectangle*>(m->contents[0]);
  if (rect->roundness != 0) return false;
  if (rect->reversed) return false;
  if (LayoutNodeHasConstraints(rect)) return false;
  if (!rect->customData.empty()) return false;
  auto* fill = static_cast<Fill*>(m->contents[1]);
  if (fill->alpha < 0.999f) return false;
  if (fill->blendMode != BlendMode::Normal) return false;
  if (fill->color != nullptr) {
    if (fill->color->nodeType() != NodeType::SolidColor) return false;
    auto* sc = static_cast<const SolidColor*>(fill->color);
    if (sc->color.alpha < 0.999f) return false;
  }
  // tgfx's scrollRect uses (x,y) as a scroll offset (the rect's top-left maps to the layer's
  // local (0,0)). To preserve the original positioning we have to translate the layer by the
  // same (left, top) — which we can only do safely when the user layer has no rotation/scale,
  // no 3D matrix, and no constraint-based positioning.
  if (!layer->matrix.isIdentity()) return false;
  if (!layer->matrix3D.isIdentity()) return false;
  if (LayoutNodeHasConstraints(layer)) return false;
  // If the layer already declares an explicit frame size, leave it alone — overwriting an
  // author-supplied width/height could change layout semantics for downstream consumers.
  if (!std::isnan(layer->width) || !std::isnan(layer->height)) return false;
  float left = rect->position.x - rect->size.width * 0.5f;
  float top = rect->position.y - rect->size.height * 0.5f;
  layer->scrollRect = {left, top, rect->size.width, rect->size.height};
  layer->hasScrollRect = true;
  layer->x += left;
  layer->y += top;
  // The displayed area collapses to exactly the scrollRect's size after this rewrite. Pin the
  // layer's frame to those dimensions so layoutBounds() reflects the visible region instead of
  // the (much larger) un-clipped child extent — otherwise verify's child-exceeds-parent check
  // (which only suppresses on the parent's clip, not the child's) would fire false positives.
  layer->width = rect->size.width;
  layer->height = rect->size.height;
  layer->mask = nullptr;
  return true;
}

bool RectMaskToScrollRectInLayer(Layer* layer) {
  bool changed = TryConvertRectMaskToScrollRect(layer);
  for (auto* child : layer->children) {
    changed |= RectMaskToScrollRectInLayer(child);
  }
  return changed;
}

// ----------------------------------------------------------------------------
// Painter signature (used to decide whether two Groups can merge geometry).
// ----------------------------------------------------------------------------

// A painter's color source can be either a shared resource (compare by pointer identity, since
// resources have meaningful IDs) or an inline literal. Two inline literals are considered equal
// only when both are SolidColor with identical Color values. Anything else (gradients, image
// patterns) is treated as opaque: only equal when the SAME pointer is used.
bool ColorSourcesEqual(const ColorSource* a, const ColorSource* b) {
  if (a == b) return true;
  if (a == nullptr || b == nullptr) return false;
  if (a->nodeType() != b->nodeType()) return false;
  // For shared resource references (i.e. nodes registered in PAGXDocument::nodes by id) the
  // SVG importer reuses the same instance, so a == b will already be true above. For inline
  // SolidColor literals we compare values.
  if (a->nodeType() == NodeType::SolidColor) {
    auto* sa = static_cast<const SolidColor*>(a);
    auto* sb = static_cast<const SolidColor*>(b);
    return sa->color == sb->color;
  }
  return false;
}

bool FillsEqual(const Fill* a, const Fill* b) {
  if (a->alpha != b->alpha) return false;
  if (a->blendMode != b->blendMode) return false;
  if (a->fillRule != b->fillRule) return false;
  if (a->placement != b->placement) return false;
  return ColorSourcesEqual(a->color, b->color);
}

bool StrokesEqual(const Stroke* a, const Stroke* b) {
  if (a->width != b->width) return false;
  if (a->alpha != b->alpha) return false;
  if (a->blendMode != b->blendMode) return false;
  if (a->cap != b->cap) return false;
  if (a->join != b->join) return false;
  if (a->miterLimit != b->miterLimit) return false;
  if (a->dashOffset != b->dashOffset) return false;
  if (a->dashAdaptive != b->dashAdaptive) return false;
  if (a->align != b->align) return false;
  if (a->placement != b->placement) return false;
  if (a->dashes != b->dashes) return false;
  return ColorSourcesEqual(a->color, b->color);
}

bool PaintersEqual(const Element* a, const Element* b) {
  if (a == b) return true;
  if (a == nullptr || b == nullptr) return false;
  if (a->nodeType() != b->nodeType()) return false;
  if (a->nodeType() == NodeType::Fill) {
    return FillsEqual(static_cast<const Fill*>(a), static_cast<const Fill*>(b));
  }
  if (a->nodeType() == NodeType::Stroke) {
    return StrokesEqual(static_cast<const Stroke*>(a), static_cast<const Stroke*>(b));
  }
  return false;
}

struct PainterSignature {
  // Trailing-painter elements; an empty signature means the Group has no painters
  // (so it has no shared painter scope to compare).
  std::vector<Element*> painters;
  bool valid = false;
};

PainterSignature ComputePainterSignature(const Group* group) {
  PainterSignature sig;
  bool foundPainter = false;
  for (auto* el : group->elements) {
    if (IsPainter(el->nodeType())) {
      foundPainter = true;
      sig.painters.push_back(el);
    } else if (foundPainter) {
      // A non-painter after a painter means painters don't form a clean trailing block.
      sig.valid = false;
      sig.painters.clear();
      return sig;
    }
  }
  if (!foundPainter) {
    return sig;
  }
  sig.valid = true;
  return sig;
}

bool PainterSignaturesEqual(const PainterSignature& a, const PainterSignature& b) {
  if (!a.valid || !b.valid) return false;
  if (a.painters.size() != b.painters.size()) return false;
  for (size_t i = 0; i < a.painters.size(); i++) {
    if (!PaintersEqual(a.painters[i], b.painters[i])) return false;
  }
  return true;
}

// ============================================================================
// Rule implementations. Each rule mutates `doc` in place and returns true if
// any change was made.
// ============================================================================

// Recursively prune empty Layers (no contents/children/composition, no constraints, not
// referenced as a mask, not a layout participant) and empty Groups.
bool PruneEmptyInLayer(Layer* layer, const std::unordered_set<const Layer*>& maskRefs);
bool PruneEmptyInGroup(Group* group);

bool PruneEmptyInElements(std::vector<Element*>& elements) {
  bool changed = false;
  size_t writeIdx = 0;
  for (size_t i = 0; i < elements.size(); i++) {
    auto* el = elements[i];
    if (el->nodeType() == NodeType::Group) {
      auto* group = static_cast<Group*>(el);
      changed |= PruneEmptyInGroup(group);
      bool empty = group->elements.empty() && std::isnan(group->width) &&
                   std::isnan(group->height) && !LayoutNodeHasConstraints(group) &&
                   group->padding.isZero() && group->customData.empty();
      if (empty) {
        changed = true;
        continue;
      }
    }
    elements[writeIdx++] = el;
  }
  if (writeIdx != elements.size()) {
    elements.resize(writeIdx);
    changed = true;
  }
  return changed;
}

bool PruneEmptyInGroup(Group* group) {
  return PruneEmptyInElements(group->elements);
}

bool PruneEmptyInLayer(Layer* layer, const std::unordered_set<const Layer*>& maskRefs) {
  bool changed = false;
  changed |= PruneEmptyInElements(layer->contents);
  bool layerHasLayout = layer->layout != LayoutMode::None;
  // Recurse into children first so empty subtrees collapse bottom-up.
  size_t writeIdx = 0;
  for (size_t i = 0; i < layer->children.size(); i++) {
    auto* child = layer->children[i];
    changed |= PruneEmptyInLayer(child, maskRefs);
    bool empty = child->contents.empty() && child->children.empty() &&
                 child->composition == nullptr && std::isnan(child->width) &&
                 std::isnan(child->height) && !LayoutNodeHasConstraints(child) &&
                 child->customData.empty() && !HasUnresolvedImport(child);
    bool inParentLayout = layerHasLayout && child->includeInLayout;
    if (empty && !inParentLayout && !LayerNeedsKeeping(child, maskRefs) &&
        !HasLayerOnlyFeatures(child)) {
      changed = true;
      continue;
    }
    layer->children[writeIdx++] = child;
  }
  if (writeIdx != layer->children.size()) {
    layer->children.resize(writeIdx);
    changed = true;
  }
  return changed;
}

bool PruneEmptyTopLevel(std::vector<Layer*>& layers, bool parentHasLayout,
                        const std::unordered_set<const Layer*>& maskRefs) {
  bool changed = false;
  size_t writeIdx = 0;
  for (size_t i = 0; i < layers.size(); i++) {
    auto* layer = layers[i];
    changed |= PruneEmptyInLayer(layer, maskRefs);
    bool empty = layer->contents.empty() && layer->children.empty() &&
                 layer->composition == nullptr && std::isnan(layer->width) &&
                 std::isnan(layer->height) && !LayoutNodeHasConstraints(layer) &&
                 layer->customData.empty() && !HasUnresolvedImport(layer);
    bool inParentLayout = parentHasLayout && layer->includeInLayout;
    if (empty && !inParentLayout && !LayerNeedsKeeping(layer, maskRefs) &&
        !HasLayerOnlyFeatures(layer)) {
      changed = true;
      continue;
    }
    layers[writeIdx++] = layer;
  }
  if (writeIdx != layers.size()) {
    layers.resize(writeIdx);
    changed = true;
  }
  return changed;
}

// ----------------------------------------------------------------------------
// Canonicalize Path nodes inside Layers/Groups.
// ----------------------------------------------------------------------------

bool CanonicalizePathsInElements(PAGXDocument* doc, std::vector<Element*>& elements) {
  bool changed = false;
  for (auto& el : elements) {
    if (el->nodeType() == NodeType::Path) {
      auto* path = static_cast<Path*>(el);
      auto* replacement = TryCanonicalizePath(doc, path);
      if (replacement != el) {
        el = replacement;
        changed = true;
      }
    } else if (el->nodeType() == NodeType::Group) {
      auto* group = static_cast<Group*>(el);
      changed |= CanonicalizePathsInElements(doc, group->elements);
    }
  }
  return changed;
}

bool CanonicalizePathsInLayer(PAGXDocument* doc, Layer* layer) {
  bool changed = false;
  changed |= CanonicalizePathsInElements(doc, layer->contents);
  for (auto* child : layer->children) {
    changed |= CanonicalizePathsInLayer(doc, child);
  }
  return changed;
}

// ----------------------------------------------------------------------------
// Downgrade-shell-children: when a parent Layer has no contents, no layout,
// and ALL children are shell+downgradable Layers without further children,
// rewrite each child as a Group inside the parent's contents.
// ----------------------------------------------------------------------------

bool DowngradeShellChildrenInLayer(PAGXDocument* doc, Layer* layer,
                                   const std::unordered_set<const Layer*>& maskRefs) {
  bool changed = false;
  for (auto* child : layer->children) {
    changed |= DowngradeShellChildrenInLayer(doc, child, maskRefs);
  }
  if (layer->layout != LayoutMode::None) return changed;
  if (layer->children.empty()) return changed;
  if (!layer->contents.empty()) return changed;  // mixed: paint order would change

  for (auto* child : layer->children) {
    if (!child->children.empty()) return changed;
    if (!IsLayerShell(child)) return changed;
    if (LayerNeedsKeeping(child, maskRefs)) return changed;
    if (HasUnresolvedImport(child)) return changed;
  }
  // All children are downgradable.
  for (auto* child : layer->children) {
    if (child->contents.empty() && child->customData.empty()) {
      // Empty shell layer with nothing to wrap — drop entirely.
      continue;
    }
    auto* group = WrapShellLayerAsGroup(doc, child);
    layer->contents.push_back(group);
  }
  layer->children.clear();
  return true;
}

bool DowngradeShellChildren(PAGXDocument* doc, std::vector<Layer*>& layers,
                            const std::unordered_set<const Layer*>& maskRefs) {
  bool changed = false;
  for (auto* layer : layers) {
    changed |= DowngradeShellChildrenInLayer(doc, layer, maskRefs);
  }
  return changed;
}

// ----------------------------------------------------------------------------
// Merge-adjacent-shell-layers: in a sibling list, runs of consecutive shell
// Layers (no children, downgradable) are replaced by a single Layer containing
// one Group per source Layer.
//
// The single Layer wrapper is necessary at the top level (and for a child run
// when there are surrounding non-downgradable siblings) so paint order is
// preserved. When the entire sibling list is downgradable inside a parent
// Layer, DowngradeShellChildren handles it earlier (no wrapper needed).
// ----------------------------------------------------------------------------

bool MergeAdjacentShellLayersInList(PAGXDocument* doc, std::vector<Layer*>& layers,
                                    bool parentHasLayout,
                                    const std::unordered_set<const Layer*>& maskRefs) {
  if (parentHasLayout) return false;
  if (layers.size() < 2) return false;

  auto isMergeable = [&](const Layer* l) {
    return !l->children.empty()
               ? false
               : (IsLayerShell(l) && !LayerNeedsKeeping(l, maskRefs) && !HasUnresolvedImport(l));
  };

  // The common case is "no adjacent shell run" — we want to avoid allocating a throwaway
  // result vector for that case. Defer allocation until the first merge is detected, then
  // backfill everything we've seen so far into the result.
  std::vector<Layer*> result;
  bool changed = false;
  auto beginMutation = [&](size_t upTo) {
    if (changed) return;
    changed = true;
    result.reserve(layers.size());
    result.insert(result.end(), layers.begin(), layers.begin() + static_cast<ptrdiff_t>(upTo));
  };
  size_t i = 0;
  while (i < layers.size()) {
    if (!isMergeable(layers[i])) {
      if (changed) result.push_back(layers[i]);
      i++;
      continue;
    }
    size_t j = i + 1;
    while (j < layers.size() && isMergeable(layers[j])) {
      j++;
    }
    if (j - i < 2) {
      if (changed) result.push_back(layers[i]);
      i++;
      continue;
    }
    beginMutation(i);
    // Build a new wrapper layer containing one Group per source.
    auto* wrapper = doc->makeNode<Layer>();
    wrapper->sourceLine = layers[i]->sourceLine;
    for (size_t k = i; k < j; k++) {
      auto* src = layers[k];
      if (src->contents.empty() && src->customData.empty()) {
        continue;  // nothing to wrap, drop
      }
      auto* group = WrapShellLayerAsGroup(doc, src);
      wrapper->contents.push_back(group);
    }
    // If every source was empty, skip the wrapper too.
    if (!wrapper->contents.empty()) {
      result.push_back(wrapper);
    }
    i = j;
  }
  if (!changed) return false;
  layers = std::move(result);
  return true;
}

bool MergeAdjacentShellLayersRecursive(PAGXDocument* doc, std::vector<Layer*>& layers,
                                       bool parentHasLayout,
                                       const std::unordered_set<const Layer*>& maskRefs) {
  bool changed = MergeAdjacentShellLayersInList(doc, layers, parentHasLayout, maskRefs);
  for (auto* layer : layers) {
    bool layerHasLayout = layer->layout != LayoutMode::None;
    changed |= MergeAdjacentShellLayersRecursive(doc, layer->children, layerHasLayout, maskRefs);
  }
  return changed;
}

bool MergeAdjacentShellLayers(PAGXDocument* doc, std::vector<Layer*>& topLevel,
                              const std::unordered_set<const Layer*>& maskRefs) {
  return MergeAdjacentShellLayersRecursive(doc, topLevel, /*parentHasLayout=*/false, maskRefs);
}

// ----------------------------------------------------------------------------
// Unwrap redundant first-child Group: when a Layer/Group's first content child
// is a default-transform Group with no painter that would leak into following
// siblings, splice its elements in place of the Group.
// ----------------------------------------------------------------------------

bool UnwrapRedundantFirstGroupInElements(std::vector<Element*>& elements) {
  if (elements.empty()) return false;
  auto* first = elements[0];
  if (first->nodeType() != NodeType::Group) return false;
  auto* group = static_cast<Group*>(first);
  if (!IsDefaultTransformGroup(group)) return false;
  if (!group->customData.empty()) return false;
  // Painter-leak guard: if the parent has more siblings AND the group contains painters,
  // unwrapping would let those painters bleed into following geometry.
  if (elements.size() > 1 && ContainsPainter(group->elements)) return false;
  // Layout guard: any direct child element using right/bottom/centerX/centerY needs the group
  // as a constraint frame (this matches verify's CanUnwrapFirstChildGroup).
  for (auto* child : group->elements) {
    auto* layoutNode = LayoutNode::AsLayoutNode(child);
    if (layoutNode == nullptr) continue;
    if (!std::isnan(layoutNode->right) || !std::isnan(layoutNode->bottom) ||
        !std::isnan(layoutNode->centerX) || !std::isnan(layoutNode->centerY)) {
      return false;
    }
  }
  std::vector<Element*> spliced;
  spliced.reserve(elements.size() - 1 + group->elements.size());
  for (auto* gc : group->elements) spliced.push_back(gc);
  for (size_t i = 1; i < elements.size(); i++) spliced.push_back(elements[i]);
  elements = std::move(spliced);
  return true;
}

bool UnwrapRedundantFirstGroupRecursive(std::vector<Element*>& elements) {
  bool changed = UnwrapRedundantFirstGroupInElements(elements);
  // Even without changes at this level, recurse into nested Groups.
  for (auto* el : elements) {
    if (el->nodeType() == NodeType::Group) {
      auto* group = static_cast<Group*>(el);
      changed |= UnwrapRedundantFirstGroupRecursive(group->elements);
    }
  }
  return changed;
}

bool UnwrapRedundantFirstGroupInLayer(Layer* layer) {
  bool changed = UnwrapRedundantFirstGroupRecursive(layer->contents);
  for (auto* child : layer->children) {
    changed |= UnwrapRedundantFirstGroupInLayer(child);
  }
  return changed;
}

// ----------------------------------------------------------------------------
// Merge consecutive Groups that share identical painters (same Fill/Stroke
// trailing block). Geometry from the second Group onwards is folded into the
// first; the duplicate painters of the second Group are dropped.
// ----------------------------------------------------------------------------

bool MergeAdjacentGroupsInElements(std::vector<Element*>& elements) {
  if (elements.size() < 2) return false;
  // Defer allocating `result` until the first merge actually occurs: most sibling lists don't
  // contain a mergeable pair, so in the common case we skip the vector entirely.
  std::vector<Element*> result;
  bool changed = false;
  auto beginMutation = [&](size_t upTo) {
    if (changed) return;
    changed = true;
    result.reserve(elements.size());
    result.insert(result.end(), elements.begin(), elements.begin() + static_cast<ptrdiff_t>(upTo));
  };
  auto keepCurrent = [&](Element* el) {
    if (changed) result.push_back(el);
  };
  size_t i = 0;
  while (i < elements.size()) {
    auto* el = elements[i];
    if (el->nodeType() != NodeType::Group) {
      keepCurrent(el);
      i++;
      continue;
    }
    auto* baseGroup = static_cast<Group*>(el);
    if (!IsDefaultTransformGroup(baseGroup) || !baseGroup->customData.empty()) {
      keepCurrent(el);
      i++;
      continue;
    }
    auto baseSig = ComputePainterSignature(baseGroup);
    if (!baseSig.valid) {
      keepCurrent(el);
      i++;
      continue;
    }
    size_t j = i + 1;
    while (j < elements.size()) {
      auto* next = elements[j];
      if (next->nodeType() != NodeType::Group) break;
      auto* nextGroup = static_cast<Group*>(next);
      if (!IsDefaultTransformGroup(nextGroup) || !nextGroup->customData.empty()) break;
      auto nextSig = ComputePainterSignature(nextGroup);
      if (!PainterSignaturesEqual(baseSig, nextSig)) break;

      // Insert nextGroup's geometry (everything before its trailing painters) into baseGroup
      // BEFORE baseGroup's painters. To preserve ordering, splice the new geometry just before
      // the first painter in baseGroup's elements.
      size_t firstPainterIdx = baseGroup->elements.size();
      for (size_t k = 0; k < baseGroup->elements.size(); k++) {
        if (IsPainter(baseGroup->elements[k]->nodeType())) {
          firstPainterIdx = k;
          break;
        }
      }
      // Geometry slice from nextGroup: everything before its painters.
      size_t nextFirstPainterIdx = nextGroup->elements.size();
      for (size_t k = 0; k < nextGroup->elements.size(); k++) {
        if (IsPainter(nextGroup->elements[k]->nodeType())) {
          nextFirstPainterIdx = k;
          break;
        }
      }
      std::vector<Element*> updated;
      updated.reserve(baseGroup->elements.size() + nextFirstPainterIdx);
      for (size_t k = 0; k < firstPainterIdx; k++) updated.push_back(baseGroup->elements[k]);
      for (size_t k = 0; k < nextFirstPainterIdx; k++) updated.push_back(nextGroup->elements[k]);
      for (size_t k = firstPainterIdx; k < baseGroup->elements.size(); k++) {
        updated.push_back(baseGroup->elements[k]);
      }
      baseGroup->elements = std::move(updated);
      // Drop nextGroup entirely (its painters were duplicates of baseGroup's).
      beginMutation(i);
      j++;
    }
    if (changed) result.push_back(baseGroup);
    i = j;
  }
  if (!changed) return false;
  elements = std::move(result);
  return true;
}

bool MergeAdjacentGroupsRecursive(std::vector<Element*>& elements) {
  bool changed = MergeAdjacentGroupsInElements(elements);
  for (auto* el : elements) {
    if (el->nodeType() == NodeType::Group) {
      auto* group = static_cast<Group*>(el);
      changed |= MergeAdjacentGroupsRecursive(group->elements);
    }
  }
  return changed;
}

bool MergeAdjacentGroupsInLayer(Layer* layer) {
  bool changed = MergeAdjacentGroupsRecursive(layer->contents);
  for (auto* child : layer->children) {
    changed |= MergeAdjacentGroupsInLayer(child);
  }
  return changed;
}

// ============================================================================
// Driver
// ============================================================================

// Recompute the set of mask references reachable from `topLevel` plus all Composition layer
// trees in the document. Some rules (e.g. RectMaskToScrollRect) detach Layer.mask pointers,
// so the maskRefs cache must be refreshed at every iteration.
void RecomputeMaskRefs(PAGXDocument* doc, std::unordered_set<const Layer*>& refs) {
  refs.clear();
  CollectMaskRefs(doc->layers, refs);
  for (auto& node : doc->nodes) {
    if (node->nodeType() == NodeType::Composition) {
      auto* comp = static_cast<Composition*>(node.get());
      CollectMaskRefs(comp->layers, refs);
    }
  }
}

// ----------------------------------------------------------------------------
// Resource dedup / prune passes.
// Run AFTER per-layer rules so any references redirected by canonicalize/simplify
// are reflected here.
// ----------------------------------------------------------------------------

// Build a content-only signature for a PathData (verbs + points). Distinct ids /
// names / sourceLines do not contribute, so structurally equal resources collide.
// Packed binary layout (verb bytes, then raw float bits for each point) is used instead of a
// textual encoding: it is dramatically faster than ostringstream-based formatting and preserves
// exact bit equality, so two paths collide only when every coordinate matches to the last bit.
std::string PathDataSignature(const PathData* data) {
  static_assert(sizeof(Point) == 2 * sizeof(float),
                "Point must be packed {x,y}; signature relies on it");
  const auto& verbs = data->verbs();
  const auto& pts = data->points();
  const size_t pointBytes = pts.size() * sizeof(Point);
  std::string sig;
  sig.resize(verbs.size() + 1 + pointBytes);
  char* out = sig.data();
  for (auto v : verbs) {
    *out++ = static_cast<char>(v);
  }
  *out++ = '|';
  if (pointBytes != 0) {
    std::memcpy(out, pts.data(), pointBytes);
  }
  return sig;
}

void RewritePathDataInElements(std::vector<Element*>& elements,
                               const std::unordered_map<PathData*, PathData*>& redirect);

void RewritePathDataInLayer(Layer* layer,
                            const std::unordered_map<PathData*, PathData*>& redirect) {
  if (layer == nullptr) return;
  RewritePathDataInElements(layer->contents, redirect);
  for (auto* child : layer->children) {
    RewritePathDataInLayer(child, redirect);
  }
  if (layer->mask != nullptr) {
    RewritePathDataInLayer(layer->mask, redirect);
  }
}

void RewritePathDataInElements(std::vector<Element*>& elements,
                               const std::unordered_map<PathData*, PathData*>& redirect) {
  for (auto* el : elements) {
    auto type = el->nodeType();
    if (type == NodeType::Path) {
      auto* path = static_cast<Path*>(el);
      auto it = redirect.find(path->data);
      if (it != redirect.end()) {
        path->data = it->second;
      }
    } else if (type == NodeType::Group || type == NodeType::TextBox) {
      auto* group = static_cast<Group*>(el);
      RewritePathDataInElements(group->elements, redirect);
    }
  }
}

// Returns true if any duplicate was collapsed. Duplicates have their `id` cleared so the
// subsequent unreferenced-resource pass removes them from the exported document.
bool DedupPathDataResources(PAGXDocument* doc) {
  std::unordered_map<std::string, PathData*> bySig;
  std::unordered_map<PathData*, PathData*> redirect;
  for (auto& nodePtr : doc->nodes) {
    if (nodePtr->nodeType() != NodeType::PathData) continue;
    auto* pd = static_cast<PathData*>(nodePtr.get());
    if (pd->id.empty()) continue;
    auto sig = PathDataSignature(pd);
    if (sig.empty()) continue;
    auto it = bySig.find(sig);
    if (it == bySig.end()) {
      bySig.emplace(std::move(sig), pd);
    } else {
      redirect[pd] = it->second;
    }
  }
  if (redirect.empty()) return false;

  for (auto* layer : doc->layers) {
    RewritePathDataInLayer(layer, redirect);
  }
  for (auto& nodePtr : doc->nodes) {
    if (nodePtr->nodeType() == NodeType::Composition) {
      auto* comp = static_cast<Composition*>(nodePtr.get());
      for (auto* layer : comp->layers) {
        RewritePathDataInLayer(layer, redirect);
      }
    }
  }
  // Drop the duplicates' ids so they will be pruned. Keep them owned in `doc->nodes`
  // (cheap; clearer than juggling unique_ptr lifetimes here).
  for (auto& kv : redirect) {
    kv.first->id.clear();
  }
  return true;
}

// Collect every resource id that is referenced from a Layer, Element, Composition or Font glyph.
void CollectReferencedIds(const PAGXDocument* doc, std::unordered_set<std::string>& refs);

void CollectRefsFromElement(const Element* element, std::unordered_set<std::string>& refs) {
  if (element == nullptr) return;
  if (!element->id.empty()) refs.insert(element->id);
  auto type = element->nodeType();
  if (type == NodeType::Group || type == NodeType::TextBox) {
    auto* group = static_cast<const Group*>(element);
    for (auto* child : group->elements) {
      CollectRefsFromElement(child, refs);
    }
  } else if (type == NodeType::Path) {
    auto* path = static_cast<const Path*>(element);
    if (path->data != nullptr && !path->data->id.empty()) {
      refs.insert(path->data->id);
    }
  } else if (type == NodeType::Fill) {
    auto* fill = static_cast<const Fill*>(element);
    if (fill->color != nullptr) {
      if (!fill->color->id.empty()) refs.insert(fill->color->id);
      if (fill->color->nodeType() == NodeType::ImagePattern) {
        auto* pattern = static_cast<const ImagePattern*>(fill->color);
        if (pattern->image != nullptr && !pattern->image->id.empty()) {
          refs.insert(pattern->image->id);
        }
      }
    }
  } else if (type == NodeType::Stroke) {
    auto* stroke = static_cast<const Stroke*>(element);
    if (stroke->color != nullptr) {
      if (!stroke->color->id.empty()) refs.insert(stroke->color->id);
      if (stroke->color->nodeType() == NodeType::ImagePattern) {
        auto* pattern = static_cast<const ImagePattern*>(stroke->color);
        if (pattern->image != nullptr && !pattern->image->id.empty()) {
          refs.insert(pattern->image->id);
        }
      }
    }
  } else if (type == NodeType::Text) {
    auto* text = static_cast<const Text*>(element);
    for (auto* run : text->glyphRuns) {
      if (run != nullptr && run->font != nullptr && !run->font->id.empty()) {
        refs.insert(run->font->id);
      }
    }
  }
}

void CollectRefsFromLayer(const Layer* layer, std::unordered_set<std::string>& refs) {
  if (layer == nullptr) return;
  if (!layer->id.empty()) refs.insert(layer->id);
  if (layer->mask != nullptr) {
    CollectRefsFromLayer(layer->mask, refs);
  }
  if (layer->composition != nullptr && !layer->composition->id.empty()) {
    refs.insert(layer->composition->id);
  }
  for (auto* el : layer->contents) {
    CollectRefsFromElement(el, refs);
  }
  for (auto* child : layer->children) {
    CollectRefsFromLayer(child, refs);
  }
}

void CollectReferencedIds(const PAGXDocument* doc, std::unordered_set<std::string>& refs) {
  for (auto* layer : doc->layers) {
    CollectRefsFromLayer(layer, refs);
  }
  for (auto& nodePtr : doc->nodes) {
    auto type = nodePtr->nodeType();
    if (type == NodeType::Composition) {
      auto* comp = static_cast<const Composition*>(nodePtr.get());
      for (auto* layer : comp->layers) {
        CollectRefsFromLayer(layer, refs);
      }
    } else if (type == NodeType::Font) {
      auto* font = static_cast<const Font*>(nodePtr.get());
      for (auto* glyph : font->glyphs) {
        if (glyph == nullptr) continue;
        if (glyph->path != nullptr && !glyph->path->id.empty()) {
          refs.insert(glyph->path->id);
        }
        if (glyph->image != nullptr && !glyph->image->id.empty()) {
          refs.insert(glyph->image->id);
        }
      }
    }
  }
}

bool IsResourceNode(NodeType type) {
  switch (type) {
    case NodeType::PathData:
    case NodeType::Image:
    case NodeType::Composition:
    case NodeType::SolidColor:
    case NodeType::LinearGradient:
    case NodeType::RadialGradient:
    case NodeType::ConicGradient:
    case NodeType::DiamondGradient:
    case NodeType::ImagePattern:
    case NodeType::Font:
      return true;
    default:
      return false;
  }
}

// Clear `id` on resources that are no longer referenced anywhere. The exporter writes
// a `<Resources>` entry only when `id` is non-empty, so this effectively prunes them.
// The owning unique_ptr stays in `doc->nodes` (negligible cost; safer than mutating that
// vector while other code may still hold raw pointers).
bool PruneUnreferencedResources(PAGXDocument* doc) {
  std::unordered_set<std::string> refs;
  CollectReferencedIds(doc, refs);
  bool changed = false;
  for (auto& nodePtr : doc->nodes) {
    auto* node = nodePtr.get();
    if (node->id.empty()) continue;
    if (!IsResourceNode(node->nodeType())) continue;
    if (refs.find(node->id) == refs.end()) {
      node->id.clear();
      changed = true;
    }
  }
  return changed;
}

void OptimizeLayerList(PAGXDocument* doc, std::vector<Layer*>& layers,
                       std::unordered_set<const Layer*>& maskRefs,
                       const PAGXOptimizer::Options& options) {
  for (int iter = 0; iter < options.maxIterations; iter++) {
    bool changed = false;

    if (options.canonicalizePaths) {
      for (auto* layer : layers) {
        changed |= CanonicalizePathsInLayer(doc, layer);
      }
    }
    if (options.rectMaskToScrollRect) {
      bool rmChanged = false;
      for (auto* layer : layers) {
        rmChanged |= RectMaskToScrollRectInLayer(layer);
      }
      if (rmChanged) {
        RecomputeMaskRefs(doc, maskRefs);
        changed = true;
      }
    }
    if (options.pruneEmpty) {
      changed |= PruneEmptyTopLevel(layers, /*parentHasLayout=*/false, maskRefs);
    }
    if (options.unwrapRedundantFirstGroup) {
      for (auto* layer : layers) {
        changed |= UnwrapRedundantFirstGroupInLayer(layer);
      }
    }
    if (options.mergeAdjacentGroups) {
      for (auto* layer : layers) {
        changed |= MergeAdjacentGroupsInLayer(layer);
      }
    }
    if (options.downgradeShellChildren) {
      changed |= DowngradeShellChildren(doc, layers, maskRefs);
    }
    if (options.mergeAdjacentShellLayers) {
      changed |= MergeAdjacentShellLayers(doc, layers, maskRefs);
    }
    if (!changed) break;
  }
}

}  // namespace

void PAGXOptimizer::Optimize(PAGXDocument* doc, const Options& options) {
  if (doc == nullptr) return;

  std::unordered_set<const Layer*> maskRefs;
  RecomputeMaskRefs(doc, maskRefs);

  OptimizeLayerList(doc, doc->layers, maskRefs, options);
  for (auto& node : doc->nodes) {
    if (node->nodeType() == NodeType::Composition) {
      auto* comp = static_cast<Composition*>(node.get());
      OptimizeLayerList(doc, comp->layers, maskRefs, options);
    }
  }

  // Resource-level cleanup: deduplicate identical PathData resources first (so the prune pass
  // sees the redirected references), then drop anything no longer referenced.
  if (options.dedupPathData) {
    DedupPathDataResources(doc);
  }
  if (options.pruneUnreferencedResources) {
    PruneUnreferencedResources(doc);
  }
}

}  // namespace pagx
