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

Point TextPath::renderPosition() const {
  return computeRenderPosition(path ? path->getBounds() : Rect{});
}

float TextPath::renderScale() const {
  return computeRenderScale();
}

Point TextPath::renderBaselineOrigin() const {
  auto pos = renderPosition();
  float scale = renderScale();
  return {pos.x + baselineOrigin.x * scale, pos.y + baselineOrigin.y * scale};
}

}  // namespace pagx
