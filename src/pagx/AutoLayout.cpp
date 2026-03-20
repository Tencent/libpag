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
#include <limits>
#include <string>
#include <utility>
#include <vector>
#include "base/utils/Log.h"
#include "pagx/FontProvider.h"
#include "pagx/PAGXDocument.h"
#include "pagx/layout/ElementConstraint.h"
#include "pagx/layout/ElementMeasure.h"
#include "pagx/layout/ElementTransform.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/types/Alignment.h"
#include "pagx/types/Arrangement.h"
#include "pagx/types/Constraints.h"
#include "pagx/types/LayoutMode.h"
#include "pagx/types/Rect.h"
#include "tgfx/core/Font.h"
#include "tgfx/core/TextBlob.h"
#include "tgfx/core/Typeface.h"

namespace pagx {

// Computes the bounds of a Text element using tgfx::TextBlob::getTightBounds() for precise
// measurement. Falls back to estimation when font is unavailable.
// Returns bounds in absolute coordinates (including text->position and textAnchor).
static Rect ComputeTextBounds(const Text* text, FontConfig* fontProvider) {
  if (text == nullptr || text->text.empty()) {
    return {};
  }

  std::shared_ptr<tgfx::Typeface> typeface = nullptr;
  if (fontProvider != nullptr) {
    typeface = fontProvider->findTypeface(text->fontFamily, text->fontStyle);
  }
  if (typeface == nullptr) {
    typeface = tgfx::Typeface::MakeFromName(text->fontFamily, text->fontStyle);
  }

  float textWidth;
  float textHeight;
  float boundsTop;  // Y offset from baseline to top of bounds (negative = above baseline)

  if (typeface != nullptr) {
    tgfx::Font font(typeface, text->fontSize);
    font.setFauxBold(text->fauxBold);
    font.setFauxItalic(text->fauxItalic);
    auto textBlob = tgfx::TextBlob::MakeFrom(text->text, font);
    if (textBlob != nullptr) {
      auto bounds = textBlob->getTightBounds();
      textWidth = bounds.right - bounds.left;
      textHeight = bounds.bottom - bounds.top;
      boundsTop = bounds.top;
    } else {
      textWidth = static_cast<float>(text->text.size()) * text->fontSize * 0.6f;
      textHeight = text->fontSize;
      boundsTop = -text->fontSize * 0.8f;
    }
  } else {
    textWidth = static_cast<float>(text->text.size()) * text->fontSize * 0.6f;
    textHeight = text->fontSize;
    boundsTop = -text->fontSize * 0.8f;
  }

  // Coordinate convention: position.x is the anchor point, position.y is the baseline.
  // boundsTop is relative to baseline (negative = above), so boundsY = baseline + boundsTop.
  float boundsX = text->position.x;
  float boundsY = text->position.y + boundsTop;
  if (text->textAnchor == TextAnchor::Center) {
    boundsX = text->position.x - textWidth * 0.5f;
  } else if (text->textAnchor == TextAnchor::End) {
    boundsX = text->position.x - textWidth;
  }
  return Rect::MakeXYWH(boundsX, boundsY, textWidth, textHeight);
}

// Local wrapper for ElementMeasure::GetContentBounds that uses ComputeTextBounds.
static Rect GetContentBounds(const Element* element, FontConfig* fontProvider) {
  return ElementMeasure::GetContentBounds(
      element, [fontProvider](const Text* text) { return ComputeTextBounds(text, fontProvider); });
}

// Local wrapper for ElementMeasure::GetMeasuredBounds that uses ComputeTextBounds.
static Rect GetMeasuredBounds(const Element* element, FontConfig* fontProvider) {
  return ElementMeasure::GetMeasuredBounds(
      element, [fontProvider](const Text* text) { return ComputeTextBounds(text, fontProvider); });
}

// Applies constraint layout to a single element within a container of the given dimensions.
// When skipScaling is true, non-stretchable elements will center without scaling (used for Text
// elements controlled by a TextBox).
static void ApplyConstraintToElement(Element* element, float containerWidth, float containerHeight,
                                     FontConfig* fontProvider, bool skipScaling = false) {
  auto constraints = ElementConstraint::GetConstraints(element);
  if (!constraints.hasAny()) {
    return;
  }
  std::string errorMessage;
  if (!constraints.isValid(errorMessage)) {
    LOGE("[Layout Warning] %s", errorMessage.c_str());
  }
  auto bounds = GetContentBounds(element, fontProvider);
  auto type = element->nodeType();
  bool stretchable = ElementConstraint::IsStretchable(type);

  // --- Proportional scaling for non-stretchable, non-Group elements ---
  // When opposite-edge constraints define a target area, scale the element to fit while preserving
  // aspect ratio. Each axis with opposite-edge constraints contributes a candidate scale factor;
  // the minimum is used so the element fits without exceeding the area. An axis without
  // opposite-edge constraints uses the measured size (scale factor = 1), so single-axis constraints
  // only scale when the target is smaller or larger than the original.
  if (!stretchable && !skipScaling && type != NodeType::Group) {
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
        bounds = GetContentBounds(element, fontProvider);
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
      // Group aligns to the target area and derives layout width.
      auto* group = static_cast<Group*>(element);
      group->position.x = left;
      ElementTransform::UpdateGroupLayoutSize(group, areaWidth, NAN);
    } else if (stretchable) {
      // Stretch to fill the target area.
      float newCenterX = left + areaWidth * 0.5f;
      ElementTransform::ApplyHorizontalStretch(element, areaWidth, newCenterX);
    } else {
      // Center within the target area (already scaled proportionally above).
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
      // Group aligns to the target area and derives layout height.
      auto* group = static_cast<Group*>(element);
      group->position.y = top;
      ElementTransform::UpdateGroupLayoutSize(group, NAN, areaHeight);
    } else if (stretchable) {
      // Stretch to fill the target area.
      float newCenterY = top + areaHeight * 0.5f;
      ElementTransform::ApplyVerticalStretch(element, areaHeight, newCenterY);
    } else {
      // Center within the target area (already scaled proportionally above).
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

// Forward declarations for recursive processing.
static void ProcessConstraintLayout(const std::vector<Element*>& elements, float containerWidth,
                                    float containerHeight, FontConfig* fontProvider);
static std::pair<float, float> MeasureLayer(const Layer* layer, FontConfig* fontProvider);
static void LayoutLayer(Layer* layer, FontConfig* fontProvider);

// Applies constraint layout to all elements within a container. Group elements are always
// processed recursively: explicit or constraint-derived dimensions are used when available,
// otherwise the Group's content bounds are measured as a fallback reference frame.
static void ProcessConstraintLayout(const std::vector<Element*>& elements, float containerWidth,
                                    float containerHeight, FontConfig* fontProvider) {
  // Check if any TextBox exists in this scope. When present, Text elements are controlled by the
  // TextBox for typesetting, so their fontSize should not be modified by proportional scaling.
  bool hasTextBox = false;
  for (auto* element : elements) {
    if (element->nodeType() == NodeType::TextBox) {
      hasTextBox = true;
      break;
    }
  }

  for (auto* element : elements) {
    bool skipScaling = hasTextBox && element->nodeType() == NodeType::Text;
    ApplyConstraintToElement(element, containerWidth, containerHeight, fontProvider, skipScaling);

    if (element->nodeType() == NodeType::Group || element->nodeType() == NodeType::TextBox) {
      auto* group = static_cast<Group*>(element);
      float groupW = group->width;
      float groupH = group->height;
      // Fall back to measured content bounds when no explicit dimensions.
      if (std::isnan(groupW) || std::isnan(groupH)) {
        auto bounds = GetContentBounds(group, fontProvider);
        if (std::isnan(groupW)) {
          groupW = bounds.width;
        }
        if (std::isnan(groupH)) {
          groupH = bounds.height;
        }
      }
      ProcessConstraintLayout(group->elements, groupW, groupH, fontProvider);
    }
  }
}

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
    if (parent->arrangement == Arrangement::SpaceBetween) {
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

  if (layer->layout != LayoutMode::Constraint && !layer->children.empty()) {
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
    // Non-layout Layer: measure from contents, considering constraint flexibility.
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
  if (layer->layout != LayoutMode::Constraint && !layer->children.empty()) {
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
  if (layer->layout != LayoutMode::Constraint && !layer->children.empty()) {
    PerformContainerLayout(layer, fontProvider);
  }

  // Step 3: For non-layout Layers without explicit dimensions, adopt measured sizes so that
  // constraint layout within contents can work. Skip if dimensions are already set.
  // Also skip if this Layer has no contents and no children (nothing to measure or constrain).
  if (layer->layout == LayoutMode::Constraint &&
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

  // Step 4: Constraint layout — position elements within this Layer.
  // Skip if there are no contents to constrain.
  if (!layer->contents.empty() && (!std::isnan(layer->width) || !std::isnan(layer->height))) {
    float containerW = std::isnan(layer->width) ? 0 : layer->width;
    float containerH = std::isnan(layer->height) ? 0 : layer->height;
    ProcessConstraintLayout(layer->contents, containerW, containerH, fontProvider);
  }

  // Step 5: Apply constraint layout to child Layers that have constraint properties.
  // Constraints on child Layers take effect when:
  // - The parent uses absolute layout (no container layout), or
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
      bool applyConstraints = layer->layout == LayoutMode::Constraint || !child->includeInLayout;
      if (!applyConstraints) {
        continue;
      }
      auto childMeasured = MeasureLayer(child, fontProvider);
      float childWidth = std::isnan(child->width) ? childMeasured.first : child->width;
      float childHeight = std::isnan(child->height) ? childMeasured.second : child->height;

      // Horizontal axis
      bool hasLeft = !std::isnan(child->left);
      bool hasRight = !std::isnan(child->right);
      bool hasCenterX = !std::isnan(child->centerX);
      if (hasLeft && hasRight) {
        child->x = child->left;
        child->width = layer->width - child->left - child->right;
      } else if (hasLeft) {
        child->x = child->left;
      } else if (hasRight) {
        child->x = layer->width - child->right - childWidth;
      } else if (hasCenterX) {
        child->x = (layer->width - childWidth) * 0.5f + child->centerX;
      }

      // Vertical axis
      bool hasTop = !std::isnan(child->top);
      bool hasBottom = !std::isnan(child->bottom);
      bool hasCenterY = !std::isnan(child->centerY);
      if (hasTop && hasBottom) {
        child->y = child->top;
        child->height = layer->height - child->top - child->bottom;
      } else if (hasTop) {
        child->y = child->top;
      } else if (hasBottom) {
        child->y = layer->height - child->bottom - childHeight;
      } else if (hasCenterY) {
        child->y = (layer->height - childHeight) * 0.5f + child->centerY;
      }
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

// Applies constraint layout and auto layout to a set of layers within a container of the given
// dimensions. Used for both document-level layers and Composition layers.
static void ApplyLayoutToLayers(const std::vector<Layer*>& layers, float containerWidth,
                                float containerHeight, FontConfig* fontProvider) {
  for (auto* layer : layers) {
    ClearMeasureCache(layer);
  }
  // Apply constraint layout for layers using the container dimensions.
  for (auto* layer : layers) {
    if (!layer->visible) {
      continue;
    }
    bool hasConstraints = !std::isnan(layer->left) || !std::isnan(layer->right) ||
                          !std::isnan(layer->top) || !std::isnan(layer->bottom) ||
                          !std::isnan(layer->centerX) || !std::isnan(layer->centerY);
    if (!hasConstraints) {
      continue;
    }
    auto measured = MeasureLayer(layer, fontProvider);
    float childWidth = std::isnan(layer->width) ? measured.first : layer->width;
    float childHeight = std::isnan(layer->height) ? measured.second : layer->height;

    // Horizontal axis
    bool hasLeft = !std::isnan(layer->left);
    bool hasRight = !std::isnan(layer->right);
    bool hasCenterX = !std::isnan(layer->centerX);
    if (hasLeft && hasRight) {
      layer->x = layer->left;
      layer->width = containerWidth - layer->left - layer->right;
    } else if (hasLeft) {
      layer->x = layer->left;
    } else if (hasRight) {
      layer->x = containerWidth - layer->right - childWidth;
    } else if (hasCenterX) {
      layer->x = (containerWidth - childWidth) * 0.5f + layer->centerX;
    }

    // Vertical axis
    bool hasTop = !std::isnan(layer->top);
    bool hasBottom = !std::isnan(layer->bottom);
    bool hasCenterY = !std::isnan(layer->centerY);
    if (hasTop && hasBottom) {
      layer->y = layer->top;
      layer->height = containerHeight - layer->top - layer->bottom;
    } else if (hasTop) {
      layer->y = layer->top;
    } else if (hasBottom) {
      layer->y = containerHeight - layer->bottom - childHeight;
    } else if (hasCenterY) {
      layer->y = (containerHeight - childHeight) * 0.5f + layer->centerY;
    }
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
