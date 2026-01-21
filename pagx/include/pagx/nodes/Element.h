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

#include "pagx/nodes/Node.h"

namespace pagx {

/**
 * Element types that can be placed in Layer.contents or Group.elements.
 */
enum class ElementType {
  // Geometry elements
  Rectangle,
  Ellipse,
  Polystar,
  Path,
  TextSpan,

  // Painters
  Fill,
  Stroke,

  // Path modifiers
  TrimPath,
  RoundCorner,
  MergePath,

  // Text animators
  TextModifier,
  TextPath,
  TextLayout,

  // Containers
  Group,
  Repeater
};

/**
 * Returns the string name of an element type.
 */
const char* ElementTypeName(ElementType type);

/**
 * Base class for elements that can be placed in Layer.contents or Group.elements.
 */
class Element : public Node {
 public:
  /**
   * Returns the element type of this element.
   */
  virtual ElementType elementType() const = 0;

 protected:
  Element() = default;
};

}  // namespace pagx
