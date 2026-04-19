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
  preferredX = position.x;
  preferredY = position.y;
  // Preferred size: authored width/height overrides the content-measured size. percentWidth/
  // percentHeight are not consulted here; they are resolved by the parent via
  // PerformConstraintLayout.
  MeasureChildNodes(elements, width, height, preferredWidth, preferredHeight);
  if (!padding.isZero()) {
    if (std::isnan(width)) {
      preferredWidth += padding.left + padding.right;
    }
    if (std::isnan(height)) {
      preferredHeight += padding.top + padding.bottom;
    }
  }
}

void Group::setLayoutSize(LayoutContext* context, float targetWidth, float targetHeight) {
  // Mirror Layer::setLayoutSize: keep content-measured axes NaN during pass 1 so percent
  // descendants fall back to their preferred size; refine from children's actual bounds and run
  // updateLayout again so size-dependent descendants pick up the refined container size.
  bool widthFromContent = std::isnan(targetWidth) && std::isnan(this->width);
  bool heightFromContent = std::isnan(targetHeight) && std::isnan(this->height);
  layoutWidth = widthFromContent ? NAN : (!std::isnan(targetWidth) ? targetWidth : preferredWidth);
  layoutHeight =
      heightFromContent ? NAN : (!std::isnan(targetHeight) ? targetHeight : preferredHeight);
  updateLayout(context);
  if (!widthFromContent && !heightFromContent) {
    return;
  }
  float maxX = 0;
  float maxY = 0;
  for (auto* element : elements) {
    auto* node = AsLayoutNode(element);
    if (node == nullptr) {
      continue;
    }
    // Match MeasureChildNodes: unconstrained nodes use their preferredX/Y as extent origin.
    float extX = node->hasConstraints() ? node->constraintExtentX() : node->preferredX;
    float extY = node->hasConstraints() ? node->constraintExtentY() : node->preferredY;
    extX += node->layoutBounds().width;
    extY += node->layoutBounds().height;
    maxX = std::max(maxX, extX);
    maxY = std::max(maxY, extY);
  }
  if (!padding.isZero()) {
    maxX += padding.left + padding.right;
    maxY += padding.top + padding.bottom;
  }
  float prevW = layoutWidth;
  float prevH = layoutHeight;
  if (widthFromContent) {
    layoutWidth = maxX;
  }
  if (heightFromContent) {
    layoutHeight = maxY;
  }
  if ((widthFromContent && layoutWidth != prevW) || (heightFromContent && layoutHeight != prevH)) {
    updateLayout(context);
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
