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
    preferredX = bounds.x;
    preferredY = bounds.y;
    preferredWidth = std::isnan(width) ? bounds.width : width;
    preferredHeight = std::isnan(height) ? bounds.height : height;
  }
}

void TextPath::setLayoutSize(LayoutContext*, float targetWidth, float targetHeight) {
  if (!path) {
    return;
  }
  auto bounds = path->getBounds();
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

Point TextPath::renderPosition() const {
  if (!path) {
    return computeRenderPosition({}, 0, 0);
  }
  auto bounds = path->getBounds();
  return computeRenderPosition(bounds, bounds.width, bounds.height);
}

float TextPath::renderScale() const {
  if (!path) {
    return 1.0f;
  }
  auto bounds = path->getBounds();
  return computeRenderScale(bounds.width, bounds.height);
}

Point TextPath::renderBaselineOrigin() const {
  auto pos = renderPosition();
  float scale = renderScale();
  return {pos.x + baselineOrigin.x * scale, pos.y + baselineOrigin.y * scale};
}

}  // namespace pagx
