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

#include <array>
#include <memory>
#include "pagx/model/nodes/Node.h"
#include "pagx/model/types/Types.h"
#include "pagx/model/types/enums/BlendMode.h"
#include "pagx/model/types/enums/TileMode.h"

namespace pagx {

/**
 * Base class for layer filter nodes.
 */
class LayerFilter : public Node {};

/**
 * Blur filter.
 */
struct BlurFilter : public LayerFilter {
  float blurrinessX = 0;
  float blurrinessY = 0;
  TileMode tileMode = TileMode::Decal;

  NodeType type() const override {
    return NodeType::BlurFilter;
  }

  std::unique_ptr<Node> clone() const override {
    return std::make_unique<BlurFilter>(*this);
  }
};

/**
 * Drop shadow filter.
 */
struct DropShadowFilter : public LayerFilter {
  float offsetX = 0;
  float offsetY = 0;
  float blurrinessX = 0;
  float blurrinessY = 0;
  Color color = {};
  bool shadowOnly = false;

  NodeType type() const override {
    return NodeType::DropShadowFilter;
  }

  std::unique_ptr<Node> clone() const override {
    return std::make_unique<DropShadowFilter>(*this);
  }
};

/**
 * Inner shadow filter.
 */
struct InnerShadowFilter : public LayerFilter {
  float offsetX = 0;
  float offsetY = 0;
  float blurrinessX = 0;
  float blurrinessY = 0;
  Color color = {};
  bool shadowOnly = false;

  NodeType type() const override {
    return NodeType::InnerShadowFilter;
  }

  std::unique_ptr<Node> clone() const override {
    return std::make_unique<InnerShadowFilter>(*this);
  }
};

/**
 * Blend filter.
 */
struct BlendFilter : public LayerFilter {
  Color color = {};
  BlendMode filterBlendMode = BlendMode::Normal;

  NodeType type() const override {
    return NodeType::BlendFilter;
  }

  std::unique_ptr<Node> clone() const override {
    return std::make_unique<BlendFilter>(*this);
  }
};

/**
 * Color matrix filter.
 */
struct ColorMatrixFilter : public LayerFilter {
  std::array<float, 20> matrix = {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0};

  NodeType type() const override {
    return NodeType::ColorMatrixFilter;
  }

  std::unique_ptr<Node> clone() const override {
    return std::make_unique<ColorMatrixFilter>(*this);
  }
};

}  // namespace pagx
