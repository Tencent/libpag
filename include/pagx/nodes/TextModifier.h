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

#include <optional>
#include <vector>
#include "pagx/nodes/Element.h"
#include "pagx/nodes/TextSelector.h"
#include "pagx/types/Point.h"
#include "pagx/types/Color.h"

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
  Point anchor = {};

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
   * The fill color override for selected characters.
   */
  std::optional<Color> fillColor = std::nullopt;

  /**
   * The stroke color override for selected characters.
   */
  std::optional<Color> strokeColor = std::nullopt;

  /**
   * The stroke width override for selected characters.
   */
  std::optional<float> strokeWidth = std::nullopt;

  /**
   * The selectors that determine which characters are affected by this modifier.
   */
  std::vector<TextSelector*> selectors = {};

  NodeType nodeType() const override {
    return NodeType::TextModifier;
  }

 private:
  TextModifier() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
