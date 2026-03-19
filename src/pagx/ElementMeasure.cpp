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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. See the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/layout/ElementMeasure.h"
#include <cmath>
#include "pagx/layout/ElementConstraint.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/TextBox.h"

namespace pagx {

Rect ElementMeasure::GetContentBounds(const Element* element, const TextMeasureFunc& measureText) {
  switch (element->nodeType()) {
    case NodeType::Rectangle: {
      auto* rect = static_cast<const Rectangle*>(element);
      float halfW = rect->size.width * 0.5f;
      float halfH = rect->size.height * 0.5f;
      return Rect::MakeXYWH(rect->position.x - halfW, rect->position.y - halfH, rect->size.width,
                            rect->size.height);
    }
    case NodeType::Ellipse: {
      auto* ellipse = static_cast<const Ellipse*>(element);
      float halfW = ellipse->size.width * 0.5f;
      float halfH = ellipse->size.height * 0.5f;
      return Rect::MakeXYWH(ellipse->position.x - halfW, ellipse->position.y - halfH,
                            ellipse->size.width, ellipse->size.height);
    }
    case NodeType::Polystar: {
      auto* polystar = static_cast<const Polystar*>(element);
      return polystar->computeBounds();
    }
    case NodeType::Path: {
      auto* path = static_cast<const Path*>(element);
      if (path->data != nullptr) {
        auto bounds = path->data->getBounds();
        return Rect::MakeXYWH(path->position.x + bounds.x, path->position.y + bounds.y,
                              bounds.width, bounds.height);
      }
      return {};
    }
    case NodeType::Text: {
      return measureText(static_cast<const Text*>(element));
    }
    case NodeType::TextBox: {
      auto* textBox = static_cast<const TextBox*>(element);
      return Rect::MakeXYWH(textBox->position.x, textBox->position.y, textBox->size.width,
                            textBox->size.height);
    }
    case NodeType::Group: {
      auto* group = static_cast<const Group*>(element);
      if (!std::isnan(group->width) && !std::isnan(group->height)) {
        return Rect::MakeXYWH(group->position.x, group->position.y, group->width, group->height);
      }
      // Without explicit dimensions, measure from child elements. Uses the same convention as
      // Layer measurement: width/height are the bottom-right extent from the local origin (0,0),
      // ignoring any top-left offset of children. This ensures frame-based alignment for Groups
      // regardless of whether dimensions are explicit or measured.
      float maxX = 0;
      float maxY = 0;
      for (auto* child : group->elements) {
        auto childBounds = GetContentBounds(child, measureText);
        if (childBounds.isEmpty()) {
          continue;
        }
        maxX = std::max(maxX, childBounds.x + childBounds.width);
        maxY = std::max(maxY, childBounds.y + childBounds.height);
      }
      return Rect::MakeXYWH(group->position.x, group->position.y, maxX, maxY);
    }
    default:
      return {};
  }
}

Rect ElementMeasure::GetMeasuredBounds(const Element* element, const TextMeasureFunc& measureText) {
  auto bounds = GetContentBounds(element, measureText);
  if (bounds.isEmpty()) {
    return {};
  }
  auto constraints = ElementConstraint::GetConstraints(element);
  if (!constraints.hasAny()) {
    return bounds;
  }
  bool stretchable = ElementConstraint::IsStretchable(element->nodeType());
  bool hasLeft = !std::isnan(constraints.left);
  bool hasRight = !std::isnan(constraints.right);
  bool hasTop = !std::isnan(constraints.top);
  bool hasBottom = !std::isnan(constraints.bottom);

  // --- Horizontal axis ---
  if (hasLeft && hasRight) {
    float left = constraints.left;
    float right = constraints.right;
    if (stretchable) {
      // Stretchable elements follow the container: minimum container width is left + right.
      bounds.x = 0;
      bounds.width = left + right;
    } else {
      // Non-stretchable elements are centered: need left + right + original width.
      bounds.x = 0;
      bounds.width = left + right + bounds.width;
    }
  } else if (hasLeft) {
    // Left-anchored: element occupies left + width.
    bounds.x = 0;
    bounds.width = constraints.left + bounds.width;
  } else if (hasRight) {
    // Right-anchored: element occupies right + width.
    bounds.x = 0;
    bounds.width = constraints.right + bounds.width;
  }

  // --- Vertical axis ---
  if (hasTop && hasBottom) {
    float top = constraints.top;
    float bottom = constraints.bottom;
    if (stretchable) {
      // Stretchable elements follow the container: minimum container height is top + bottom.
      bounds.y = 0;
      bounds.height = top + bottom;
    } else {
      // Non-stretchable elements are centered: need top + bottom + original height.
      bounds.y = 0;
      bounds.height = top + bottom + bounds.height;
    }
  } else if (hasTop) {
    // Top-anchored: element occupies top + height.
    bounds.y = 0;
    bounds.height = constraints.top + bounds.height;
  } else if (hasBottom) {
    // Bottom-anchored: element occupies bottom + height.
    bounds.y = 0;
    bounds.height = constraints.bottom + bounds.height;
  }

  return bounds;
}

}  // namespace pagx
