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

#include "pagx/nodes/TextPath.h"
#include <cmath>
#include "pagx/nodes/LayoutNode.h"

namespace pagx {

void TextPath::onMeasure(LayoutContext*) {
  if (path) {
    auto bounds = path->getBounds();
    measuredX = bounds.x;
    measuredY = bounds.y;
    measuredWidth = bounds.width;
    measuredHeight = bounds.height;
  }
}

void TextPath::setLayoutSize(LayoutContext*, float width, float height) {
  if (!path) {
    return;
  }
  float scale = LayoutNode::ComputeUniformScale(measuredWidth, measuredHeight, width, height);
  layoutWidth = measuredWidth * scale;
  layoutHeight = measuredHeight * scale;
}

void TextPath::setLayoutPosition(LayoutContext*, float x, float y) {
  if (!path) {
    return;
  }
  if (!std::isnan(x)) {
    layoutX = x;
  }
  if (!std::isnan(y)) {
    layoutY = y;
  }
}

Point TextPath::renderPosition() const {
  auto bounds = layoutBounds();
  auto pathBounds = path ? path->getBounds() : Rect{};
  float scale =
      LayoutNode::ComputeUniformScale(measuredWidth, measuredHeight, bounds.width, bounds.height);
  float offsetX = (bounds.width - pathBounds.width * scale) * 0.5f;
  float offsetY = (bounds.height - pathBounds.height * scale) * 0.5f;
  return {bounds.x + offsetX - pathBounds.x * scale, bounds.y + offsetY - pathBounds.y * scale};
}

float TextPath::renderScale() const {
  auto bounds = layoutBounds();
  return LayoutNode::ComputeUniformScale(measuredWidth, measuredHeight, bounds.width,
                                         bounds.height);
}

Point TextPath::renderBaselineOrigin() const {
  auto pos = renderPosition();
  float scale = renderScale();
  return {pos.x + baselineOrigin.x * scale, pos.y + baselineOrigin.y * scale};
}

}  // namespace pagx
