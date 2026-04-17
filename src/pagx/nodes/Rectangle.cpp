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

#include "pagx/nodes/Rectangle.h"
#include <cmath>

namespace pagx {

void Rectangle::onMeasure(LayoutContext*) {
  measuredX = std::isnan(position.x) ? 0 : (position.x - size.width * 0.5f);
  measuredY = std::isnan(position.y) ? 0 : (position.y - size.height * 0.5f);
  measuredWidth = size.width;
  measuredHeight = size.height;
}

void Rectangle::setLayoutSize(LayoutContext*, float width, float height) {
  layoutWidth = !std::isnan(width) ? width : measuredWidth;
  layoutHeight = !std::isnan(height) ? height : measuredHeight;
}

void Rectangle::setLayoutPosition(LayoutContext*, float x, float y) {
  if (!std::isnan(x)) {
    layoutX = x;
  }
  if (!std::isnan(y)) {
    layoutY = y;
  }
}

Point Rectangle::renderPosition() const {
  auto bounds = layoutBounds();
  return {bounds.x + bounds.width * 0.5f, bounds.y + bounds.height * 0.5f};
}

Size Rectangle::renderSize() const {
  auto bounds = layoutBounds();
  return {bounds.width, bounds.height};
}

}  // namespace pagx
