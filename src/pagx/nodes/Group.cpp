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

#include "pagx/nodes/Group.h"
#include <cmath>
#include "pagx/layout/LayoutNode.h"

namespace pagx {

void Group::updateSize(const LayoutContext& context) {
  for (auto* child : elements) {
    auto* node = LayoutNode::AsLayoutNode(child);
    if (node) {
      node->updateSize(context);
    }
  }
  LayoutNode::updateSize(context);
}

void Group::onMeasure(const LayoutContext&) {
  MeasureChildNodes(elements, width, height, preferredWidth, preferredHeight);
}

void Group::setLayoutSize(const LayoutContext&, float width, float height) {
  actualWidth = !std::isnan(width) ? width : preferredWidth;
  actualHeight = !std::isnan(height) ? height : preferredHeight;
}

void Group::setLayoutPosition(const LayoutContext&, float x, float y) {
  if (!std::isnan(x)) {
    position.x = x;
  }
  if (!std::isnan(y)) {
    position.y = y;
  }
}

void Group::updateLayout(const LayoutContext& context) {
  auto nodes = CollectLayoutNodes(elements, false);
  PerformConstraintLayout(nodes, actualWidth, actualHeight, context);
}

}  // namespace pagx
