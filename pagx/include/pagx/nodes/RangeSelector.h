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

#include "pagx/nodes/Node.h"
#include "pagx/types/SelectorMode.h"
#include "pagx/types/SelectorShape.h"
#include "pagx/types/SelectorUnit.h"

namespace pagx {

/**
 * Range selector for text modifier.
 */
struct RangeSelector : public Node {
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

  NodeType type() const override {
    return NodeType::RangeSelector;
  }
};

}  // namespace pagx
