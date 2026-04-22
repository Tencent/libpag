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

#include "pagx/nodes/Path.h"
#include <cmath>
#include "pagx/nodes/LayoutNode.h"

namespace pagx {

void Path::onMeasure(LayoutContext*) {
  if (data) {
    auto bounds = data->getBounds();
    // position is the offset of the path coordinate system origin relative to the node origin.
    preferredX = position.x + bounds.x;
    preferredY = position.y + bounds.y;
    // Preferred size: authored width/height overrides the intrinsic path bounds when present.
    // percentWidth/Height are not consulted here.
    preferredWidth = std::isnan(width) ? bounds.width : width;
    preferredHeight = std::isnan(height) ? bounds.height : height;
  }
}

void Path::setLayoutSize(LayoutContext*, float targetWidth, float targetHeight) {
  if (!data) {
    return;
  }
  // Scale intrinsic path bounds to fit the requested target. NaN on an axis means the parent did
  // not constrain that axis, so ComputeUniformScale skips it (single-axis proportional scaling).
  // When both axes are NaN, fall back to the preferred size on both axes so an authored
  // width/height still drives the scaling even without parent constraints.
  auto bounds = data->getBounds();
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

Point Path::renderPosition() const {
  if (!data) {
    return computeRenderPosition({}, 0, 0);
  }
  auto bounds = data->getBounds();
  return computeRenderPosition(bounds, bounds.width, bounds.height);
}

float Path::renderScale() const {
  if (!data) {
    return 1.0f;
  }
  auto bounds = data->getBounds();
  return computeRenderScale(bounds.width, bounds.height);
}

}  // namespace pagx
