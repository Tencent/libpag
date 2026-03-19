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

#include "ConstraintLayout.h"
#include <cmath>
#include "pagx/layout/ElementConstraint.h"
#include "pagx/layout/ElementMeasure.h"
#include "pagx/layout/ElementTransform.h"
#include "pagx/nodes/Group.h"

namespace pagx {

static void ApplyConstraintToElement(Element* element, float containerWidth, float containerHeight,
                                     const TextMeasureFunc& measureText) {
  auto constraints = ElementConstraint::GetConstraints(element);
  if (!constraints.hasAny()) {
    return;
  }
  auto bounds = ElementMeasure::GetContentBounds(element, measureText);
  auto type = element->nodeType();
  bool stretchable = ElementConstraint::IsStretchable(type);

  // --- Proportional scaling for non-stretchable, non-Group elements ---
  if (!stretchable && type != NodeType::Group) {
    bool hasLeftRight = !std::isnan(constraints.left) && !std::isnan(constraints.right);
    bool hasTopBottom = !std::isnan(constraints.top) && !std::isnan(constraints.bottom);
    if (hasLeftRight || hasTopBottom) {
      float scale = 0.0f;
      if (hasLeftRight && bounds.width > 0) {
        float areaWidth = containerWidth - constraints.left - constraints.right;
        scale = areaWidth / bounds.width;
      }
      if (hasTopBottom && bounds.height > 0) {
        float scaleY = (containerHeight - constraints.top - constraints.bottom) / bounds.height;
        scale = (scale > 0) ? std::min(scale, scaleY) : scaleY;
      }
      if (scale > 0 && scale != 1.0f) {
        ElementTransform::ApplyScaling(element, scale);
        bounds = ElementMeasure::GetContentBounds(element, measureText);
      }
    }
  }

  // --- Horizontal axis ---
  bool hasLeft = !std::isnan(constraints.left);
  bool hasRight = !std::isnan(constraints.right);
  bool hasCenterX = !std::isnan(constraints.centerX);

  if (hasLeft && hasRight) {
    float left = constraints.left;
    float right = constraints.right;
    float areaWidth = containerWidth - left - right;
    if (type == NodeType::Group) {
      auto* group = static_cast<Group*>(element);
      group->position.x = left;
      ElementTransform::UpdateGroupLayoutSize(group, areaWidth, NAN);
    } else if (stretchable) {
      float newCenterX = left + areaWidth * 0.5f;
      ElementTransform::ApplyHorizontalStretch(element, areaWidth, newCenterX);
    } else {
      float tx = left + (areaWidth - bounds.width) * 0.5f - bounds.x;
      ElementTransform::TranslateX(element, tx);
    }
  } else if (hasLeft) {
    float tx = constraints.left - bounds.x;
    ElementTransform::TranslateX(element, tx);
  } else if (hasRight) {
    float tx = (containerWidth - constraints.right) - (bounds.x + bounds.width);
    ElementTransform::TranslateX(element, tx);
  } else if (hasCenterX) {
    float boundsCenter = bounds.x + bounds.width * 0.5f;
    float tx = (containerWidth * 0.5f + constraints.centerX) - boundsCenter;
    ElementTransform::TranslateX(element, tx);
  }

  // --- Vertical axis ---
  bool hasTop = !std::isnan(constraints.top);
  bool hasBottom = !std::isnan(constraints.bottom);
  bool hasCenterY = !std::isnan(constraints.centerY);

  if (hasTop && hasBottom) {
    float top = constraints.top;
    float bottom = constraints.bottom;
    float areaHeight = containerHeight - top - bottom;
    if (type == NodeType::Group) {
      auto* group = static_cast<Group*>(element);
      group->position.y = top;
      ElementTransform::UpdateGroupLayoutSize(group, NAN, areaHeight);
    } else if (stretchable) {
      float newCenterY = top + areaHeight * 0.5f;
      ElementTransform::ApplyVerticalStretch(element, areaHeight, newCenterY);
    } else {
      float ty = top + (areaHeight - bounds.height) * 0.5f - bounds.y;
      ElementTransform::TranslateY(element, ty);
    }
  } else if (hasTop) {
    float ty = constraints.top - bounds.y;
    ElementTransform::TranslateY(element, ty);
  } else if (hasBottom) {
    float ty = (containerHeight - constraints.bottom) - (bounds.y + bounds.height);
    ElementTransform::TranslateY(element, ty);
  } else if (hasCenterY) {
    float boundsCenter = bounds.y + bounds.height * 0.5f;
    float ty = (containerHeight * 0.5f + constraints.centerY) - boundsCenter;
    ElementTransform::TranslateY(element, ty);
  }
}

Rect ConstraintLayout::GetContentBounds(const Element* element,
                                        const TextMeasureFunc& measureText) {
  return ElementMeasure::GetContentBounds(element, measureText);
}

Rect ConstraintLayout::GetMeasuredBounds(const Element* element,
                                         const TextMeasureFunc& measureText) {
  return ElementMeasure::GetMeasuredBounds(element, measureText);
}

std::pair<float, float> ConstraintLayout::MeasureContents(const std::vector<Element*>& elements,
                                                          const TextMeasureFunc& measureText) {
  float measuredWidth = 0;
  float measuredHeight = 0;

  for (auto* element : elements) {
    auto bounds = GetMeasuredBounds(element, measureText);
    if (bounds.isEmpty()) {
      continue;
    }
    measuredWidth = std::max(measuredWidth, bounds.x + bounds.width);
    measuredHeight = std::max(measuredHeight, bounds.y + bounds.height);
  }

  return {measuredWidth > 0 ? measuredWidth : 0, measuredHeight > 0 ? measuredHeight : 0};
}

void ConstraintLayout::Apply(const std::vector<Element*>& elements, float containerWidth,
                             float containerHeight, const TextMeasureFunc& measureText) {
  for (auto* element : elements) {
    ApplyConstraintToElement(element, containerWidth, containerHeight, measureText);

    if (element->nodeType() == NodeType::Group) {
      auto* group = static_cast<Group*>(element);
      float groupW = group->width;
      float groupH = group->height;
      if (std::isnan(groupW) || std::isnan(groupH)) {
        auto bounds = GetContentBounds(group, measureText);
        if (std::isnan(groupW)) {
          groupW = bounds.width;
        }
        if (std::isnan(groupH)) {
          groupH = bounds.height;
        }
      }
      Apply(group->elements, groupW, groupH, measureText);
    }
  }
}

}  // namespace pagx
