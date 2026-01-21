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

#include <memory>
#include <string>
#include <vector>
#include "pagx/model/Node.h"
#include "pagx/model/Types.h"
#include "pagx/model/VectorElement.h"
#include "pagx/model/enums/Overflow.h"
#include "pagx/model/enums/SelectorMode.h"
#include "pagx/model/enums/SelectorShape.h"
#include "pagx/model/enums/SelectorUnit.h"
#include "pagx/model/enums/TextAlign.h"
#include "pagx/model/enums/TextPathAlign.h"
#include "pagx/model/enums/VerticalAlign.h"

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

  std::unique_ptr<Node> clone() const override {
    return std::make_unique<RangeSelector>(*this);
  }
};

/**
 * Text modifier.
 */
struct TextModifier : public VectorElement {
  Point anchorPoint = {};
  Point position = {};
  float rotation = 0;
  Point scale = {1, 1};
  float skew = 0;
  float skewAxis = 0;
  float alpha = 1;
  std::string fillColor = {};
  std::string strokeColor = {};
  float strokeWidth = -1;
  std::vector<RangeSelector> rangeSelectors = {};

  NodeType type() const override {
    return NodeType::TextModifier;
  }

  std::unique_ptr<Node> clone() const override {
    return std::make_unique<TextModifier>(*this);
  }
};

/**
 * Text path modifier.
 */
struct TextPath : public VectorElement {
  std::string path = {};
  TextPathAlign textPathAlign = TextPathAlign::Start;
  float firstMargin = 0;
  float lastMargin = 0;
  bool perpendicularToPath = true;
  bool reversed = false;
  bool forceAlignment = false;

  NodeType type() const override {
    return NodeType::TextPath;
  }

  std::unique_ptr<Node> clone() const override {
    return std::make_unique<TextPath>(*this);
  }
};

/**
 * Text layout modifier.
 */
struct TextLayout : public VectorElement {
  float width = 0;
  float height = 0;
  TextAlign textAlign = TextAlign::Left;
  VerticalAlign verticalAlign = VerticalAlign::Top;
  float lineHeight = 1.2f;
  float indent = 0;
  Overflow overflow = Overflow::Clip;

  NodeType type() const override {
    return NodeType::TextLayout;
  }

  std::unique_ptr<Node> clone() const override {
    return std::make_unique<TextLayout>(*this);
  }
};

}  // namespace pagx
