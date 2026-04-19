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
#include <vector>
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/LayoutNode.h"
#include "pagx/types/Alignment.h"
#include "pagx/types/Arrangement.h"
#include "pagx/types/LayoutMode.h"
#include "pagx/types/Rect.h"

namespace pagx {

static float ComputeCrossTarget(Alignment alignment, bool horizontal, const Layer* child,
                                float alignCrossSize) {
  if (alignment != Alignment::Stretch) {
    return NAN;
  }
  // Any explicit cross-axis size (absolute or percentage) opts out of stretch.
  bool hasExplicitCross = horizontal
                              ? (!std::isnan(child->height) || !std::isnan(child->percentHeight))
                              : (!std::isnan(child->width) || !std::isnan(child->percentWidth));
  if (hasExplicitCross) {
    return NAN;
  }
  return alignCrossSize;
}

// Returns true if child has an explicit main-axis size (absolute or percentage).
static bool HasExplicitMainSize(bool horizontal, const Layer* child) {
  if (horizontal) {
    return !std::isnan(child->percentWidth) || !std::isnan(child->width);
  }
  return !std::isnan(child->percentHeight) || !std::isnan(child->height);
}

// Returns the target main-axis size computed from explicit dimensions: percentage resolves against
// containerMain; otherwise falls back to the absolute width/height. Returns NAN when neither is set.
static float ComputeExplicitMainSize(bool horizontal, const Layer* child, float containerMain) {
  if (horizontal) {
    if (!std::isnan(child->percentWidth)) {
      return std::ceil(containerMain * child->percentWidth / 100.0f);
    }
    return child->width;
  }
  if (!std::isnan(child->percentHeight)) {
    return std::ceil(containerMain * child->percentHeight / 100.0f);
  }
  return child->height;
}

// Returns the target cross-axis size computed from explicit dimensions: percentage resolves against
// containerCross; otherwise falls back to the absolute width/height. Returns NAN when neither is set.
static float ComputeExplicitCrossSize(bool horizontal, const Layer* child, float containerCross) {
  if (horizontal) {
    if (!std::isnan(child->percentHeight)) {
      return std::ceil(containerCross * child->percentHeight / 100.0f);
    }
    return child->height;
  }
  if (!std::isnan(child->percentWidth)) {
    return std::ceil(containerCross * child->percentWidth / 100.0f);
  }
  return child->width;
}

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
  // Preferred size: if both width/height are authored, use them directly (skip content
  // measurement). Otherwise compute per-axis from composition/children/contents, letting an
  // authored axis still override the content-measured value. percentWidth/percentHeight are not
  // consulted here; they are resolved by the parent via PerformConstraintLayout.
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
      // preferredWidth/Height already folds in the child's authored width/height.
      // percentWidth/percentHeight cannot participate because the parent's layout size is not
      // known yet.
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
    // Add padding to content measurement, consistent with layout mode behavior.
    measuredW += padding.left + padding.right;
    measuredH += padding.top + padding.bottom;
  }

  preferredWidth = !std::isnan(width) ? width : measuredW;
  preferredHeight = !std::isnan(height) ? height : measuredH;
}

