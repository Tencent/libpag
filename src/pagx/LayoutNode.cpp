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

#include "pagx/layout/LayoutNode.h"
#include <algorithm>
#include <cmath>
#include "LayoutContext.h"
#include "pagx/nodes/Element.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/nodes/TextPath.h"

namespace pagx {

bool LayoutNode::hasConstraints() const {
  return !std::isnan(left) || !std::isnan(right) || !std::isnan(top) || !std::isnan(bottom) ||
         !std::isnan(centerX) || !std::isnan(centerY);
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

void LayoutNode::updateSize(const LayoutContext& context) {
  // If both preferred dimensions are already measured, skip onMeasure.
  if (!std::isnan(preferredWidth) && !std::isnan(preferredHeight)) {
    return;
  }
  onMeasure(context);
}

void LayoutNode::setLayoutSize(const LayoutContext&, float, float) {
  // Default implementation: no rendering attributes to write.
  // actualWidth/actualHeight remain NAN (leaf nodes without specific behavior).
}

void LayoutNode::PerformConstraintLayout(const std::vector<LayoutNode*>& nodes, float containerW,
                                         float containerH, const LayoutContext& context) {
  for (auto* child : nodes) {
    // Phase 1: compute target size from opposite-edge constraints.
    float targetW = NAN;
    float targetH = NAN;
    if (!std::isnan(child->left) && !std::isnan(child->right)) {
      targetW = std::round(containerW - child->left - child->right);
    }
    if (!std::isnan(child->top) && !std::isnan(child->bottom)) {
      targetH = std::round(containerH - child->top - child->bottom);
    }
    // Phase 2: write self rendering attributes and actualWidth/actualHeight.
    child->setLayoutSize(context, targetW, targetH);
    // Phase 3: compute position from actualWidth/actualHeight.
    if (child->hasConstraints()) {
      auto pos = CalculateConstrainedPosition(containerW, containerH, child->actualWidth,
                                              child->actualHeight, *child);
      // Phase 4: write self position.
      child->setLayoutPosition(context, std::round(pos.x), std::round(pos.y));
    }
    // Phase 5: recursively lay out children (containers only, leaf nodes no-op).
    child->updateLayout(context);
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

Rect LayoutNode::CalculateConstrainedPosition(float containerW, float containerH, float actualW,
                                              float actualH, const LayoutNode& node) {
  float x = NAN;
  float y = NAN;

  // Horizontal axis: centerX > (left+right) > left > right
  bool hasLeft = !std::isnan(node.left);
  bool hasRight = !std::isnan(node.right);
  bool hasCenterX = !std::isnan(node.centerX);

  if (hasCenterX) {
    x = (containerW - actualW) * 0.5f + node.centerX;
  } else if (hasLeft && hasRight) {
    x = node.left + (containerW - node.left - node.right - actualW) * 0.5f;
  } else if (hasLeft) {
    x = node.left;
  } else if (hasRight) {
    x = containerW - actualW - node.right;
  }

  // Vertical axis: centerY > (top+bottom) > top > bottom
  bool hasTop = !std::isnan(node.top);
  bool hasBottom = !std::isnan(node.bottom);
  bool hasCenterY = !std::isnan(node.centerY);

  if (hasCenterY) {
    y = (containerH - actualH) * 0.5f + node.centerY;
  } else if (hasTop && hasBottom) {
    y = node.top + (containerH - node.top - node.bottom - actualH) * 0.5f;
  } else if (hasTop) {
    y = node.top;
  } else if (hasBottom) {
    y = containerH - actualH - node.bottom;
  }

  return Rect::MakeXYWH(x, y, actualW, actualH);
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
                                                        bool skipText) {
  std::vector<LayoutNode*> result = {};
  for (auto* element : elements) {
    if (skipText && element->nodeType() == NodeType::Text) {
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
    if (element->nodeType() == NodeType::Stroke) {
      auto* stroke = static_cast<Stroke*>(element);
      float expansion = 0.0f;
      switch (stroke->align) {
        case StrokeAlign::Center:
          expansion = stroke->width * 0.5f;
          break;
        case StrokeAlign::Outside:
          expansion = stroke->width;
          break;
        case StrokeAlign::Inside:
          break;
      }
      maxX += expansion;
      maxY += expansion;
      continue;
    }
    auto* node = AsLayoutNode(element);
    if (node == nullptr || std::isnan(node->preferredWidth) || std::isnan(node->preferredHeight)) {
      continue;
    }
    float extX = node->hasConstraints() ? node->constraintExtentX() : node->preferredX;
    float extY = node->hasConstraints() ? node->constraintExtentY() : node->preferredY;
    extX += node->preferredWidth;
    extY += node->preferredHeight;
    maxX = std::max(maxX, extX);
    maxY = std::max(maxY, extY);
  }

  float prevW = std::isnan(outWidth) ? 0 : outWidth;
  float prevH = std::isnan(outHeight) ? 0 : outHeight;
  outWidth = !std::isnan(explicitWidth) ? explicitWidth : std::max(prevW, maxX);
  outHeight = !std::isnan(explicitHeight) ? explicitHeight : std::max(prevH, maxY);
}

}  // namespace pagx
