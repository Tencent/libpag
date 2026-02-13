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

#include "pagx/nodes/TextSelector.h"
#include "pagx/types/SelectorTypes.h"

namespace pagx {

/**
 * A range selector that defines which characters in a text are affected by a text modifier. It
 * provides flexible control over character selection through start/end positions, shapes, and
 * randomization.
 */
class RangeSelector : public TextSelector {
 public:
  /**
   * The starting position of the selection range, in units defined by the unit property.
   * The default value is 0.
   */
  float start = 0;

  /**
   * The ending position of the selection range, in units defined by the unit property. The default
   * value is 1.
   */
  float end = 1;

  /**
   * The offset to shift the selection range. The default value is 0.
   */
  float offset = 0;

  /**
   * The unit used for start, end, and offset values. The default value is Percentage.
   */
  SelectorUnit unit = SelectorUnit::Percentage;

  /**
   * The shape of the selection falloff curve. The default value is Square.
   */
  SelectorShape shape = SelectorShape::Square;

  /**
   * The ease-in amount for the selection shape, ranging from 0 to 1. The default value is 0.
   */
  float easeIn = 0;

  /**
   * The ease-out amount for the selection shape, ranging from 0 to 1. The default value is 0.
   */
  float easeOut = 0;

  /**
   * The mode for combining multiple selectors. The default value is Add.
   */
  SelectorMode mode = SelectorMode::Add;

  /**
   * The weight of this selector's influence, ranging from 0 to 1. The default value is 1.
   */
  float weight = 1;

  /**
   * Whether to randomize the order of character selection. The default value is false.
   */
  bool randomOrder = false;

  /**
   * The seed for random order generation. The default value is 0.
   */
  int randomSeed = 0;

  NodeType nodeType() const override {
    return NodeType::RangeSelector;
  }

 private:
  RangeSelector() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
