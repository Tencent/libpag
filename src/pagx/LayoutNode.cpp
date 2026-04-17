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

#include "pagx/nodes/LayoutNode.h"
#include <algorithm>
#include <cmath>
#include "LayoutContext.h"
#include "pagx/nodes/Element.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/nodes/TextPath.h"

namespace pagx {

bool LayoutNode::hasConstraints() const {
  return !std::isnan(left) || !std::isnan(right) || !std::isnan(top) || !std::isnan(bottom) ||
         !std::isnan(centerX) || !std::isnan(centerY);
}

Rect LayoutNode::layoutBounds() const {
  float x = std::isnan(layoutX) ? measuredX : layoutX;
  float y = std::isnan(layoutY) ? measuredY : layoutY;
  float w = std::isnan(layoutWidth) ? (std::isnan(measuredWidth) ? 0 : measuredWidth) : layoutWidth;
  float h =
      std::isnan(layoutHeight) ? (std::isnan(measuredHeight) ? 0 : measuredHeight) : layoutHeight;
  return Rect::MakeXYWH(x, y, w, h);
}

float LayoutNode::constraintExtentX() const {
  if (!std::isnan(centerX)) {
    return std::abs(centerX) * 2;
  }
  float extent = 0;
  if (!std::isnan(left)) {
    extent += left;
  }
  if (!std::isnan(right)) {
    extent += right;
  }
  return extent;
}

float LayoutNode::constraintExtentY() const {
  if (!std::isnan(centerY)) {
    return std::abs(centerY) * 2;
  }
  float extent = 0;
  if (!std::isnan(top)) {
    extent += top;
  }
  if (!std::isnan(bottom)) {
    extent += bottom;
  }
  return extent;
}

void LayoutNode::updateSize(LayoutContext* context) {
  // If both measured dimensions are already set, skip onMeasure.
  if (!std::isnan(measuredWidth) && !std::isnan(measuredHeight)) {
    return;
  }
  onMeasure(context);
}

void LayoutNode::setLayoutSize(LayoutContext*, float, float) {
  // Default implementation: no rendering attributes to write.
  // layoutWidth/layoutHeight remain NAN (leaf nodes without specific behavior).
}

void LayoutNode::PerformConstraintLayout(const std::vector<LayoutNode*>& nodes, float containerW,
                                         float containerH, const Padding& padding,
                                         LayoutContext* context) {
  bool hasPadding = !padding.isZero();
  float cw = hasPadding ? std::max(0.0f, containerW - padding.left - padding.right) : containerW;
  float ch = hasPadding ? std::max(0.0f, containerH - padding.top - padding.bottom) : containerH;
  for (auto* child : nodes) {
    // Phase 1: compute target size from opposite-edge constraints.
    float targetW = NAN;
    float targetH = NAN;
    if (!std::isnan(child->left) && !std::isnan(child->right)) {
      targetW = std::max(0.0f, std::ceil(cw - child->left - child->right));
    }
    if (!std::isnan(child->top) && !std::isnan(child->bottom)) {
      targetH = std::max(0.0f, std::ceil(ch - child->top - child->bottom));
    }
    // Phase 2: write self rendering attributes and layoutWidth/layoutHeight.
    child->setLayoutSize(context, targetW, targetH);
    // Phase 3: compute position from layoutWidth/layoutHeight.
    if (child->hasConstraints()) {
      auto pos =
          CalculateConstrainedPosition(cw, ch, child->layoutWidth, child->layoutHeight, *child);
      float finalX = hasPadding ? std::round(pos.x + padding.left) : std::round(pos.x);
      float finalY = hasPadding ? std::round(pos.y + padding.top) : std::round(pos.y);
      // Phase 4: write self position and layoutX/layoutY.
      child->setLayoutPosition(context, finalX, finalY);
    } else if (hasPadding) {
      child->setLayoutPosition(context, std::round(padding.left), std::round(padding.top));
    }
  }
}

LayoutNode* LayoutNode::AsLayoutNode(Element* element) {
  if (element == nullptr) {
    return nullptr;
  }
  switch (element->nodeType()) {
    case NodeType::Rectangle:
      return static_cast<Rectangle*>(element);
    case NodeType::Ellipse:
      return static_cast<Ellipse*>(element);
    case NodeType::Path:
      return static_cast<Path*>(element);
    case NodeType::Polystar:
      return static_cast<Polystar*>(element);
    case NodeType::Text:
      return static_cast<Text*>(element);
    case NodeType::TextPath:
      return static_cast<TextPath*>(element);
    case NodeType::Group:
      return static_cast<Group*>(element);
    case NodeType::TextBox:
      return static_cast<TextBox*>(element);
    default:
      return nullptr;
  }
}

Rect LayoutNode::CalculateConstrainedPosition(float containerW, float containerH, float layoutW,
                                              float layoutH, const LayoutNode& node) {
  float x = NAN;
  float y = NAN;

  // Horizontal axis: centerX > (left+right) > left > right
  bool hasLeft = !std::isnan(node.left);
  bool hasRight = !std::isnan(node.right);
  bool hasCenterX = !std::isnan(node.centerX);

  if (hasCenterX) {
    x = (containerW - layoutW) * 0.5f + node.centerX;
  } else if (hasLeft && hasRight) {
    x = node.left + (containerW - node.left - node.right - layoutW) * 0.5f;
  } else if (hasLeft) {
    x = node.left;
  } else if (hasRight) {
    x = containerW - layoutW - node.right;
  }

  // Vertical axis: centerY > (top+bottom) > top > bottom
  bool hasTop = !std::isnan(node.top);
  bool hasBottom = !std::isnan(node.bottom);
  bool hasCenterY = !std::isnan(node.centerY);

  if (hasCenterY) {
    y = (containerH - layoutH) * 0.5f + node.centerY;
  } else if (hasTop && hasBottom) {
    y = node.top + (containerH - node.top - node.bottom - layoutH) * 0.5f;
  } else if (hasTop) {
    y = node.top;
  } else if (hasBottom) {
    y = containerH - layoutH - node.bottom;
  }

  return Rect::MakeXYWH(x, y, layoutW, layoutH);
}

float LayoutNode::ComputeUniformScale(float contentW, float contentH, float targetW,
                                      float targetH) {
  bool hasW = !std::isnan(targetW) && contentW > 0;
  bool hasH = !std::isnan(targetH) && contentH > 0;
  if (hasW && hasH) {
    return std::min(targetW / contentW, targetH / contentH);
  }
  if (hasW) {
    return targetW / contentW;
  }
  if (hasH) {
    return targetH / contentH;
  }
  return 1.0f;
}

std::vector<LayoutNode*> LayoutNode::CollectLayoutNodes(const std::vector<Element*>& elements,
                                                        bool skipTextLayout) {
  std::vector<LayoutNode*> result = {};
  for (auto* element : elements) {
    if (skipTextLayout &&
        (element->nodeType() == NodeType::Text || element->nodeType() == NodeType::TextBox)) {
      continue;
    }
    auto* node = AsLayoutNode(element);
    if (node) {
      result.push_back(node);
    }
  }
  return result;
}

void LayoutNode::MeasureChildNodes(const std::vector<Element*>& elements, float explicitWidth,
                                   float explicitHeight, float& outWidth, float& outHeight) {
  if (!std::isnan(explicitWidth) && !std::isnan(explicitHeight)) {
    outWidth = explicitWidth;
    outHeight = explicitHeight;
    return;
  }

  float maxX = 0;
  float maxY = 0;
  for (auto* element : elements) {
    auto* node = AsLayoutNode(element);
    if (node == nullptr || std::isnan(node->measuredWidth) || std::isnan(node->measuredHeight)) {
      continue;
    }
    float extX = node->hasConstraints() ? node->constraintExtentX() : node->measuredX;
    float extY = node->hasConstraints() ? node->constraintExtentY() : node->measuredY;
    extX += node->measuredWidth;
    extY += node->measuredHeight;
    maxX = std::max(maxX, extX);
    maxY = std::max(maxY, extY);
  }

  float prevW = std::isnan(outWidth) ? 0 : outWidth;
  float prevH = std::isnan(outHeight) ? 0 : outHeight;
  outWidth = !std::isnan(explicitWidth) ? explicitWidth : std::max(prevW, maxX);
  outHeight = !std::isnan(explicitHeight) ? explicitHeight : std::max(prevH, maxY);
}

}  // namespace pagx
