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
  // A child participates if it is included in layout. Visibility does not affect layout —
  // invisible Layers still need correct sizes and positions.
  std::vector<std::pair<float, float>> childMeasuredSizes(childCount);
  std::vector<bool> isParticipating(childCount, false);
  for (size_t i = 0; i < childCount; i++) {
    isParticipating[i] = children[i]->includeInLayout;
    if (isParticipating[i]) {
      childMeasuredSizes[i] = measureLayer(children[i]);
    }
  }

  // Determine child main-axis sizing with three-state logic:
  // 1. Explicit width/height → fixed size (flex ignored)
  // 2. No explicit size, flex=0 (default) → content-measured size
  // 3. No explicit size, flex>0 → proportional share of remaining space
  enum class SizeMode { Fixed, ContentMeasured, Flex };
  std::vector<SizeMode> sizeMode(childCount, SizeMode::Fixed);
  std::vector<float> childMainSizes(childCount, 0);
  std::vector<float> childCrossSizes(childCount, 0);

  for (size_t i = 0; i < childCount; i++) {
    if (!isParticipating[i]) {
      continue;
    }
    auto* child = children[i];
    float mainSize = horizontal ? child->width : child->height;
    if (!std::isnan(mainSize)) {
      sizeMode[i] = SizeMode::Fixed;
      childMainSizes[i] = mainSize;
    } else if (child->flex > 0) {
      sizeMode[i] = SizeMode::Flex;
      childMainSizes[i] = 0;
    } else {
      sizeMode[i] = SizeMode::ContentMeasured;
      childMainSizes[i] = horizontal ? childMeasuredSizes[i].first : childMeasuredSizes[i].second;
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

  // Calculate non-flex total and collect flex items with their weights.
  float nonFlexTotal = 0;
  float totalFlex = 0;
  std::vector<size_t> flexIndices = {};
  for (auto idx : indices) {
    if (sizeMode[idx] == SizeMode::Flex) {
      totalFlex += children[idx]->flex;
      flexIndices.push_back(idx);
    } else {
      nonFlexTotal += childMainSizes[idx];
    }
  }

  // Distribute remaining space proportionally among flex children by weight.
  float totalFlexSpace = availableMain - nonFlexTotal - totalGap;
  if (!flexIndices.empty() && totalFlexSpace > 0 && totalFlex > 0) {
    for (auto idx : flexIndices) {
      childMainSizes[idx] = totalFlexSpace * children[idx]->flex / totalFlex;
    }
  }

  // Assign computed sizes back to flex children (set width/height).
  // Use rounding error propagation to ensure sizes sum to the exact container width.
  float roundOff = 0;
  for (auto idx : flexIndices) {
    auto* child = children[idx];
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
    case Arrangement::SpaceEvenly: {
      float totalChildMain = totalMain - totalGap;
      float spaceToDistribute = availableMain - totalChildMain;
      extraGap = spaceToDistribute / static_cast<float>(visibleCount + 1);
      mainOffset += extraGap;
      break;
    }
    case Arrangement::SpaceAround: {
      float totalChildMain = totalMain - totalGap;
      float spaceToDistribute = availableMain - totalChildMain;
      extraGap = spaceToDistribute / static_cast<float>(visibleCount);
      mainOffset += extraGap * 0.5f;
      break;
    }
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
    if (parent->arrangement == Arrangement::SpaceBetween ||
        parent->arrangement == Arrangement::SpaceEvenly ||
        parent->arrangement == Arrangement::SpaceAround) {
      mainOffset += extraGap;
    } else {
      mainOffset += parent->gap;
    }
  }
}

}  // namespace pagx
