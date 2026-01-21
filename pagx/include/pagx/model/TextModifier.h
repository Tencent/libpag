/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include <string>
#include <vector>
#include "pagx/model/Element.h"
#include "pagx/model/RangeSelector.h"
#include "pagx/model/types/Types.h"

namespace pagx {

/**
 * TextModifier is a text animator that modifies text appearance with range-based transformations.
 * It applies transformations like position, rotation, scale, and color changes to selected ranges
 * of characters.
 */
class TextModifier : public Element {
 public:
  /**
   * The anchor point for transformations.
   */
  Point anchorPoint = {};

  /**
   * The position offset applied to selected characters.
   */
  Point position = {};

  /**
   * The rotation angle in degrees applied to selected characters. The default value is 0.
   */
  float rotation = 0;

  /**
   * The scale factor applied to selected characters. The default value is {1, 1}.
   */
  Point scale = {1, 1};

  /**
   * The skew angle in degrees applied to selected characters. The default value is 0.
   */
  float skew = 0;

  /**
   * The axis angle in degrees for the skew transformation. The default value is 0.
   */
  float skewAxis = 0;

  /**
   * The opacity applied to selected characters, ranging from 0 to 1. The default value is 1.
   */
  float alpha = 1;

  /**
   * The fill color override for selected characters as a hex color string.
   */
  std::string fillColor = {};

  /**
   * The stroke color override for selected characters as a hex color string.
   */
  std::string strokeColor = {};

  /**
   * The stroke width override for selected characters. A value of -1 means no override. The
   * default value is -1.
   */
  float strokeWidth = -1;

  /**
   * The range selectors that determine which characters are affected by this modifier.
   */
  std::vector<RangeSelector> rangeSelectors = {};

  ElementType elementType() const override {
    return ElementType::TextModifier;
  }

  NodeType type() const override {
    return NodeType::TextModifier;
  }
};

}  // namespace pagx
