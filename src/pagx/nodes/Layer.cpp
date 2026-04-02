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

#include "pagx/nodes/Layer.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>
#include "pagx/layout/LayoutNode.h"
#include "pagx/nodes/Composition.h"
#include "pagx/types/Alignment.h"
#include "pagx/types/Arrangement.h"
#include "pagx/types/LayoutMode.h"
#include "pagx/types/Rect.h"

namespace pagx {

void Layer::updateSize(LayoutContext* context) {
  for (auto* element : contents) {
    auto* node = LayoutNode::AsLayoutNode(element);
    if (node) {
      node->updateSize(context);
    }
  }
  for (auto* child : children) {
    child->updateSize(context);
  }
  LayoutNode::updateSize(context);
}

void Layer::onMeasure(LayoutContext*) {
  preferredX = x;
  preferredY = y;
  // If both dimensions are explicit, use them directly.
  if (!std::isnan(width) && !std::isnan(height)) {
    preferredWidth = width;
    preferredHeight = height;
    return;
  }

  float measuredW = 0;
  float measuredH = 0;

  // If this Layer references a Composition, use its dimensions as intrinsic size.
  if (composition != nullptr) {
    measuredW = std::max(measuredW, composition->width);
    measuredH = std::max(measuredH, composition->height);
  }

  // Measure from flex container children along the layout axis.
  if (layout != LayoutMode::None && !children.empty()) {
    bool horizontal = (layout == LayoutMode::Horizontal);
    float paddingMainStart = horizontal ? padding.left : padding.top;
    float paddingMainEnd = horizontal ? padding.right : padding.bottom;
    float paddingCrossStart = horizontal ? padding.top : padding.left;
    float paddingCrossEnd = horizontal ? padding.bottom : padding.right;

    float totalMain = 0;
    float maxCross = 0;
    size_t visibleChildCount = 0;
    for (auto* child : children) {
      if (!child->includeInLayout) {
        continue;
      }
      visibleChildCount++;
      float childMain = horizontal ? child->preferredWidth : child->preferredHeight;
      float childCross = horizontal ? child->preferredHeight : child->preferredWidth;
      totalMain += childMain;
      maxCross = std::max(maxCross, childCross);
    }

    float totalGap = gap * static_cast<float>(visibleChildCount > 1 ? visibleChildCount - 1 : 0);
    float mainSize = totalMain + totalGap + paddingMainStart + paddingMainEnd;
    float crossSize = maxCross + paddingCrossStart + paddingCrossEnd;
    if (horizontal) {
      measuredW = std::max(measuredW, mainSize);
      measuredH = std::max(measuredH, crossSize);
    } else {
      measuredH = std::max(measuredH, mainSize);
      measuredW = std::max(measuredW, crossSize);
    }
  }

  // Measure from contents elements (merge via max to preserve flex-measured values above).
  float contentsW = 0;
  float contentsH = 0;
  MeasureChildNodes(contents, NAN, NAN, contentsW, contentsH);
  measuredW = std::max(measuredW, contentsW);
  measuredH = std::max(measuredH, contentsH);

  // Measure from non-flex child Layers (skip children excluded from layout).
  if (layout == LayoutMode::None) {
    for (auto* child : children) {
      if (!child->includeInLayout) {
        continue;
      }
      float cx = child->preferredWidth + child->constraintExtentX();
      float cy = child->preferredHeight + child->constraintExtentY();
      measuredW = std::max(measuredW, cx);
      measuredH = std::max(measuredH, cy);
    }
  }

  // Use explicit value if set, otherwise use measured value.
  preferredWidth = !std::isnan(width) ? width : measuredW;
  preferredHeight = !std::isnan(height) ? height : measuredH;
}

void Layer::setLayoutSize(LayoutContext*, float width, float height) {
  layoutWidth = !std::isnan(width) ? width : preferredWidth;
  layoutHeight = !std::isnan(height) ? height : preferredHeight;
  if (clipToBounds && !hasScrollRect && !std::isnan(layoutWidth) && !std::isnan(layoutHeight)) {
    scrollRect = Rect::MakeXYWH(0, 0, layoutWidth, layoutHeight);
    hasScrollRect = true;
  }
}

void Layer::setLayoutPosition(LayoutContext*, float x, float y) {
  if (!std::isnan(x)) {
    this->x = x;
    layoutX = x;
  }
  if (!std::isnan(y)) {
    this->y = y;
    layoutY = y;
  }
}

void Layer::updateLayout(LayoutContext* context) {
  // Flex container layout: arrange flex children.
  if (layout != LayoutMode::None && !children.empty()) {
    performContainerLayout(context);
  }

  // Collect contents + non-flex child Layers for unified constraint positioning.
  auto nodes = LayoutNode::CollectLayoutNodes(contents, false);
  for (auto* child : children) {
    bool inFlexFlow = (layout != LayoutMode::None && child->includeInLayout);
    if (!inFlexFlow) {
      nodes.push_back(child);
    }
  }
  LayoutNode::PerformConstraintLayout(nodes, layoutWidth, layoutHeight, context);
}

void Layer::performContainerLayout(LayoutContext* context) {
  bool horizontal = (layout == LayoutMode::Horizontal);

  float containerMainSize = horizontal ? layoutWidth : layoutHeight;
  float containerCrossSize = horizontal ? layoutHeight : layoutWidth;

  float paddingMainStart = horizontal ? padding.left : padding.top;
  float paddingMainEnd = horizontal ? padding.right : padding.bottom;
  float paddingCrossStart = horizontal ? padding.top : padding.left;
  float paddingCrossEnd = horizontal ? padding.bottom : padding.right;

  float availableMain = containerMainSize - paddingMainStart - paddingMainEnd;

  size_t childCount = children.size();

  // Collect participating children.
  std::vector<size_t> indices = {};
  for (size_t i = 0; i < childCount; i++) {
    if (children[i]->includeInLayout) {
      indices.push_back(i);
    }
  }
  if (indices.empty()) {
    return;
  }

  size_t visibleCount = indices.size();
  float totalGap = gap * static_cast<float>(visibleCount > 1 ? visibleCount - 1 : 0);

  // Determine main-axis sizing: explicit > content-measured > flex.
  enum class SizeMode { Fixed, ContentMeasured, Flex };
  std::vector<SizeMode> sizeMode(childCount, SizeMode::Fixed);
  std::vector<float> childMainSizes(childCount, 0);
  std::vector<float> childCrossSizes(childCount, 0);

  for (auto idx : indices) {
    auto* child = children[idx];
    float explicitMain = horizontal ? child->width : child->height;
    if (!std::isnan(explicitMain)) {
      sizeMode[idx] = SizeMode::Fixed;
      childMainSizes[idx] = explicitMain;
    } else if (child->flex > 0) {
      sizeMode[idx] = SizeMode::Flex;
      childMainSizes[idx] = 0;
    } else {
      sizeMode[idx] = SizeMode::ContentMeasured;
      childMainSizes[idx] = horizontal ? child->preferredWidth : child->preferredHeight;
    }

    float explicitCross = horizontal ? child->height : child->width;
    if (!std::isnan(explicitCross)) {
      childCrossSizes[idx] = explicitCross;
    } else {
      childCrossSizes[idx] = horizontal ? child->preferredHeight : child->preferredWidth;
    }
  }

  // Distribute flex space.
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

  float totalFlexSpace = availableMain - nonFlexTotal - totalGap;
  if (!flexIndices.empty() && totalFlexSpace > 0 && totalFlex > 0) {
    for (auto idx : flexIndices) {
      childMainSizes[idx] = totalFlexSpace * children[idx]->flex / totalFlex;
    }
  }

  // Round flex sizes with error propagation.
  float roundOff = 0;
  for (auto idx : flexIndices) {
    float rawSize = childMainSizes[idx];
    float roundedSize = std::round(rawSize + roundOff);
    roundOff += rawSize - roundedSize;
    childMainSizes[idx] = roundedSize;
  }

  // Calculate total main size.
  float totalMain = 0;
  for (auto idx : indices) {
    totalMain += childMainSizes[idx];
  }
  totalMain += totalGap;

  // Main-axis starting position based on arrangement.
  float mainOffset = paddingMainStart;
  float extraGap = 0;

  switch (arrangement) {
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

  float alignCrossSize = containerCrossSize - paddingCrossStart - paddingCrossEnd;

  // Position each flex child via setLayoutSize + setLayoutPosition + updateLayout.
  for (auto idx : indices) {
    auto* child = children[idx];

    float childMain = childMainSizes[idx];
    float childCross = childCrossSizes[idx];

    // Cross axis alignment.
    float crossPos = paddingCrossStart;
    switch (this->alignment) {
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
        }
        break;
      }
    }

    // Set size via setLayoutSize (does not write back width/height user properties).
    float targetW = horizontal ? childMain : childCross;
    float targetH = horizontal ? childCross : childMain;
    child->setLayoutSize(context, std::round(targetW), std::round(targetH));

    // Set position via setLayoutPosition.
    float posX = horizontal ? mainOffset : crossPos;
    float posY = horizontal ? crossPos : mainOffset;
    child->setLayoutPosition(context, std::round(posX), std::round(posY));

    // Recursively lay out child's children.
    child->updateLayout(context);

    mainOffset += childMain;
    if (arrangement == Arrangement::SpaceBetween || arrangement == Arrangement::SpaceEvenly ||
        arrangement == Arrangement::SpaceAround) {
      mainOffset += extraGap;
    } else {
      mainOffset += gap;
    }
  }
}

}  // namespace pagx
