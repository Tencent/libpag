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

#include "pagx/nodes/Polystar.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include "pagx/nodes/LayoutNode.h"

namespace pagx {

Rect Polystar::getContentBounds() const {
  auto numPoints = static_cast<int>(ceilf(pointCount));
  if (numPoints <= 0) {
    return Rect::MakeXYWH(0, 0, 0, 0);
  }
  float startAngle = (rotation - 90.0f) * static_cast<float>(M_PI) / 180.0f;
  float minX = std::numeric_limits<float>::max();
  float minY = std::numeric_limits<float>::max();
  float maxX = std::numeric_limits<float>::lowest();
  float maxY = std::numeric_limits<float>::lowest();
  if (type == PolystarType::Star) {
    float angleStep = static_cast<float>(M_PI) / pointCount;
    float angle = startAngle;
    for (int i = 0; i < numPoints * 2; i++) {
      float radius = (i % 2 == 0) ? outerRadius : innerRadius;
      float vx = radius * cosf(angle);
      float vy = radius * sinf(angle);
      minX = std::min(minX, vx);
      minY = std::min(minY, vy);
      maxX = std::max(maxX, vx);
      maxY = std::max(maxY, vy);
      angle += angleStep;
    }
  } else {
    float angleStep = 2.0f * static_cast<float>(M_PI) / static_cast<float>(numPoints);
    float angle = startAngle;
    for (int i = 0; i < numPoints; i++) {
      float vx = outerRadius * cosf(angle);
      float vy = outerRadius * sinf(angle);
      minX = std::min(minX, vx);
      minY = std::min(minY, vy);
      maxX = std::max(maxX, vx);
      maxY = std::max(maxY, vy);
      angle += angleStep;
    }
  }
  return Rect::MakeXYWH(minX, minY, maxX - minX, maxY - minY);
}

void Polystar::onMeasure(LayoutContext*) {
  auto bounds = getContentBounds();
  // position specifies the polystar center; when unset, top-left of the bounding box sits at (0,0).
  preferredX = std::isnan(position.x) ? 0 : (position.x + bounds.x);
  preferredY = std::isnan(position.y) ? 0 : (position.y + bounds.y);
  // Preferred size: authored width/height overrides the intrinsic content bounds when present.
  preferredWidth = std::isnan(width) ? bounds.width : width;
  preferredHeight = std::isnan(height) ? bounds.height : height;
}

void Polystar::setLayoutSize(LayoutContext*, float targetWidth, float targetHeight) {
  auto bounds = getContentBounds();
  float tW = targetWidth;
  float tH = targetHeight;
  if (std::isnan(tW) && std::isnan(tH)) {
    tW = preferredWidth;
    tH = preferredHeight;
  }
  float scale = LayoutNode::ComputeUniformScale(bounds.width, bounds.height, tW, tH);
  layoutWidth = bounds.width * scale;
  layoutHeight = bounds.height * scale;
}

Point Polystar::renderPosition() const {
  auto bounds = getContentBounds();
  return computeRenderPosition(bounds, bounds.width, bounds.height);
}

float Polystar::renderScale() const {
  auto bounds = getContentBounds();
  return computeRenderScale(bounds.width, bounds.height);
}

float Polystar::renderOuterRadius() const {
  return outerRadius * renderScale();
}

float Polystar::renderInnerRadius() const {
  return innerRadius * renderScale();
}

}  // namespace pagx
