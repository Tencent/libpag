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

#include "pagx/nodes/TextBox.h"
#include <cmath>
#include "pagx/TextLayout.h"
#include "pagx/layout/LayoutNode.h"

namespace pagx {

void TextBox::updateSize(const LayoutContext& context) {
  for (auto* child : elements) {
    auto* node = LayoutNode::AsLayoutNode(child);
    if (node) {
      node->updateSize(context);
    }
  }
  LayoutNode::updateSize(context);
}

void TextBox::onMeasure(const LayoutContext& context) {
  if (!std::isnan(width) && !std::isnan(height)) {
    preferredWidth = width;
    preferredHeight = height;
    return;
  }
  float w = std::isnan(width) ? 0 : width;
  float h = std::isnan(height) ? 0 : height;
  auto measured = TextLayout::MeasureTextBox(this, width, height, context);
  if (std::isnan(width)) {
    w = measured.width;
  }
  if (std::isnan(height)) {
    h = measured.height;
  }
  preferredWidth = w;
  preferredHeight = h;
}

void TextBox::setLayoutSize(const LayoutContext&, float width, float height) {
  actualWidth = !std::isnan(width) ? width : preferredWidth;
  actualHeight = !std::isnan(height) ? height : preferredHeight;
}

void TextBox::updateLayout(const LayoutContext& context) {
  // Skip Text elements: they are positioned by TextLayout, not constraint positioning.
  auto nodes = CollectLayoutNodes(elements, true);
  PerformConstraintLayout(nodes, actualWidth, actualHeight, context);
  // Text elements are typeset by TextLayout after constraint positioning.
  TextLayout::LayoutTextBox(this, actualWidth, actualHeight, context);
}

}  // namespace pagx
