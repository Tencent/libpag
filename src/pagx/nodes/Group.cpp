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
#include "pagx/nodes/LayoutNode.h"

namespace pagx {

void Group::updateSize(LayoutContext* context) {
  for (auto* child : elements) {
    auto* node = LayoutNode::AsLayoutNode(child);
    if (node) {
      node->updateSize(context);
    }
  }
  LayoutNode::updateSize(context);
}

void Group::onMeasure(LayoutContext*) {
  measuredX = position.x;
  measuredY = position.y;
  MeasureChildNodes(elements, width, height, measuredWidth, measuredHeight);
  if (!padding.isZero()) {
    if (std::isnan(width)) {
      measuredWidth += padding.left + padding.right;
    }
    if (std::isnan(height)) {
      measuredHeight += padding.top + padding.bottom;
    }
  }
}

void Group::setLayoutSize(LayoutContext* context, float width, float height) {
  layoutWidth = !std::isnan(width) ? width : measuredWidth;
  layoutHeight = !std::isnan(height) ? height : measuredHeight;
  updateLayout(context);
  // An axis is content-measured when neither the parent nor the element itself specifies its size.
  // When a content-measured axis exists and another axis changed from its measured value,
  // re-measure the content-measured axis from children's actual layout sizes.
  bool widthFromContent = std::isnan(width) && std::isnan(this->width);
  bool heightFromContent = std::isnan(height) && std::isnan(this->height);
  bool sizeChanged = (!std::isnan(width) && width != measuredWidth) ||
                     (!std::isnan(height) && height != measuredHeight);
  if ((widthFromContent || heightFromContent) && sizeChanged) {
    float maxX = 0;
    float maxY = 0;
    for (auto* element : elements) {
      auto* node = AsLayoutNode(element);
      if (node == nullptr) {
        continue;
      }
      float extX = node->hasConstraints() ? node->constraintExtentX() : 0;
      float extY = node->hasConstraints() ? node->constraintExtentY() : 0;
      extX += node->layoutBounds().width;
      extY += node->layoutBounds().height;
      maxX = std::max(maxX, extX);
      maxY = std::max(maxY, extY);
    }
    if (!padding.isZero()) {
      maxX += padding.left + padding.right;
      maxY += padding.top + padding.bottom;
    }
    if (widthFromContent) {
      layoutWidth = std::ceil(maxX);
    }
    if (heightFromContent) {
      layoutHeight = std::ceil(maxY);
    }
  }
}

Point Group::renderPosition() const {
  auto bounds = layoutBounds();
  return {bounds.x, bounds.y};
}

void Group::updateLayout(LayoutContext* context) {
  auto nodes = CollectLayoutNodes(elements, false);
  PerformConstraintLayout(nodes, layoutWidth, layoutHeight, padding, context);
}

}  // namespace pagx
