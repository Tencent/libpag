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
#include "pagx/nodes/Node.h"
#include "pagx/types/BlendMode.h"
#include "pagx/types/FillRule.h"
#include "pagx/types/Placement.h"

namespace pagx {

/**
 * A fill painter.
 * The color can be a simple color string ("#FF0000"), a reference ("#gradientId"),
 * or an inline color source node.
 */
struct Fill : public Node {
  std::string color = {};
  std::unique_ptr<Node> colorSource = nullptr;
  float alpha = 1;
  BlendMode blendMode = BlendMode::Normal;
  FillRule fillRule = FillRule::Winding;
  Placement placement = Placement::Background;

  NodeType type() const override {
    return NodeType::Fill;
  }
};

}  // namespace pagx
