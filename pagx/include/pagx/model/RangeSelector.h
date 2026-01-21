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
#include "pagx/model/TextSelector.h"

namespace pagx {

/**
 * Range selector unit.
 */
enum class SelectorUnit {
  Index,
  Percentage
};

std::string SelectorUnitToString(SelectorUnit unit);
SelectorUnit SelectorUnitFromString(const std::string& str);

/**
 * Range selector shape.
 */
enum class SelectorShape {
  Square,
  RampUp,
  RampDown,
  Triangle,
  Round,
  Smooth
};

std::string SelectorShapeToString(SelectorShape shape);
SelectorShape SelectorShapeFromString(const std::string& str);

/**
 * Range selector combination mode.
 */
enum class SelectorMode {
  Add,
  Subtract,
  Intersect,
  Min,
  Max,
  Difference
};

std::string SelectorModeToString(SelectorMode mode);
SelectorMode SelectorModeFromString(const std::string& str);

/**
 * Range selector for text modifier.
 */
class RangeSelector : public TextSelector {
 public:
  float start = 0;
  float end = 1;
  float offset = 0;
  SelectorUnit unit = SelectorUnit::Percentage;
  SelectorShape shape = SelectorShape::Square;
  float easeIn = 0;
  float easeOut = 0;
  SelectorMode mode = SelectorMode::Add;
  float weight = 1;
  bool randomizeOrder = false;
  int randomSeed = 0;

  TextSelectorType type() const override {
    return TextSelectorType::RangeSelector;
  }
};

}  // namespace pagx
