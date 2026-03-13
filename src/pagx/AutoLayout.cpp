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
#include <optional>
#include <utility>
#include <vector>
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/GlyphRun.h"
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
#include "pagx/types/LayoutDirection.h"
#include "pagx/types/Rect.h"

namespace pagx {

// Computes the bounds of a Text element from its GlyphRuns (precise) or estimates from text
// properties (fallback). Returns the axis-aligned bounding box.
static Rect ComputeTextBounds(const Text* text) {
  float minX = std::numeric_limits<float>::max();
  float minY = std::numeric_limits<float>::max();
  float maxX = std::numeric_limits<float>::lowest();
  float maxY = std::numeric_limits<float>::lowest();
  bool hasGlyph = false;

  // Use GlyphRuns if available (precise).
  if (!text->glyphRuns.empty()) {
    for (auto* glyphRun : text->glyphRuns) {
      if (glyphRun == nullptr || glyphRun->glyphs.empty()) {
        continue;
      }
      // Use GlyphRun position as base (in text coordinate space).
      float baseX = glyphRun->x;
      float baseY = glyphRun->y;
      // Accumulate bounds from all glyphs.
      for (size_t i = 0; i < glyphRun->glyphs.size(); i++) {
        float glyphX = baseX;
        if (i < glyphRun->xOffsets.size()) {
          glyphX += glyphRun->xOffsets[i];
        }
        if (i < glyphRun->positions.size()) {
          glyphX += glyphRun->positions[i].x;
        }
        float glyphY = baseY;
        if (i < glyphRun->positions.size()) {
          glyphY += glyphRun->positions[i].y;
        }
        // Estimate glyph bounds as a box (advance * scale, fontSize * scale).
        // This is conservative: actual glyph bounds depend on font metrics.
        float scaleX = 1.0f;
        float scaleY = 1.0f;
        if (i < glyphRun->scales.size()) {
          scaleX = glyphRun->scales[i].x;
          scaleY = glyphRun->scales[i].y;
        }
        float glyphWidth = glyphRun->fontSize * 0.6f * scaleX;     // Rough advance estimate
        float glyphHeight = glyphRun->fontSize * scaleY;           // Full height
        float glyphYOffset = -glyphRun->fontSize * 0.8f * scaleY;  // Ascent estimate
        minX = std::min(minX, glyphX - glyphWidth * 0.5f);
        minY = std::min(minY, glyphY + glyphYOffset);
        maxX = std::max(maxX, glyphX + glyphWidth * 0.5f);
        maxY = std::max(maxY, glyphY + glyphHeight * 0.2f);
        hasGlyph = true;
      }
    }
  }

  // Fallback: estimate from text properties.
  if (!hasGlyph) {
    float estimatedWidth = static_cast<float>(text->text.size()) * text->fontSize * 0.6f;
    float ascent = text->fontSize * 0.8f;
    float descent = text->fontSize * 0.2f;
    float boundsX = 0.0f;
    float boundsY = -ascent;
    if (text->textAnchor == TextAnchor::Center) {
      boundsX = -estimatedWidth * 0.5f;
    } else if (text->textAnchor == TextAnchor::End) {
      boundsX = -estimatedWidth;
    }
    minX = boundsX;
    minY = boundsY;
    maxX = boundsX + estimatedWidth;
    maxY = boundsY + (ascent + descent);
  }

  if (minX > maxX) {
    return {};
  }
  float width = maxX - minX;
  float height = maxY - minY;
  return Rect::MakeXYWH(text->position.x + minX, text->position.y + minY, width, height);
}

// Returns the content bounds of an element in its local coordinate system, without applying
// constraints. For center-anchored elements (Rectangle, Ellipse), the bounds are symmetric around
// position. For origin-anchored elements (Path, Group), the bounds start from the content's actual
// coordinates.
static Rect GetContentBounds(const Element* element) {
  switch (element->nodeType()) {
    case NodeType::Rectangle: {
      auto* rect = static_cast<const Rectangle*>(element);
      float halfW = rect->size.width * 0.5f;
      float halfH = rect->size.height * 0.5f;
      return Rect::MakeXYWH(rect->position.x - halfW, rect->position.y - halfH, rect->size.width,
                            rect->size.height);
    }
    case NodeType::Ellipse: {
      auto* ellipse = static_cast<const Ellipse*>(element);
      float halfW = ellipse->size.width * 0.5f;
      float halfH = ellipse->size.height * 0.5f;
      return Rect::MakeXYWH(ellipse->position.x - halfW, ellipse->position.y - halfH,
                            ellipse->size.width, ellipse->size.height);
    }
    case NodeType::Polystar: {
      auto* polystar = static_cast<const Polystar*>(element);
      float r = polystar->outerRadius;
      return Rect::MakeXYWH(polystar->position.x - r, polystar->position.y - r, r * 2.0f, r * 2.0f);
    }
    case NodeType::Path: {
      auto* path = static_cast<const Path*>(element);
      if (path->data != nullptr) {
        auto bounds = path->data->getBounds();
        return Rect::MakeXYWH(path->position.x + bounds.x, path->position.y + bounds.y,
                              bounds.width, bounds.height);
      }
      return {};
    }
    case NodeType::Text: {
      return ComputeTextBounds(static_cast<const Text*>(element));
    }
    case NodeType::TextBox: {
      auto* textBox = static_cast<const TextBox*>(element);
      return Rect::MakeXYWH(textBox->position.x, textBox->position.y, textBox->size.width,
                            textBox->size.height);
    }
    case NodeType::Group: {
      auto* group = static_cast<const Group*>(element);
      if (group->width.has_value() && group->height.has_value()) {
        return Rect::MakeXYWH(group->position.x, group->position.y, *group->width, *group->height);
      }
      // Without explicit dimensions, compute bounds from child elements.
      float minX = std::numeric_limits<float>::max();
      float minY = std::numeric_limits<float>::max();
      float maxX = std::numeric_limits<float>::lowest();
      float maxY = std::numeric_limits<float>::lowest();
      bool hasContent = false;
      for (auto* child : group->elements) {
        auto childBounds = GetContentBounds(child);
        if (childBounds.isEmpty()) {
          continue;
        }
        hasContent = true;
        minX = std::min(minX, childBounds.x);
        minY = std::min(minY, childBounds.y);
        maxX = std::max(maxX, childBounds.x + childBounds.width);
        maxY = std::max(maxY, childBounds.y + childBounds.height);
      }
      if (!hasContent) {
        return Rect::MakeXYWH(group->position.x, group->position.y, 0, 0);
      }
      return Rect::MakeXYWH(group->position.x + minX, group->position.y + minY, maxX - minX,
                            maxY - minY);
    }
    default:
      return {};
  }
}

// Returns true if the element type supports stretching when opposite edges are both constrained.
static bool IsStretchable(NodeType type) {
  return type == NodeType::Rectangle || type == NodeType::Ellipse || type == NodeType::TextBox;
}

// Returns the constraints for an element, or nullptr if the element has no constraint support.
static const Constraints* GetConstraints(const Element* element) {
  switch (element->nodeType()) {
    case NodeType::Rectangle:
      return &static_cast<const Rectangle*>(element)->constraints;
    case NodeType::Ellipse:
      return &static_cast<const Ellipse*>(element)->constraints;
    case NodeType::Polystar:
      return &static_cast<const Polystar*>(element)->constraints;
    case NodeType::Path:
      return &static_cast<const Path*>(element)->constraints;
    case NodeType::Text:
      return &static_cast<const Text*>(element)->constraints;
    case NodeType::TextBox:
      return &static_cast<const TextBox*>(element)->constraints;
    case NodeType::Group:
      return &static_cast<const Group*>(element)->constraints;
    default:
      return nullptr;
  }
}

// Applies horizontal constraint to a stretchable element, modifying its size and position.
static void ApplyHorizontalStretch(Element* element, float newWidth, float newCenterX) {
  switch (element->nodeType()) {
    case NodeType::Rectangle: {
      auto* rect = static_cast<Rectangle*>(element);
      rect->size.width = newWidth;
      rect->position.x = newCenterX;
      break;
    }
    case NodeType::Ellipse: {
      auto* ellipse = static_cast<Ellipse*>(element);
      ellipse->size.width = newWidth;
      ellipse->position.x = newCenterX;
      break;
    }
    case NodeType::TextBox: {
      auto* textBox = static_cast<TextBox*>(element);
      textBox->size.width = newWidth;
      textBox->position.x = newCenterX - newWidth * 0.5f;
      break;
    }
    default:
      break;
  }
}

// Applies vertical constraint to a stretchable element, modifying its size and position.
static void ApplyVerticalStretch(Element* element, float newHeight, float newCenterY) {
  switch (element->nodeType()) {
    case NodeType::Rectangle: {
      auto* rect = static_cast<Rectangle*>(element);
      rect->size.height = newHeight;
      rect->position.y = newCenterY;
      break;
    }
    case NodeType::Ellipse: {
      auto* ellipse = static_cast<Ellipse*>(element);
      ellipse->size.height = newHeight;
      ellipse->position.y = newCenterY;
      break;
    }
    case NodeType::TextBox: {
      auto* textBox = static_cast<TextBox*>(element);
      textBox->size.height = newHeight;
      textBox->position.y = newCenterY - newHeight * 0.5f;
      break;
    }
    default:
      break;
  }
}

// Applies a horizontal translation to an element's position.
static void TranslateX(Element* element, float tx) {
  switch (element->nodeType()) {
    case NodeType::Rectangle:
      static_cast<Rectangle*>(element)->position.x += tx;
      break;
    case NodeType::Ellipse:
      static_cast<Ellipse*>(element)->position.x += tx;
      break;
    case NodeType::Polystar:
      static_cast<Polystar*>(element)->position.x += tx;
      break;
    case NodeType::Path:
      static_cast<Path*>(element)->position.x += tx;
      break;
    case NodeType::Text:
      static_cast<Text*>(element)->position.x += tx;
      break;
    case NodeType::TextBox:
      static_cast<TextBox*>(element)->position.x += tx;
      break;
    case NodeType::Group:
      static_cast<Group*>(element)->position.x += tx;
      break;
    default:
      break;
  }
}

// Applies a vertical translation to an element's position.
static void TranslateY(Element* element, float ty) {
  switch (element->nodeType()) {
    case NodeType::Rectangle:
      static_cast<Rectangle*>(element)->position.y += ty;
      break;
    case NodeType::Ellipse:
      static_cast<Ellipse*>(element)->position.y += ty;
      break;
    case NodeType::Polystar:
      static_cast<Polystar*>(element)->position.y += ty;
      break;
    case NodeType::Path:
      static_cast<Path*>(element)->position.y += ty;
      break;
    case NodeType::Text:
      static_cast<Text*>(element)->position.y += ty;
      break;
    case NodeType::TextBox:
      static_cast<TextBox*>(element)->position.y += ty;
      break;
    case NodeType::Group:
      static_cast<Group*>(element)->position.y += ty;
      break;
    default:
      break;
  }
}

// Updates the layout width/height of a Group element derived from constraints.
static void UpdateGroupLayoutSize(Group* group, std::optional<float> derivedWidth,
                                  std::optional<float> derivedHeight) {
  if (derivedWidth.has_value() && !group->width.has_value()) {
    group->width = derivedWidth;
  }
  if (derivedHeight.has_value() && !group->height.has_value()) {
    group->height = derivedHeight;
  }
}

// Applies constraint layout to a single element within a container of the given dimensions.
static void ApplyConstraintToElement(Element* element, float containerWidth,
                                     float containerHeight) {
  auto* constraints = GetConstraints(element);
  if (constraints == nullptr || !constraints->hasAny()) {
    return;
  }
  auto bounds = GetContentBounds(element);
  auto type = element->nodeType();
  bool stretchable = IsStretchable(type);

  // --- Horizontal axis ---
  bool hasLeft = constraints->left.has_value();
  bool hasRight = constraints->right.has_value();
  bool hasCenterX = constraints->centerX.has_value();

  if (hasLeft && hasRight) {
    float left = *constraints->left;
    float right = *constraints->right;
    float areaWidth = containerWidth - left - right;
    if (stretchable) {
      // Stretch to fill the target area.
      float newCenterX = left + areaWidth * 0.5f;
      ApplyHorizontalStretch(element, areaWidth, newCenterX);
    } else {
      // Degrade to centering within the target area.
      float tx = left + (areaWidth - bounds.width) * 0.5f - bounds.x;
      TranslateX(element, tx);
    }
    // Derive Group layout width from constraints.
    if (type == NodeType::Group) {
      UpdateGroupLayoutSize(static_cast<Group*>(element), areaWidth, std::nullopt);
    }
  } else if (hasLeft) {
    float tx = *constraints->left - bounds.x;
    TranslateX(element, tx);
  } else if (hasRight) {
    float tx = (containerWidth - *constraints->right) - (bounds.x + bounds.width);
    TranslateX(element, tx);
  } else if (hasCenterX) {
    float boundsCenter = bounds.x + bounds.width * 0.5f;
    float tx = (containerWidth * 0.5f + *constraints->centerX) - boundsCenter;
    TranslateX(element, tx);
  }

  // --- Vertical axis ---
  bool hasTop = constraints->top.has_value();
  bool hasBottom = constraints->bottom.has_value();
  bool hasCenterY = constraints->centerY.has_value();

  if (hasTop && hasBottom) {
    float top = *constraints->top;
    float bottom = *constraints->bottom;
    float areaHeight = containerHeight - top - bottom;
    if (stretchable) {
      // Stretch to fill the target area.
      float newCenterY = top + areaHeight * 0.5f;
      ApplyVerticalStretch(element, areaHeight, newCenterY);
    } else {
      // Degrade to centering within the target area.
      float ty = top + (areaHeight - bounds.height) * 0.5f - bounds.y;
      TranslateY(element, ty);
    }
    // Derive Group layout height from constraints.
    if (type == NodeType::Group) {
      UpdateGroupLayoutSize(static_cast<Group*>(element), std::nullopt, areaHeight);
    }
  } else if (hasTop) {
    float ty = *constraints->top - bounds.y;
    TranslateY(element, ty);
  } else if (hasBottom) {
    float ty = (containerHeight - *constraints->bottom) - (bounds.y + bounds.height);
    TranslateY(element, ty);
  } else if (hasCenterY) {
    float boundsCenter = bounds.y + bounds.height * 0.5f;
    float ty = (containerHeight * 0.5f + *constraints->centerY) - boundsCenter;
    TranslateY(element, ty);
  }
}

// Forward declarations for recursive processing.
static void ProcessConstraintLayout(const std::vector<Element*>& elements, float containerWidth,
                                    float containerHeight);
static std::pair<float, float> MeasureLayer(const Layer* layer);
static void LayoutLayer(Layer* layer);

// Applies constraint layout to all elements within a container. Group elements with layout
// dimensions are processed recursively.
static void ProcessConstraintLayout(const std::vector<Element*>& elements, float containerWidth,
                                    float containerHeight) {
  for (auto* element : elements) {
    ApplyConstraintToElement(element, containerWidth, containerHeight);

    // Recursively process Group children if the Group has layout dimensions.
    if (element->nodeType() == NodeType::Group) {
      auto* group = static_cast<Group*>(element);
      if (group->width.has_value() || group->height.has_value()) {
        float groupW = group->width.value_or(0);
        float groupH = group->height.value_or(0);
        ProcessConstraintLayout(group->elements, groupW, groupH);
      }
    }
  }
}

// Returns the measured bounds of an element for bottom-up size calculation.
// Constraints affect measurement:
// - Opposite-edge constraints (left+right or top+bottom) reset position to 0 (the constraint
//   area becomes the reference frame). Stretchable types also reset size to 0 (they follow
//   container). Non-stretchable types keep their original size.
// - Single-edge constraints (left, right, top, or bottom) reset position to 0 but keep size.
// - Center constraints (centerX, centerY) don't affect size, position reset not needed.
static Rect GetMeasuredBounds(const Element* element) {
  auto bounds = GetContentBounds(element);
  if (bounds.isEmpty()) {
    return {};
  }
  auto* constraints = GetConstraints(element);
  if (constraints == nullptr) {
    return bounds;
  }
  bool stretchable = IsStretchable(element->nodeType());
  bool hasLeft = constraints->left.has_value();
  bool hasRight = constraints->right.has_value();
  bool hasTop = constraints->top.has_value();
  bool hasBottom = constraints->bottom.has_value();

  // --- Horizontal axis ---
  if (hasLeft && hasRight) {
    // Opposite-edge constraint: position is determined by the constraint area.
    bounds.x = 0;
    if (stretchable) {
      // Stretchable elements follow the container width.
      bounds.width = 0;
    }
    // Non-stretchable elements keep their original width, centered in the constraint area.
  } else if (hasLeft || hasRight) {
    // Single-edge constraint: position is anchored, but reset to 0 for measurement.
    bounds.x = 0;
  }

  // --- Vertical axis ---
  if (hasTop && hasBottom) {
    // Opposite-edge constraint: position is determined by the constraint area.
    bounds.y = 0;
    if (stretchable) {
      // Stretchable elements follow the container height.
      bounds.height = 0;
    }
    // Non-stretchable elements keep their original height, centered in the constraint area.
  } else if (hasTop || hasBottom) {
    // Single-edge constraint: position is anchored, but reset to 0 for measurement.
    bounds.y = 0;
  }

  return bounds;
}

// Computes the measured content size of a Layer (width and height) for bottom-up measurement.
// Considers constraints on content elements: stretchable elements with opposite-edge constraints
// contribute 0 on the constrained axis (they follow the container).
static std::pair<float, float> MeasureLayerContents(const Layer* layer) {
  float measuredWidth = 0;
  float measuredHeight = 0;

  for (auto* element : layer->contents) {
    auto bounds = GetMeasuredBounds(element);
    if (bounds.isEmpty()) {
      continue;
    }
    measuredWidth = std::max(measuredWidth, bounds.x + bounds.width);
    measuredHeight = std::max(measuredHeight, bounds.y + bounds.height);
  }

  // Also consider child Layers (recursively).
  for (auto* child : layer->children) {
    auto childMeasured = MeasureLayer(child);
    float cx = child->x + childMeasured.first;
    float cy = child->y + childMeasured.second;
    measuredWidth = std::max(measuredWidth, cx);
    measuredHeight = std::max(measuredHeight, cy);
  }

  return {measuredWidth > 0 ? measuredWidth : 0, measuredHeight > 0 ? measuredHeight : 0};
}

// Clamps a value within min/max constraints. Used for flexible child sizing.
static float ClampSize(float value, std::optional<float> minVal, std::optional<float> maxVal) {
  if (minVal.has_value()) {
    value = std::max(value, *minVal);
  }
  if (maxVal.has_value()) {
    value = std::min(value, *maxVal);
  }
  return value;
}

// Represents a row/column of children in wrap mode.
struct WrapLine {
  std::vector<size_t> childIndices = {};
  float mainAxisUsed = 0;
  float crossAxisSize = 0;
};

// Performs container layout on a parent Layer that has `layout` set. Arranges child Layers
// along the main axis, distributes flexible sizes, and applies alignment/arrangement.
// The result is that each child Layer gets assigned x, y, width, and height based on the
// container's layout parameters and the child's own size properties and constraints.
static void PerformContainerLayout(Layer* parent) {
  auto direction = *parent->layout;
  bool horizontal = (direction == LayoutDirection::Horizontal);

  float containerMainSize = horizontal ? parent->width.value_or(0) : parent->height.value_or(0);
  float containerCrossSize = horizontal ? parent->height.value_or(0) : parent->width.value_or(0);

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

  // Measure all children once upfront to avoid repeated measurement.
  std::vector<std::pair<float, float>> childMeasuredSizes(childCount);
  for (size_t i = 0; i < childCount; i++) {
    childMeasuredSizes[i] = MeasureLayer(children[i]);
  }

  // Determine which children have fixed vs flexible main-axis sizes.
  // A child with explicit width/height on the main axis is "fixed"; otherwise "flexible".
  // Flexible children get equal shares of available space (no intrinsic basis).
  std::vector<bool> isFixed(childCount, false);
  std::vector<float> childMainSizes(childCount, 0);
  std::vector<float> childCrossSizes(childCount, 0);

  for (size_t i = 0; i < childCount; i++) {
    auto* child = children[i];
    bool hasMainSize = horizontal ? child->width.has_value() : child->height.has_value();
    if (hasMainSize) {
      isFixed[i] = true;
      childMainSizes[i] = horizontal ? *child->width : *child->height;
    }
    // Flexible children: childMainSizes[i] stays 0, will be set during distribution.

    bool hasCrossSize = horizontal ? child->height.has_value() : child->width.has_value();
    if (hasCrossSize) {
      childCrossSizes[i] = horizontal ? *child->height : *child->width;
    } else {
      // Use pre-measured cross size to avoid redundant MeasureLayer calls.
      childCrossSizes[i] = horizontal ? childMeasuredSizes[i].second : childMeasuredSizes[i].first;
    }
  }

  // Split children into wrap lines if layoutWrap is enabled.
  std::vector<WrapLine> lines = {};

  if (parent->layoutWrap) {
    WrapLine currentLine = {};
    for (size_t i = 0; i < childCount; i++) {
      float itemMain = childMainSizes[i];
      float neededMain = currentLine.mainAxisUsed;
      if (!currentLine.childIndices.empty()) {
        neededMain += parent->gap;
      }
      neededMain += itemMain;

      if (!currentLine.childIndices.empty() && neededMain > availableMain) {
        lines.push_back(std::move(currentLine));
        currentLine = {};
      }

      if (!currentLine.childIndices.empty()) {
        currentLine.mainAxisUsed += parent->gap;
      }
      currentLine.childIndices.push_back(i);
      currentLine.mainAxisUsed += itemMain;
      currentLine.crossAxisSize = std::max(currentLine.crossAxisSize, childCrossSizes[i]);
    }
    if (!currentLine.childIndices.empty()) {
      lines.push_back(std::move(currentLine));
    }
  } else {
    WrapLine singleLine = {};
    for (size_t i = 0; i < childCount; i++) {
      singleLine.childIndices.push_back(i);
    }
    lines.push_back(std::move(singleLine));
  }

  // Process each line: distribute flexible sizes, then position children.
  float crossOffset = paddingCrossStart;

  for (auto& line : lines) {
    auto& indices = line.childIndices;
    size_t lineChildCount = indices.size();
    float totalGap = parent->gap * static_cast<float>(lineChildCount > 1 ? lineChildCount - 1 : 0);

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

    // Distribute available space equally among flexible children using max-min fairness.
    float totalFlexSpace = availableMain - fixedTotal - totalGap;
    if (!flexIndices.empty() && totalFlexSpace > 0) {
      std::vector<size_t> uncapped = flexIndices;
      float spaceLeft = totalFlexSpace;

      // Iteratively assign equal shares, respecting min/max constraints.
      while (!uncapped.empty() && spaceLeft > 0.01f) {
        float share = spaceLeft / static_cast<float>(uncapped.size());
        std::vector<size_t> nextUncapped = {};
        float usedThisRound = 0;
        for (auto idx : uncapped) {
          auto* child = children[idx];
          float clamped = horizontal ? ClampSize(share, child->minWidth, child->maxWidth)
                                     : ClampSize(share, child->minHeight, child->maxHeight);
          if (clamped != share) {
            childMainSizes[idx] = clamped;
            usedThisRound += clamped;
          } else {
            childMainSizes[idx] = share;
            usedThisRound += share;
            nextUncapped.push_back(idx);
          }
        }
        spaceLeft -= usedThisRound;
        uncapped = std::move(nextUncapped);
      }
    } else if (!flexIndices.empty()) {
      // No available space: apply min constraints only.
      for (auto idx : flexIndices) {
        auto* child = children[idx];
        if (horizontal && child->minWidth.has_value()) {
          childMainSizes[idx] = *child->minWidth;
        } else if (!horizontal && child->minHeight.has_value()) {
          childMainSizes[idx] = *child->minHeight;
        }
      }
    }

    // Assign computed sizes back to children (set width/height if they were flexible).
    for (auto idx : indices) {
      auto* child = children[idx];
      if (!isFixed[idx]) {
        if (horizontal) {
          child->width = childMainSizes[idx];
        } else {
          child->height = childMainSizes[idx];
        }
      }
    }

    // Calculate the actual total main size used by this line.
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
        if (lineChildCount > 1) {
          float totalChildMain = totalMain - totalGap;
          float spaceToDistribute = availableMain - totalChildMain;
          extraGap = spaceToDistribute / static_cast<float>(lineChildCount - 1);
        }
        break;
    }

    // Compute the max cross-axis size for this line (used for wrap mode line advancement).
    float lineCrossSize = 0;
    for (auto idx : indices) {
      lineCrossSize = std::max(lineCrossSize, childCrossSizes[idx]);
    }

    // Compute the cross-axis reference size for alignment. In non-wrap mode, use the container's
    // available cross-axis space. In wrap mode, use the line's max cross-axis size.
    float alignCrossSize = parent->layoutWrap
                               ? lineCrossSize
                               : (containerCrossSize - paddingCrossStart - paddingCrossEnd);

    // Position each child along the main axis and cross axis.
    for (auto idx : indices) {
      auto* child = children[idx];

      // Main axis position.
      float mainPos = mainOffset;

      // Cross axis position with alignment.
      float childCross = childCrossSizes[idx];
      float crossPos = crossOffset;
      switch (parent->alignment) {
        case Alignment::Start:
          break;
        case Alignment::Center:
          crossPos += (alignCrossSize - childCross) * 0.5f;
          break;
        case Alignment::End:
          crossPos += (alignCrossSize - childCross);
          break;
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

    crossOffset += lineCrossSize + parent->gap;
  }
}

// Measures the desired size of a Layer by examining its contents and children (bottom-up).
// Returns {width, height}. For each axis: if the Layer has an explicit dimension, that is used;
// otherwise, the size is derived from content bounds and child Layers.
static std::pair<float, float> MeasureLayer(const Layer* layer) {
  float measuredWidth = 0;
  float measuredHeight = 0;

  if (layer->width.has_value()) {
    measuredWidth = *layer->width;
  }
  if (layer->height.has_value()) {
    measuredHeight = *layer->height;
  }

  // If both dimensions are explicit, no measurement needed.
  if (layer->width.has_value() && layer->height.has_value()) {
    return {measuredWidth, measuredHeight};
  }

  if (layer->layout.has_value() && !layer->children.empty()) {
    // Layout container: measure from child Layers along the layout axis.
    bool horizontal = (*layer->layout == LayoutDirection::Horizontal);
    size_t childCount = layer->children.size();

    float paddingMainStart = horizontal ? layer->padding.left : layer->padding.top;
    float paddingMainEnd = horizontal ? layer->padding.right : layer->padding.bottom;
    float paddingCrossStart = horizontal ? layer->padding.top : layer->padding.left;
    float paddingCrossEnd = horizontal ? layer->padding.bottom : layer->padding.right;

    bool needsMainAxis = !(horizontal ? layer->width.has_value() : layer->height.has_value());
    bool needsCrossAxis = !(horizontal ? layer->height.has_value() : layer->width.has_value());

    // Measure children once, reuse for both axes.
    if (needsMainAxis || needsCrossAxis) {
      float totalMain = 0;
      float maxCross = 0;
      for (auto* child : layer->children) {
        auto childMeasured = MeasureLayer(child);
        if (needsMainAxis) {
          totalMain += horizontal ? childMeasured.first : childMeasured.second;
        }
        if (needsCrossAxis) {
          float childCross = horizontal ? childMeasured.second : childMeasured.first;
          maxCross = std::max(maxCross, childCross);
        }
      }
      if (needsMainAxis) {
        float totalGap = layer->gap * static_cast<float>(childCount > 1 ? childCount - 1 : 0);
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
    auto contentSize = MeasureLayerContents(layer);
    if (!layer->width.has_value()) {
      measuredWidth = contentSize.first;
    }
    if (!layer->height.has_value()) {
      measuredHeight = contentSize.second;
    }
  }

  // Apply min/max constraints to measured dimensions (applies to all Layers, not just layout).
  if (!layer->width.has_value()) {
    measuredWidth = ClampSize(measuredWidth, layer->minWidth, layer->maxWidth);
  }
  if (!layer->height.has_value()) {
    measuredHeight = ClampSize(measuredHeight, layer->minHeight, layer->maxHeight);
  }

  return {measuredWidth, measuredHeight};
}

// Layout phase (top-down): determines final sizes and positions for all Layers and their contents.
// Measurement happens as needed when dimensions are not explicit. Each axis is measured
// independently to avoid redundant computation.
static void LayoutLayer(Layer* layer) {
  // Step 1: If this Layer has layout but missing dimensions, use measured sizes.
  // The parent's PerformContainerLayout may have already assigned sizes for flexible children.
  // This handles the case where this Layer is a top-level container without explicit dimensions.
  if (layer->layout.has_value() && !layer->children.empty()) {
    if (!layer->width.has_value() || !layer->height.has_value()) {
      auto measured = MeasureLayer(layer);
      if (!layer->width.has_value()) {
        layer->width = measured.first;
      }
      if (!layer->height.has_value()) {
        layer->height = measured.second;
      }
    }
  }

  // Step 2: Container layout — arrange child Layers and assign flexible sizes.
  if (layer->layout.has_value() && !layer->children.empty()) {
    PerformContainerLayout(layer);
  }

  // Step 3: For non-layout Layers without explicit dimensions, adopt measured sizes so that
  // constraint layout within contents can work. Skip if dimensions are already set.
  // Also skip if this Layer has no contents and no children (nothing to measure or constrain).
  if (!layer->layout.has_value() && (!layer->width.has_value() || !layer->height.has_value()) &&
      (!layer->contents.empty() || !layer->children.empty())) {
    auto measured = MeasureLayer(layer);
    if (!layer->width.has_value()) {
      layer->width = measured.first;
    }
    if (!layer->height.has_value()) {
      layer->height = measured.second;
    }
  }

  // Step 4: Constraint layout — position elements within this Layer.
  // Skip if there are no contents to constrain.
  if (!layer->contents.empty() && (layer->width.has_value() || layer->height.has_value())) {
    float containerW = layer->width.value_or(0);
    float containerH = layer->height.value_or(0);
    ProcessConstraintLayout(layer->contents, containerW, containerH);
  }

  // Step 5: Recurse into child Layers.
  for (auto* child : layer->children) {
    LayoutLayer(child);
  }
}

void AutoLayout::Apply(PAGXDocument* document) {
  if (document == nullptr) {
    return;
  }
  for (auto* layer : document->layers) {
    LayoutLayer(layer);
  }
}

}  // namespace pagx
