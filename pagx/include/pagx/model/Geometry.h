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
#include "pagx/PathData.h"
#include "pagx/model/Types.h"
#include "pagx/model/VectorElement.h"
#include "pagx/model/enums/FontStyle.h"
#include "pagx/model/enums/PolystarType.h"
#include "pagx/model/enums/TextAnchor.h"

namespace pagx {

/**
 * A rectangle shape.
 */
struct Rectangle : public VectorElement {
  Point center = {};
  Size size = {100, 100};
  float roundness = 0;
  bool reversed = false;

  NodeType type() const override {
    return NodeType::Rectangle;
  }

  std::unique_ptr<Node> clone() const override {
    return std::make_unique<Rectangle>(*this);
  }
};

/**
 * An ellipse shape.
 */
struct Ellipse : public VectorElement {
  Point center = {};
  Size size = {100, 100};
  bool reversed = false;

  NodeType type() const override {
    return NodeType::Ellipse;
  }

  std::unique_ptr<Node> clone() const override {
    return std::make_unique<Ellipse>(*this);
  }
};

/**
 * A polygon or star shape.
 */
struct Polystar : public VectorElement {
  Point center = {};
  PolystarType polystarType = PolystarType::Star;
  float pointCount = 5;
  float outerRadius = 100;
  float innerRadius = 50;
  float rotation = 0;
  float outerRoundness = 0;
  float innerRoundness = 0;
  bool reversed = false;

  NodeType type() const override {
    return NodeType::Polystar;
  }

  std::unique_ptr<Node> clone() const override {
    return std::make_unique<Polystar>(*this);
  }
};

/**
 * A path shape.
 */
struct Path : public VectorElement {
  PathData data = {};
  bool reversed = false;

  NodeType type() const override {
    return NodeType::Path;
  }

  std::unique_ptr<Node> clone() const override {
    return std::make_unique<Path>(*this);
  }
};

/**
 * A text span.
 */
struct TextSpan : public VectorElement {
  float x = 0;
  float y = 0;
  std::string font = {};
  float fontSize = 12;
  int fontWeight = 400;
  FontStyle fontStyle = FontStyle::Normal;
  float tracking = 0;
  float baselineShift = 0;
  TextAnchor textAnchor = TextAnchor::Start;
  std::string text = {};

  NodeType type() const override {
    return NodeType::TextSpan;
  }

  std::unique_ptr<Node> clone() const override {
    return std::make_unique<TextSpan>(*this);
  }
};

}  // namespace pagx
