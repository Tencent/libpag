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

void Rectangle::onMeasure(const LayoutContext&) {
  preferredWidth = size.width;
  preferredHeight = size.height;
}

void Rectangle::setLayoutSize(const LayoutContext&, float width, float height) {
  if (!std::isnan(width)) {
    size.width = width;
  }
  if (!std::isnan(height)) {
    size.height = height;
  }
  actualWidth = size.width;
  actualHeight = size.height;
}

void Rectangle::setLayoutPosition(const LayoutContext&, float x, float y) {
  if (!std::isnan(x)) {
    position.x = x + size.width * 0.5f;
  }
  if (!std::isnan(y)) {
    position.y = y + size.height * 0.5f;
  }
}

}  // namespace pagx
