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
#include "pagx/model/ColorSource.h"
#include "pagx/model/VectorElement.h"
#include "pagx/model/enums/BlendMode.h"
#include "pagx/model/enums/FillRule.h"
#include "pagx/model/enums/LineCap.h"
#include "pagx/model/enums/LineJoin.h"
#include "pagx/model/enums/Placement.h"
#include "pagx/model/enums/StrokeAlign.h"

namespace pagx {

/**
 * A fill painter.
 * The color can be a simple color string ("#FF0000"), a reference ("#gradientId"),
 * or an inline color source node.
 */
struct Fill : public VectorElement {
  std::string color = {};
  std::unique_ptr<ColorSource> colorSource = nullptr;
  float alpha = 1;
  BlendMode blendMode = BlendMode::Normal;
  FillRule fillRule = FillRule::Winding;
  Placement placement = Placement::Background;

  NodeType type() const override {
    return NodeType::Fill;
  }

  std::unique_ptr<Node> clone() const override {
    auto node = std::make_unique<Fill>();
    node->color = color;
    if (colorSource) {
      node->colorSource.reset(static_cast<ColorSource*>(colorSource->clone().release()));
    }
    node->alpha = alpha;
    node->blendMode = blendMode;
    node->fillRule = fillRule;
    node->placement = placement;
    return node;
  }
};

/**
 * A stroke painter.
 */
struct Stroke : public VectorElement {
  std::string color = {};
  std::unique_ptr<ColorSource> colorSource = nullptr;
  float strokeWidth = 1;
  float alpha = 1;
  BlendMode blendMode = BlendMode::Normal;
  LineCap cap = LineCap::Butt;
  LineJoin join = LineJoin::Miter;
  float miterLimit = 4;
  std::vector<float> dashes = {};
  float dashOffset = 0;
  StrokeAlign align = StrokeAlign::Center;
  Placement placement = Placement::Background;

  NodeType type() const override {
    return NodeType::Stroke;
  }

  std::unique_ptr<Node> clone() const override {
    auto node = std::make_unique<Stroke>();
    node->color = color;
    if (colorSource) {
      node->colorSource.reset(static_cast<ColorSource*>(colorSource->clone().release()));
    }
    node->strokeWidth = strokeWidth;
    node->alpha = alpha;
    node->blendMode = blendMode;
    node->cap = cap;
    node->join = join;
    node->miterLimit = miterLimit;
    node->dashes = dashes;
    node->dashOffset = dashOffset;
    node->align = align;
    node->placement = placement;
    return node;
  }
};

}  // namespace pagx
