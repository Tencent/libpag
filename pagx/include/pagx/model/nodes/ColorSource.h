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
#include "pagx/model/nodes/Node.h"
#include "pagx/model/nodes/Resource.h"
#include "pagx/model/types/Types.h"
#include "pagx/model/types/enums/SamplingMode.h"
#include "pagx/model/types/enums/TileMode.h"

namespace pagx {

/**
 * A color stop in a gradient.
 */
struct ColorStop : public Node {
  float offset = 0;
  Color color = {};

  NodeType type() const override {
    return NodeType::ColorStop;
  }

  std::unique_ptr<Node> clone() const override {
    return std::make_unique<ColorStop>(*this);
  }
};

/**
 * Base class for color source nodes.
 * Color sources can be stored as resources (with an id) or inline.
 */
class ColorSource : public Resource {};

/**
 * A solid color.
 */
struct SolidColor : public ColorSource {
  Color color = {};

  NodeType type() const override {
    return NodeType::SolidColor;
  }

  std::unique_ptr<Node> clone() const override {
    return std::make_unique<SolidColor>(*this);
  }
};

/**
 * A linear gradient.
 */
struct LinearGradient : public ColorSource {
  Point startPoint = {};
  Point endPoint = {};
  Matrix matrix = {};
  std::vector<ColorStop> colorStops = {};

  NodeType type() const override {
    return NodeType::LinearGradient;
  }

  std::unique_ptr<Node> clone() const override {
    return std::make_unique<LinearGradient>(*this);
  }
};

/**
 * A radial gradient.
 */
struct RadialGradient : public ColorSource {
  Point center = {};
  float radius = 0;
  Matrix matrix = {};
  std::vector<ColorStop> colorStops = {};

  NodeType type() const override {
    return NodeType::RadialGradient;
  }

  std::unique_ptr<Node> clone() const override {
    return std::make_unique<RadialGradient>(*this);
  }
};

/**
 * A conic (sweep) gradient.
 */
struct ConicGradient : public ColorSource {
  Point center = {};
  float startAngle = 0;
  float endAngle = 360;
  Matrix matrix = {};
  std::vector<ColorStop> colorStops = {};

  NodeType type() const override {
    return NodeType::ConicGradient;
  }

  std::unique_ptr<Node> clone() const override {
    return std::make_unique<ConicGradient>(*this);
  }
};

/**
 * A diamond gradient.
 */
struct DiamondGradient : public ColorSource {
  Point center = {};
  float halfDiagonal = 0;
  Matrix matrix = {};
  std::vector<ColorStop> colorStops = {};

  NodeType type() const override {
    return NodeType::DiamondGradient;
  }

  std::unique_ptr<Node> clone() const override {
    return std::make_unique<DiamondGradient>(*this);
  }
};

/**
 * An image pattern.
 */
struct ImagePattern : public ColorSource {
  std::string image = {};
  TileMode tileModeX = TileMode::Clamp;
  TileMode tileModeY = TileMode::Clamp;
  SamplingMode sampling = SamplingMode::Linear;
  Matrix matrix = {};

  NodeType type() const override {
    return NodeType::ImagePattern;
  }

  std::unique_ptr<Node> clone() const override {
    return std::make_unique<ImagePattern>(*this);
  }
};

}  // namespace pagx
