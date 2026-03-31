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
#include <cmath>
#include "pagx/layout/LayoutNode.h"

namespace pagx {

void Polystar::onMeasure(const LayoutContext&) {
  auto bounds = computeBounds();
  preferredX = bounds.x;
  preferredY = bounds.y;
  preferredWidth = bounds.width;
  preferredHeight = bounds.height;
}

void Polystar::setLayoutSize(const LayoutContext&, float width, float height) {
  float scale = LayoutNode::ComputeUniformScale(preferredWidth, preferredHeight, width, height);
  if (scale != 1.0f) {
    outerRadius = outerRadius * scale;
    innerRadius = innerRadius * scale;
  }
  auto bounds = computeBounds();
  actualWidth = bounds.width;
  actualHeight = bounds.height;
}

void Polystar::setLayoutPosition(const LayoutContext&, float x, float y) {
  auto bounds = computeBounds();
  if (!std::isnan(x)) {
    position.x = x - bounds.x;
  }
  if (!std::isnan(y)) {
    position.y = y - bounds.y;
  }
}

}  // namespace pagx
