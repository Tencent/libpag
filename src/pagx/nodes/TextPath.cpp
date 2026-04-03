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
#include "pagx/layout/LayoutNode.h"
#include "pagx/types/Matrix.h"

namespace pagx {

void TextPath::onMeasure(LayoutContext*) {
  if (path) {
    auto bounds = path->getBounds();
    preferredX = bounds.x;
    preferredY = bounds.y;
    // Clamp to 1px minimum so that line paths (e.g. horizontal lines with height=0) have a
    // meaningful size for content measurement and constraint positioning.
    preferredWidth = std::max(bounds.width, 1.0f);
    preferredHeight = std::max(bounds.height, 1.0f);
  }
}

void TextPath::setLayoutSize(LayoutContext*, float width, float height) {
  if (!path) {
    return;
  }
  float scale = LayoutNode::ComputeUniformScale(preferredWidth, preferredHeight, width, height);
  if (scale != 1.0f) {
    path->transform(Matrix::Scale(scale, scale));
    baselineOrigin.x *= scale;
    baselineOrigin.y *= scale;
  }
  auto bounds = path->getBounds();
  layoutWidth = std::max(bounds.width, 1.0f);
  layoutHeight = std::max(bounds.height, 1.0f);
}

void TextPath::setLayoutPosition(LayoutContext*, float x, float y) {
  if (!path) {
    return;
  }
  auto bounds = path->getBounds();
  float tx = std::isnan(x) ? 0 : x - bounds.x;
  float ty = std::isnan(y) ? 0 : y - bounds.y;
  if (tx != 0 || ty != 0) {
    path->transform(Matrix::Translate(tx, ty));
    baselineOrigin.x += tx;
    baselineOrigin.y += ty;
  }
  if (!std::isnan(x)) {
    layoutX = x;
  }
  if (!std::isnan(y)) {
    layoutY = y;
  }
}

}  // namespace pagx
