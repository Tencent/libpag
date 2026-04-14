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

#include "pagx/PAGXAnalyzer.h"
#include <algorithm>
#include <cmath>
#include "pagx/nodes/LayoutNode.h"
#include "pagx/nodes/PathData.h"

namespace pagx {

bool PAGXAnalyzer::IsEmptyLayer(const Layer* layer, bool parentHasLayout) {
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

bool PAGXAnalyzer::IsEmptyGroup(const Group* group) {
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

bool PAGXAnalyzer::CanDowngradeLayerToGroup(const Layer* layer) {
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

bool PAGXAnalyzer::HasDefaultGroupTransform(const Group* group) {
  return group->anchor.x == 0 && group->anchor.y == 0 && group->position.x == 0 &&
         group->position.y == 0 && group->rotation == 0 && group->scale.x == 1 &&
         group->scale.y == 1 && group->skew == 0 && group->alpha == 1 && std::isnan(group->width) &&
         std::isnan(group->height);
}

bool PAGXAnalyzer::CanUnwrapFirstChildGroup(const Group* group) {
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

PathPrimitive PAGXAnalyzer::DetectPathPrimitive(const Path* path, RectInfo* rectOut,
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

}  // namespace pagx
