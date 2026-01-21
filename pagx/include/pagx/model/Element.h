/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "pagx/model/NodeType.h"

namespace pagx {

/**
 * ElementType enumerates all types of elements that can be placed in Layer.contents or
 * Group.elements.
 */
enum class ElementType {
  /**
   * A rectangle shape with optional rounded corners.
   */
  Rectangle,
  /**
   * An ellipse shape defined by center point and size.
   */
  Ellipse,
  /**
   * A polygon or star shape with configurable points and roundness.
   */
  Polystar,
  /**
   * A freeform path shape defined by a PathData.
   */
  Path,
  /**
   * A text span that generates glyph paths for rendering.
   */
  TextSpan,
  /**
   * A fill painter that fills shapes with a color or gradient.
   */
  Fill,
  /**
   * A stroke painter that outlines shapes with a color or gradient.
   */
  Stroke,
  /**
   * A path modifier that trims paths to a specified range.
   */
  TrimPath,
  /**
   * A path modifier that rounds the corners of shapes.
   */
  RoundCorner,
  /**
   * A path modifier that merges multiple paths using boolean operations.
   */
  MergePath,
  /**
   * A text animator that modifies text appearance with range-based transformations.
   */
  TextModifier,
  /**
   * A text animator that places text along a path.
   */
  TextPath,
  /**
   * A text animator that controls text layout within a bounding box.
   */
  TextLayout,
  /**
   * A container that groups multiple elements with its own transform.
   */
  Group,
  /**
   * A modifier that creates multiple copies of preceding elements.
   */
  Repeater
};

/**
 * Returns the string name of an element type.
 */
const char* ElementTypeName(ElementType type);

/**
 * Element is the base class for all elements in a shape layer. It includes shapes (Rectangle,
 * Ellipse, Polystar, Path, TextSpan), painters (Fill, Stroke), modifiers (TrimPath, RoundCorner,
 * MergePath), text elements (TextModifier, TextPath, TextLayout), and containers (Group, Repeater).
 */
class Element {
 public:
  virtual ~Element() = default;

  /**
   * Returns the element type of this element.
   */
  virtual ElementType elementType() const = 0;

  /**
   * Returns the unified node type of this element.
   */
  virtual NodeType type() const = 0;

 protected:
  Element() = default;
};

}  // namespace pagx