void Layer::setLayoutSize(LayoutContext* context, float targetWidth, float targetHeight) {
  // A content-measured axis is one the parent did not constrain and the layer did not author.
  // For a non-flex layer, keep such axes NaN during the first updateLayout so percent-sized
  // descendants fall back to their preferred size instead of locking onto a provisional value
  // derived from this layer's onMeasure. After the first pass, refine the axis from children's
  // actual bounds and run updateLayout again so size-dependent descendants pick up the final
  // container size. Flex layers keep the provisional size on both axes because their child
  // allocation logic requires numeric main-axis dimensions.
  bool widthFromContent = std::isnan(targetWidth) && std::isnan(this->width);
  bool heightFromContent = std::isnan(targetHeight) && std::isnan(this->height);
  // A Layer that references a Composition takes its preferred size from that Composition, so it
  // is not a content-measured axis; do not defer in that case.
  bool canDefer = layout == LayoutMode::None && composition == nullptr;
  bool deferWidth = widthFromContent && canDefer;
  bool deferHeight = heightFromContent && canDefer;
  layoutWidth = deferWidth ? NAN : (!std::isnan(targetWidth) ? targetWidth : preferredWidth);
  layoutHeight = deferHeight ? NAN : (!std::isnan(targetHeight) ? targetHeight : preferredHeight);
  if (clipToBounds && !hasScrollRect && !std::isnan(layoutWidth) && !std::isnan(layoutHeight)) {
    scrollRect = Rect::MakeXYWH(0, 0, layoutWidth, layoutHeight);
    hasScrollRect = true;
  }
  updateLayout(context);
  // An axis is content-measured when neither the parent nor this layer authored its size. When
  // one axis was forced by the parent, re-measure the content-measured axis from children's
  // actual layout sizes to reflect wrapping / flex redistribution.
  bool sizeChanged = (!std::isnan(targetWidth) && targetWidth != preferredWidth) ||
                     (!std::isnan(targetHeight) && targetHeight != preferredHeight);
  if (!(widthFromContent || heightFromContent) || (!deferWidth && !deferHeight && !sizeChanged)) {
    return;
  }
  float maxX = 0;
  float maxY = 0;
  for (auto* element : contents) {
    auto* node = LayoutNode::AsLayoutNode(element);
    if (node == nullptr) {
      continue;
    }
    // Match Layer::onMeasure / MeasureChildNodes: unconstrained nodes use their preferredX/Y as
    // extent origin (so an element authored with a negative preferredY like a centered Path is
    // not measured as if it started at 0).
    float extX = node->hasConstraints() ? node->constraintExtentX() : node->preferredX;
    float extY = node->hasConstraints() ? node->constraintExtentY() : node->preferredY;
    extX += node->layoutBounds().width;
    extY += node->layoutBounds().height;
    maxX = std::max(maxX, extX);
    maxY = std::max(maxY, extY);
  }
  if (layout != LayoutMode::None && !children.empty()) {
    bool horizontal = (layout == LayoutMode::Horizontal);
    float totalMain = 0;
    float maxCross = 0;
    size_t visibleCount = 0;
    for (auto* child : children) {
      if (!child->includeInLayout) {
        continue;
      }
      visibleCount++;
      float childMain = horizontal ? child->layoutWidth : child->layoutHeight;
      float childCross = horizontal ? child->layoutHeight : child->layoutWidth;
      totalMain += childMain;
      maxCross = std::max(maxCross, childCross);
    }
    float totalGap = gap * static_cast<float>(visibleCount > 1 ? visibleCount - 1 : 0);
    float mainSize = totalMain + totalGap +
                     (horizontal ? padding.left + padding.right : padding.top + padding.bottom);
    float crossSize =
        maxCross + (horizontal ? padding.top + padding.bottom : padding.left + padding.right);
    if (horizontal) {
      maxX = std::max(maxX, mainSize);
      maxY = std::max(maxY, crossSize);
    } else {
      maxY = std::max(maxY, mainSize);
      maxX = std::max(maxX, crossSize);
    }
  } else {
    for (auto* child : children) {
      if (!child->includeInLayout) {
        continue;
      }
      float extX = child->constraintExtentX() + child->layoutWidth;
      float extY = child->constraintExtentY() + child->layoutHeight;
      maxX = std::max(maxX, extX);
      maxY = std::max(maxY, extY);
    }
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
  // If we deferred an axis to NaN in pass 1, we must run updateLayout again with the refined
  // numeric value so size-dependent descendants (e.g. width/height=100%) pick it up.
  bool needSecondPass = (deferWidth && !std::isnan(layoutWidth) && layoutWidth != prevW) ||
                        (deferHeight && !std::isnan(layoutHeight) && layoutHeight != prevH);
  if (needSecondPass) {
    if (clipToBounds && hasScrollRect) {
      scrollRect = Rect::MakeXYWH(0, 0, layoutWidth, layoutHeight);
    } else if (clipToBounds && !hasScrollRect && !std::isnan(layoutWidth) &&
               !std::isnan(layoutHeight)) {
      scrollRect = Rect::MakeXYWH(0, 0, layoutWidth, layoutHeight);
      hasScrollRect = true;
    }
    updateLayout(context);
  }
}

Point Layer::renderPosition() const {
  auto bounds = layoutBounds();
  return {bounds.x, bounds.y};
}

void Layer::updateLayout(LayoutContext* context) {
  // Flex container layout: arrange flex children.
  if (layout != LayoutMode::None && !children.empty()) {
    performContainerLayout(context);
  }

  // VectorElements (contents): use padding-inset bounds.
  auto contentNodes = LayoutNode::CollectLayoutNodes(contents, false);
  LayoutNode::PerformConstraintLayout(contentNodes, layoutWidth, layoutHeight, padding, context);

  // Child Layers not in flex flow: use padding-inset bounds.
  std::vector<LayoutNode*> childNodes;
  for (auto* child : children) {
    bool inFlexFlow = (layout != LayoutMode::None && child->includeInLayout);
    if (!inFlexFlow) {
      childNodes.push_back(child);
    }
  }
  if (!childNodes.empty()) {
    LayoutNode::PerformConstraintLayout(childNodes, layoutWidth, layoutHeight, padding, context);
  }
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
  float alignCrossSize = containerCrossSize - paddingCrossStart - paddingCrossEnd;

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

  // ==================== Pass 1: Determine sizes ====================
  std::vector<float> childMainSizes(childCount, 0);
  std::vector<float> childCrossSizes(childCount, 0);
  std::vector<size_t> flexIndices = {};
  float totalFlex = 0;

  for (auto idx : indices) {
    auto* child = children[idx];

    // Cross-axis: explicit percentage/absolute size takes priority over stretch alignment.
    float explicitCross = ComputeExplicitCrossSize(horizontal, child, alignCrossSize);
    float crossTarget = !std::isnan(explicitCross)
                            ? explicitCross
                            : ComputeCrossTarget(alignment, horizontal, child, alignCrossSize);

    // Explicit main-axis size (percentage or absolute) takes priority over flex.
    if (child->flex > 0 && !HasExplicitMainSize(horizontal, child)) {
      flexIndices.push_back(idx);
      totalFlex += child->flex;
      childMainSizes[idx] = 0;
    } else {
      // Non-flex: main-axis from explicit size (percentage/absolute) or NaN for child-sizing.
      float explicitMain = ComputeExplicitMainSize(horizontal, child, availableMain);
      float targetW = horizontal ? explicitMain : crossTarget;
      float targetH = horizontal ? crossTarget : explicitMain;
      child->setLayoutSize(context, targetW, targetH);
      childMainSizes[idx] = horizontal ? child->layoutWidth : child->layoutHeight;
    }
    childCrossSizes[idx] = horizontal ? child->layoutHeight : child->layoutWidth;
  }

  // Distribute flex space. Non-flex children (including explicit-size children with flex > 0)
  // already have their main sizes set; flex children have childMainSizes = 0.
  float nonFlexTotal = 0;
  for (auto idx : indices) {
    nonFlexTotal += childMainSizes[idx];
  }

  float totalFlexSpace = availableMain - nonFlexTotal - totalGap;
  if (!flexIndices.empty() && totalFlexSpace > 0 && totalFlex > 0) {
    float roundOff = 0;
    for (auto idx : flexIndices) {
      float rawSize = totalFlexSpace * children[idx]->flex / totalFlex;
      float roundedSize = std::round(rawSize + roundOff);
      roundOff += rawSize - roundedSize;
      childMainSizes[idx] = roundedSize;
    }
  }

  // setLayoutSize for flex children: main-axis forced, cross-axis from explicit/stretch.
  for (auto idx : flexIndices) {
    auto* child = children[idx];
    float explicitCross = ComputeExplicitCrossSize(horizontal, child, alignCrossSize);
    float crossTarget = !std::isnan(explicitCross)
                            ? explicitCross
                            : ComputeCrossTarget(alignment, horizontal, child, alignCrossSize);
    float targetW = horizontal ? childMainSizes[idx] : crossTarget;
    float targetH = horizontal ? crossTarget : childMainSizes[idx];
    child->setLayoutSize(context, targetW, targetH);
    childCrossSizes[idx] = horizontal ? child->layoutHeight : child->layoutWidth;
  }

  // ==================== Pass 2: Position ====================
  float totalMain = totalGap;
  for (auto idx : indices) {
    totalMain += childMainSizes[idx];
  }

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

  for (auto idx : indices) {
    auto* child = children[idx];
    float childMain = childMainSizes[idx];
    float childCross = childCrossSizes[idx];

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
      case Alignment::Stretch:
        break;
    }

    float posX = horizontal ? mainOffset : crossPos;
    float posY = horizontal ? crossPos : mainOffset;
    child->setLayoutPosition(context, std::round(posX), std::round(posY));

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
