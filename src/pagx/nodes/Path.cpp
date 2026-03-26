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
#include "pagx/layout/LayoutNode.h"
#include "pagx/types/Matrix.h"

namespace pagx {

void Path::onMeasure(const LayoutContext&) {
  if (data) {
    auto bounds = data->getBounds();
    preferredWidth = bounds.width;
    preferredHeight = bounds.height;
  }
}

void Path::setLayoutSize(const LayoutContext&, float width, float height) {
  if (!data) {
    return;
  }
  float scale = LayoutNode::ComputeUniformScale(preferredWidth, preferredHeight, width, height);
  if (scale != 1.0f) {
    data->transform(Matrix::Scale(scale, scale));
  }
  auto bounds = data->getBounds();
  actualWidth = bounds.width;
  actualHeight = bounds.height;
}

void Path::setLayoutPosition(const LayoutContext&, float x, float y) {
  if (!data) {
    return;
  }
  auto bounds = data->getBounds();
  if (!std::isnan(x)) {
    position.x = x - bounds.x;
  }
  if (!std::isnan(y)) {
    position.y = y - bounds.y;
  }
}

}  // namespace pagx
