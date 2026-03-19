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

#include "ContainerLayout.h"
#include <cmath>
#include <vector>
#include "pagx/nodes/Layer.h"
#include "pagx/types/Alignment.h"
#include "pagx/types/Arrangement.h"
#include "pagx/types/LayoutMode.h"

namespace pagx {

void ContainerLayout::Apply(Layer* parent, const MeasureFunc& measureLayer) {
  auto direction = parent->layout;
  bool horizontal = (direction == LayoutMode::Horizontal);

  float containerMainSize = horizontal ? parent->width : parent->height;
  float containerCrossSize = horizontal ? parent->height : parent->width;

  float paddingMainStart = horizontal ? parent->padding.left : parent->padding.top;
  float paddingMainEnd = horizontal ? parent->padding.right : parent->padding.bottom;
  float paddingCrossStart = horizontal ? parent->padding.top : parent->padding.left;
  float paddingCrossEnd = horizontal ? parent->padding.bottom : parent->padding.right;

  float availableMain = containerMainSize - paddingMainStart - paddingMainEnd;

  auto& children = parent->children;
  if (children.empty()) {
    return;
  }

  size_t childCount = children.size();

  // Measure all participating children once upfront to avoid repeated measurement.
  // A child participates if it is visible and included in layout.
  std::vector<std::pair<float, float>> childMeasuredSizes(childCount);
  std::vector<bool> isParticipating(childCount, false);
  for (size_t i = 0; i < childCount; i++) {
    isParticipating[i] = children[i]->visible && children[i]->includeInLayout;
    if (isParticipating[i]) {
      childMeasuredSizes[i] = measureLayer(children[i]);
    }
  }

  // Determine which children have fixed vs flexible main-axis sizes.
  // A child with explicit width/height on the main axis is "fixed"; otherwise "flexible".
  // Flexible children get equal shares of available space (no intrinsic basis).
  std::vector<bool> isFixed(childCount, false);
  std::vector<float> childMainSizes(childCount, 0);
  std::vector<float> childCrossSizes(childCount, 0);

  for (size_t i = 0; i < childCount; i++) {
    if (!isParticipating[i]) {
      continue;
    }
    auto* child = children[i];
    float mainSize = horizontal ? child->width : child->height;
    if (!std::isnan(mainSize)) {
      isFixed[i] = true;
      childMainSizes[i] = mainSize;
    }

    float crossSize = horizontal ? child->height : child->width;
    if (!std::isnan(crossSize)) {
      childCrossSizes[i] = crossSize;
    } else {
      childCrossSizes[i] = horizontal ? childMeasuredSizes[i].second : childMeasuredSizes[i].first;
    }
  }

  // Collect all participating child indices.
  std::vector<size_t> indices = {};
  for (size_t i = 0; i < childCount; i++) {
    if (!isParticipating[i]) {
      continue;
    }
    indices.push_back(i);
  }

  size_t visibleCount = indices.size();
  float totalGap = parent->gap * static_cast<float>(visibleCount > 1 ? visibleCount - 1 : 0);

  // Calculate fixed total. Flexible items contribute 0 to fixed total.
  float fixedTotal = 0;
  std::vector<size_t> flexIndices = {};
  for (auto idx : indices) {
    if (isFixed[idx]) {
      fixedTotal += childMainSizes[idx];
    } else {
      flexIndices.push_back(idx);
    }
  }

  // Distribute available space equally among flexible children.
  float totalFlexSpace = availableMain - fixedTotal - totalGap;
  if (!flexIndices.empty() && totalFlexSpace > 0) {
    float share = totalFlexSpace / static_cast<float>(flexIndices.size());
    for (auto idx : flexIndices) {
      childMainSizes[idx] = share;
    }
  }

  // Assign computed sizes back to children (set width/height if they were flexible).
  // Use rounding error propagation to ensure sizes sum to the exact container width.
  float roundOff = 0;
  for (auto idx : indices) {
    auto* child = children[idx];
    if (!isFixed[idx]) {
      float rawSize = childMainSizes[idx];
      float roundedSize = std::round(rawSize + roundOff);
      roundOff += rawSize - roundedSize;
      childMainSizes[idx] = roundedSize;
      if (horizontal) {
        child->width = roundedSize;
      } else {
        child->height = roundedSize;
      }
    }
  }

  // Calculate the actual total main size used.
  float totalMain = 0;
  for (auto idx : indices) {
    totalMain += childMainSizes[idx];
  }
  totalMain += totalGap;

  // Calculate main-axis starting position based on arrangement.
  float mainOffset = paddingMainStart;
  float extraGap = 0;

  switch (parent->arrangement) {
    case Arrangement::Start:
      break;
    case Arrangement::Center:
      mainOffset += (availableMain - totalMain) * 0.5f;
      break;
    case Arrangement::End:
      mainOffset += (availableMain - totalMain);
      break;
    case Arrangement::SpaceBetween:
      if (visibleCount > 1) {
        float totalChildMain = totalMain - totalGap;
        float spaceToDistribute = availableMain - totalChildMain;
        extraGap = spaceToDistribute / static_cast<float>(visibleCount - 1);
      }
      break;
  }

  // Compute the cross-axis reference size for alignment.
  float alignCrossSize = (containerCrossSize - paddingCrossStart - paddingCrossEnd);

  // Position each child along the main axis and cross axis.
  for (auto idx : indices) {
    auto* child = children[idx];

    // Main axis position.
    float mainPos = mainOffset;

    // Cross axis position with alignment.
    float childCross = childCrossSizes[idx];
    float crossPos = paddingCrossStart;
    switch (parent->alignment) {
      case Alignment::Start:
        break;
      case Alignment::Center:
        crossPos += (alignCrossSize - childCross) * 0.5f;
        break;
      case Alignment::End:
        crossPos += (alignCrossSize - childCross);
        break;
      case Alignment::Stretch: {
        float explicitCross = horizontal ? child->height : child->width;
        if (std::isnan(explicitCross)) {
          childCross = alignCrossSize;
          childCrossSizes[idx] = childCross;
          if (horizontal) {
            child->height = childCross;
          } else {
            child->width = childCross;
          }
        }
        break;
      }
    }

    if (horizontal) {
      child->x = mainPos;
      child->y = crossPos;
    } else {
      child->x = crossPos;
      child->y = mainPos;
    }

    mainOffset += childMainSizes[idx];
    if (parent->arrangement == Arrangement::SpaceBetween) {
      mainOffset += extraGap;
    } else {
      mainOffset += parent->gap;
    }
  }
}

}  // namespace pagx
