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
         !std::isnan(centerX) || !std::isnan(centerY) || !std::isnan(percentWidth) ||
         !std::isnan(percentHeight);
}

Rect LayoutNode::layoutBounds() const {
  float x = std::isnan(layoutX) ? preferredX : layoutX;
  float y = std::isnan(layoutY) ? preferredY : layoutY;
  float w =
      std::isnan(layoutWidth) ? (std::isnan(preferredWidth) ? 0 : preferredWidth) : layoutWidth;
  float h =
      std::isnan(layoutHeight) ? (std::isnan(preferredHeight) ? 0 : preferredHeight) : layoutHeight;
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
  // If both preferred dimensions are already set, skip onMeasure.
  if (!std::isnan(preferredWidth) && !std::isnan(preferredHeight)) {
    return;
  }
  onMeasure(context);
}

void LayoutNode::setLayoutSize(LayoutContext*, float, float) {
  // Default implementation: no rendering attributes to write.
  // layoutWidth/layoutHeight remain NAN (leaf nodes without specific behavior).
}

void LayoutNode::setLayoutPosition(LayoutContext*, float x, float y) {
  if (!std::isnan(x)) {
    layoutX = x;
  }
  if (!std::isnan(y)) {
    layoutY = y;
  }
}

Point LayoutNode::computeRenderPosition(const Rect& contentBounds, float intrinsicWidth,
                                        float intrinsicHeight) const {
  auto bounds = layoutBounds();
  float scale = ComputeUniformScale(intrinsicWidth, intrinsicHeight, bounds.width, bounds.height);
  float offsetX = (bounds.width - contentBounds.width * scale) * 0.5f;
  float offsetY = (bounds.height - contentBounds.height * scale) * 0.5f;
  return {bounds.x + offsetX - contentBounds.x * scale,
          bounds.y + offsetY - contentBounds.y * scale};
}

float LayoutNode::computeRenderScale(float intrinsicWidth, float intrinsicHeight) const {
  auto bounds = layoutBounds();
  return ComputeUniformScale(intrinsicWidth, intrinsicHeight, bounds.width, bounds.height);
}

void LayoutNode::PerformConstraintLayout(const std::vector<LayoutNode*>& nodes, float containerW,
                                         float containerH, const Padding& padding,
                                         LayoutContext* context) {
  bool hasPadding = !padding.isZero();
  // A NaN container axis means the parent is still resolving that axis (pass 1 of a
  // content-measured parent). Propagate NaN downward so percent children fall back to their
  // preferred size instead of locking onto a provisional value.
  float cw =
      std::isnan(containerW)
          ? NAN
          : (hasPadding ? std::max(0.0f, containerW - padding.left - padding.right) : containerW);
  float ch =
      std::isnan(containerH)
          ? NAN
          : (hasPadding ? std::max(0.0f, containerH - padding.top - padding.bottom) : containerH);
  for (auto* child : nodes) {
    // Phase 1: compute target size from parent-side inputs only.
    //   opposite-edge constraints > percentWidth/Height > NAN (child uses its preferred size)
    // An authored width/height is NOT considered here: the child already folds it into its
    // preferred size during onMeasure(), so passing NAN lets setLayoutSize fall back to it.
    float targetW = NAN;
    if (!std::isnan(child->left) && !std::isnan(child->right)) {
      targetW = std::isnan(cw) ? NAN : std::max(0.0f, std::ceil(cw - child->left - child->right));
    } else if (!std::isnan(child->percentWidth)) {
      targetW = std::ceil(cw * child->percentWidth / 100.0f);
    }
    float targetH = NAN;
    if (!std::isnan(child->top) && !std::isnan(child->bottom)) {
      targetH = std::isnan(ch) ? NAN : std::max(0.0f, std::ceil(ch - child->top - child->bottom));
    } else if (!std::isnan(child->percentHeight)) {
      targetH = std::ceil(ch * child->percentHeight / 100.0f);
    }
    // Phase 2: write self rendering attributes and layoutWidth/layoutHeight.
    child->setLayoutSize(context, targetW, targetH);
    // Phase 3: compute position from layoutWidth/layoutHeight. Any axis without a positional
    // constraint defaults to 0 relative to the padding origin, so padding always applies.
    if (child->hasConstraints() || hasPadding) {
      auto pos =
          CalculateConstrainedPosition(cw, ch, child->layoutWidth, child->layoutHeight, *child);
      float relX = std::isnan(pos.x) ? 0.0f : pos.x;
      float relY = std::isnan(pos.y) ? 0.0f : pos.y;
      float finalX = hasPadding ? std::round(relX + padding.left) : std::round(relX);
      float finalY = hasPadding ? std::round(relY + padding.top) : std::round(relY);
      // Phase 4: write self position and layoutX/layoutY.
      child->setLayoutPosition(context, finalX, finalY);
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
