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

#pragma once

#include <cmath>
#include <vector>
#include "pagx/types/Rect.h"

namespace pagx {

class Element;
class LayoutContext;

class LayoutNode {
 public:
  // Constraint attributes (user input, not modified during layout).
  float left = NAN;
  float right = NAN;
  float top = NAN;
  float bottom = NAN;
  float centerX = NAN;
  float centerY = NAN;

  virtual ~LayoutNode() = default;

  /** Returns true if any constraint attribute is set. */
  bool hasConstraints() const;

  /** Returns the horizontal extent contributed by constraints for parent measurement. */
  float constraintExtentX() const;

  /** Returns the vertical extent contributed by constraints for parent measurement. */
  float constraintExtentY() const;

  /**
   * Phase 1 (bottom-up): measures this node. Container nodes override to recurse children first,
   * then call LayoutNode::updateSize. Leaf nodes use the base implementation directly.
   */
  virtual void updateSize(const LayoutContext& context);

  /**
   * Writes self rendering attributes and actualWidth/actualHeight. Does not touch children.
   */
  virtual void setLayoutSize(const LayoutContext& context, float width, float height);

  /** Writes self position. */
  virtual void setLayoutPosition(const LayoutContext&, float, float) {
  }

  /**
   * Lays out children using actualWidth/actualHeight as container size.
   * Only container nodes (Group/TextBox/Layer) override this.
   */
  virtual void updateLayout(const LayoutContext&) {
  }

  /**
   * Constraint layout algorithm: for each node computes target size from constraints,
   * calls setLayoutSize + setLayoutPosition + updateLayout.
   */
  static void PerformConstraintLayout(const std::vector<LayoutNode*>& nodes, float containerW,
                                      float containerH, const LayoutContext& context);

  /** Casts an Element to LayoutNode if the element participates in layout. Returns nullptr
   * otherwise. */
  static LayoutNode* AsLayoutNode(Element* element);

  /** Computes position from container size, actual size, and constraints. */
  static Rect CalculateConstrainedPosition(float containerW, float containerH, float actualW,
                                           float actualH, const LayoutNode& node);

  /**
   * Computes a uniform scale factor to fit content into target size. Only non-NAN axes contribute.
   * When both axes have targets, returns the smaller scale.
   */
  static float ComputeUniformScale(float contentW, float contentH, float targetW, float targetH);

  /**
   * Collects LayoutNode pointers from a list of elements. Skips elements that don't participate
   * in layout. When skipText is true, Text elements are excluded (used by TextBox).
   */
  static std::vector<LayoutNode*> CollectLayoutNodes(const std::vector<Element*>& elements,
                                                     bool skipText);

  /**
   * Shared measurement: computes preferred size from child LayoutNodes.
   * When explicitWidth/explicitHeight is not NAN, that axis uses the explicit value directly.
   */
  static void MeasureChildNodes(const std::vector<Element*>& elements, float explicitWidth,
                                float explicitHeight, float& outWidth, float& outHeight);

 protected:
  LayoutNode() = default;

  /** Writes preferredWidth/preferredHeight. Called by updateSize when not yet measured. */
  virtual void onMeasure(const LayoutContext&) {
  }

 private:
  // Preferred position and size (written by onMeasure during updateSize, read-only after that).
  float preferredX = 0;
  float preferredY = 0;
  float preferredWidth = NAN;
  float preferredHeight = NAN;

  // Actual layout size (written by setLayoutSize during layout phase).
  float actualWidth = NAN;
  float actualHeight = NAN;

  friend class Rectangle;
  friend class Ellipse;
  friend class Path;
  friend class Polystar;
  friend class Text;
  friend class TextPath;
  friend class TextBox;
  friend class Group;
  friend class Layer;
};

}  // namespace pagx
