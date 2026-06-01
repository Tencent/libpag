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
  // Preferred size: authored width/height overrides the intrinsic 'size'. percentWidth/Height are
  // not consulted here; they are resolved by the parent via PerformConstraintLayout.
  float w = std::isnan(width) ? size.width : width;
  float h = std::isnan(height) ? size.height : height;
  // position specifies the rectangle center. When position.x/y is NaN, the rectangle's top-left is
  // placed at the origin (0, 0).
  preferredX = std::isnan(position.x) ? 0 : (position.x - w * 0.5f);
  preferredY = std::isnan(position.y) ? 0 : (position.y - h * 0.5f);
  preferredWidth = w;
  preferredHeight = h;
}

void Rectangle::setLayoutSize(LayoutContext*, float targetWidth, float targetHeight) {
  layoutWidth = !std::isnan(targetWidth) ? targetWidth : preferredWidth;
  layoutHeight = !std::isnan(targetHeight) ? targetHeight : preferredHeight;
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
