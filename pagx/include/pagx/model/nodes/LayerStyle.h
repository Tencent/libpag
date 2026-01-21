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
#include "pagx/model/nodes/Node.h"
#include "pagx/model/types/Types.h"
#include "pagx/model/types/enums/BlendMode.h"
#include "pagx/model/types/enums/TileMode.h"

namespace pagx {

/**
 * Base class for layer style nodes.
 */
class LayerStyle : public Node {
 public:
  BlendMode blendMode = BlendMode::Normal;
};

/**
 * Drop shadow style.
 */
struct DropShadowStyle : public LayerStyle {
  float offsetX = 0;
  float offsetY = 0;
  float blurrinessX = 0;
  float blurrinessY = 0;
  Color color = {};
  bool showBehindLayer = true;

  NodeType type() const override {
    return NodeType::DropShadowStyle;
  }

  std::unique_ptr<Node> clone() const override {
    return std::make_unique<DropShadowStyle>(*this);
  }
};

/**
 * Inner shadow style.
 */
struct InnerShadowStyle : public LayerStyle {
  float offsetX = 0;
  float offsetY = 0;
  float blurrinessX = 0;
  float blurrinessY = 0;
  Color color = {};

  NodeType type() const override {
    return NodeType::InnerShadowStyle;
  }

  std::unique_ptr<Node> clone() const override {
    return std::make_unique<InnerShadowStyle>(*this);
  }
};

/**
 * Background blur style.
 */
struct BackgroundBlurStyle : public LayerStyle {
  float blurrinessX = 0;
  float blurrinessY = 0;
  TileMode tileMode = TileMode::Mirror;

  NodeType type() const override {
    return NodeType::BackgroundBlurStyle;
  }

  std::unique_ptr<Node> clone() const override {
    return std::make_unique<BackgroundBlurStyle>(*this);
  }
};

}  // namespace pagx
