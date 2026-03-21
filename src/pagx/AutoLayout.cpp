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

#include "AutoLayout.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>
#include "ConstraintLayout.h"
#include "pagx/PAGXDocument.h"
#include "pagx/layout/ElementMeasure.h"
#include "pagx/layout/LayoutUtils.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/types/Alignment.h"
#include "pagx/types/Arrangement.h"
#include "pagx/types/LayoutMode.h"
#include "pagx/types/Rect.h"

namespace pagx {

// Wraps ElementMeasure::GetMeasuredBounds with TextBox write-back support. Triggers
// LayoutUtils::GetContentBounds first for TextBox to ensure dimensions are determined.
static Rect GetMeasuredBounds(const Element* element, FontConfig* fontProvider) {
  if (element->nodeType() == NodeType::TextBox) {
    LayoutUtils::GetContentBounds(element, fontProvider);
  }
  return ElementMeasure::GetMeasuredBounds(element, [fontProvider](const Text* text) {
    return LayoutUtils::ComputeTextBounds(text, fontProvider);
  });
}

// Forward declarations for recursive processing.
static std::pair<float, float> MeasureLayer(const Layer* layer, FontConfig* fontProvider);
static void PerformContainerLayout(Layer* parent, FontConfig* fontProvider);
static void LayoutLayer(Layer* layer, FontConfig* fontProvider);

// Computes the measured content size of a Layer (width and height) for bottom-up measurement.
// Considers constraints on content elements: stretchable elements with opposite-edge constraints
// contribute 0 on the constrained axis (they follow the container).
static std::pair<float, float> MeasureLayerContents(const Layer* layer, FontConfig* fontProvider) {
  float measuredWidth = 0;
  float measuredHeight = 0;

  for (auto* element : layer->contents) {
    auto bounds = GetMeasuredBounds(element, fontProvider);
    if (bounds.isEmpty()) {
      continue;
    }
    measuredWidth = std::max(measuredWidth, bounds.x + bounds.width);
    measuredHeight = std::max(measuredHeight, bounds.y + bounds.height);
  }

  // Also consider child Layers (recursively).
  for (auto* child : layer->children) {
    if (!child->visible) {
      continue;
    }
    auto childMeasured = MeasureLayer(child, fontProvider);
    float cx = child->x + childMeasured.first;
    float cy = child->y + childMeasured.second;
    measuredWidth = std::max(measuredWidth, cx);
    measuredHeight = std::max(measuredHeight, cy);
  }

  return {measuredWidth > 0 ? measuredWidth : 0, measuredHeight > 0 ? measuredHeight : 0};
}

// Performs container layout on a parent Layer that has `layout` set. Arranges child Layers
// along the main axis, distributes flexible sizes, and applies alignment/arrangement.
// The result is that each child Layer gets assigned x, y, width, and height based on the
// container's layout parameters and the child's own size properties and constraints.
static void PerformContainerLayout(Layer* parent, FontConfig* fontProvider) {
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
      childMeasuredSizes[i] = MeasureLayer(children[i], fontProvider);
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
      // Use pre-measured cross size to avoid redundant MeasureLayer calls.
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
        // Stretch only affects children WITHOUT explicit cross-axis size (CSS behavior)
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

// Measures the desired size of a Layer by examining its contents and children (bottom-up).
// Returns {width, height}. For each axis: if the Layer has an explicit dimension, that is used;
// otherwise, the size is derived from content bounds and child Layers.
static std::pair<float, float> MeasureLayer(const Layer* layer, FontConfig* fontProvider) {
  if (layer->measureCached) {
    return {layer->cachedMeasuredWidth, layer->cachedMeasuredHeight};
  }
  float measuredWidth = 0;
  float measuredHeight = 0;

  if (!std::isnan(layer->width)) {
    measuredWidth = layer->width;
  }
  if (!std::isnan(layer->height)) {
    measuredHeight = layer->height;
  }

  // If both dimensions are explicit, no measurement needed.
  if (!std::isnan(layer->width) && !std::isnan(layer->height)) {
    return {measuredWidth, measuredHeight};
  }

  // If this Layer references a Composition, use the Composition's dimensions as intrinsic size.
  if (layer->composition != nullptr) {
    if (std::isnan(layer->width)) {
      measuredWidth = layer->composition->width;
    }
    if (std::isnan(layer->height)) {
      measuredHeight = layer->composition->height;
    }
    layer->measureCached = true;
    layer->cachedMeasuredWidth = measuredWidth;
    layer->cachedMeasuredHeight = measuredHeight;
    return {measuredWidth, measuredHeight};
  }

  if (layer->layout != LayoutMode::None && !layer->children.empty()) {
    // Layout container: measure from child Layers along the layout axis.
    bool horizontal = (layer->layout == LayoutMode::Horizontal);

    float paddingMainStart = horizontal ? layer->padding.left : layer->padding.top;
    float paddingMainEnd = horizontal ? layer->padding.right : layer->padding.bottom;
    float paddingCrossStart = horizontal ? layer->padding.top : layer->padding.left;
    float paddingCrossEnd = horizontal ? layer->padding.bottom : layer->padding.right;

    bool needsMainAxis = horizontal ? std::isnan(layer->width) : std::isnan(layer->height);
    bool needsCrossAxis = horizontal ? std::isnan(layer->height) : std::isnan(layer->width);

    // Measure children once, reuse for both axes.
    if (needsMainAxis || needsCrossAxis) {
      float totalMain = 0;
      float maxCross = 0;
      size_t visibleChildCount = 0;
      for (auto* child : layer->children) {
        if (!child->visible || !child->includeInLayout) {
          continue;
        }
        visibleChildCount++;
        auto childMeasured = MeasureLayer(child, fontProvider);
        if (needsMainAxis) {
          totalMain += horizontal ? childMeasured.first : childMeasured.second;
        }
        if (needsCrossAxis) {
          float childCross = horizontal ? childMeasured.second : childMeasured.first;
          maxCross = std::max(maxCross, childCross);
        }
      }
      if (needsMainAxis) {
        float totalGap =
            layer->gap * static_cast<float>(visibleChildCount > 1 ? visibleChildCount - 1 : 0);
        float mainSize = totalMain + totalGap + paddingMainStart + paddingMainEnd;
        if (horizontal) {
          measuredWidth = mainSize;
        } else {
          measuredHeight = mainSize;
        }
      }
      if (needsCrossAxis) {
        float crossSize = maxCross + paddingCrossStart + paddingCrossEnd;
        if (horizontal) {
          measuredHeight = crossSize;
        } else {
          measuredWidth = crossSize;
        }
      }
    }
  } else {
    // Non-container Layer: measure from contents, considering constraint flexibility.
    auto contentSize = MeasureLayerContents(layer, fontProvider);
    if (std::isnan(layer->width)) {
      measuredWidth = contentSize.first;
    }
    if (std::isnan(layer->height)) {
      measuredHeight = contentSize.second;
    }
  }

  layer->measureCached = true;
  layer->cachedMeasuredWidth = measuredWidth;
  layer->cachedMeasuredHeight = measuredHeight;
  return {measuredWidth, measuredHeight};
}

static void ClearMeasureCache(Layer* layer) {
  layer->measureCached = false;
  for (auto* child : layer->children) {
    ClearMeasureCache(child);
  }
}

// Layout phase (top-down): determines final sizes and positions for all Layers and their contents.
// Measurement happens as needed when dimensions are not explicit. Each axis is measured
// independently to avoid redundant computation.
static void LayoutLayer(Layer* layer, FontConfig* fontProvider) {
  // Step 1: If this Layer has layout but missing dimensions, use measured sizes.
  // The parent's PerformContainerLayout may have already assigned sizes for flexible children.
  // This handles the case where this Layer is a top-level container without explicit dimensions.
  if (layer->layout != LayoutMode::None && !layer->children.empty()) {
    if (std::isnan(layer->width) || std::isnan(layer->height)) {
      auto measured = MeasureLayer(layer, fontProvider);
      if (std::isnan(layer->width)) {
        layer->width = measured.first;
      }
      if (std::isnan(layer->height)) {
        layer->height = measured.second;
      }
    }
  }

  // Step 2: Container layout — arrange child Layers and assign flexible sizes.
  if (layer->layout != LayoutMode::None && !layer->children.empty()) {
    PerformContainerLayout(layer, fontProvider);
  }

  // Step 3: For Layers without container layout and missing explicit dimensions, adopt measured
  // sizes so that constraint positioning within contents can work. Skip if dimensions are already
  // set. Also skip if this Layer has no contents and no children (nothing to measure or constrain).
  if (layer->layout == LayoutMode::None &&
      (std::isnan(layer->width) || std::isnan(layer->height)) &&
      (!layer->contents.empty() || !layer->children.empty())) {
    auto measured = MeasureLayer(layer, fontProvider);
    if (std::isnan(layer->width)) {
      layer->width = measured.first;
    }
    if (std::isnan(layer->height)) {
      layer->height = measured.second;
    }
  }

  // Step 4: Constraint positioning — position elements within this Layer.
  // Skip if there are no contents to constrain.
  if (!layer->contents.empty() && (!std::isnan(layer->width) || !std::isnan(layer->height))) {
    float containerW = std::isnan(layer->width) ? 0 : layer->width;
    float containerH = std::isnan(layer->height) ? 0 : layer->height;
    ConstraintLayout::ApplyElementsConstraints(layer->contents, containerW, containerH,
                                               fontProvider);
  }

  // Step 5: Apply constraint positioning to child Layers.
  // Constraints on child Layers take effect when:
  // - The parent has no container layout, or
  // - The child has includeInLayout=false (opted out of container layout flow).
  if (!layer->children.empty() && !std::isnan(layer->width) && !std::isnan(layer->height)) {
    for (auto* child : layer->children) {
      if (!child->visible) {
        continue;
      }
      bool hasConstraints = !std::isnan(child->left) || !std::isnan(child->right) ||
                            !std::isnan(child->top) || !std::isnan(child->bottom) ||
                            !std::isnan(child->centerX) || !std::isnan(child->centerY);
      if (!hasConstraints) {
        continue;
      }
      bool applyConstraints = layer->layout == LayoutMode::None || !child->includeInLayout;
      if (!applyConstraints) {
        continue;
      }
      auto childMeasured = MeasureLayer(child, fontProvider);
      ConstraintLayout::ApplyLayerConstraints(child, layer->width, layer->height,
                                              childMeasured.first, childMeasured.second);
    }
  }

  // Step 6: Recurse into visible child Layers.
  for (auto* child : layer->children) {
    if (!child->visible) {
      continue;
    }
    LayoutLayer(child, fontProvider);
  }
}

// Rounds all layout-computed coordinates and sizes to integer pixels to avoid subpixel rendering.
static void SnapToPixelGrid(Layer* layer) {
  layer->x = std::round(layer->x);
  layer->y = std::round(layer->y);
  if (!std::isnan(layer->width)) {
    layer->width = std::round(layer->width);
  }
  if (!std::isnan(layer->height)) {
    layer->height = std::round(layer->height);
  }
  for (auto* child : layer->children) {
    SnapToPixelGrid(child);
  }
}

// Applies constraint positioning and auto layout to a set of layers within a container of the given
// dimensions. Used for both document-level layers and Composition layers.
static void ApplyLayoutToLayers(const std::vector<Layer*>& layers, float containerWidth,
                                float containerHeight, FontConfig* fontProvider) {
  for (auto* layer : layers) {
    ClearMeasureCache(layer);
  }
  // Apply constraint positioning for top-level layers using the container dimensions.
  for (auto* layer : layers) {
    if (!layer->visible) {
      continue;
    }
    auto measured = MeasureLayer(layer, fontProvider);
    ConstraintLayout::ApplyLayerConstraints(layer, containerWidth, containerHeight, measured.first,
                                            measured.second);
  }
  for (auto* layer : layers) {
    LayoutLayer(layer, fontProvider);
  }
  for (auto* layer : layers) {
    SnapToPixelGrid(layer);
  }
}

void AutoLayout::Apply(PAGXDocument* document, FontConfig* fontProvider) {
  if (document == nullptr) {
    return;
  }
  // Process Composition layers first, since they may be referenced by document layers.
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Composition) {
      auto* comp = static_cast<Composition*>(node.get());
      if (!comp->layers.empty()) {
        ApplyLayoutToLayers(comp->layers, comp->width, comp->height, fontProvider);
      }
    }
  }
  // Process document-level layers.
  ApplyLayoutToLayers(document->layers, document->width, document->height, fontProvider);
}

}  // namespace pagx
