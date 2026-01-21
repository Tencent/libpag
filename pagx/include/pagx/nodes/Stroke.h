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
#include "pagx/nodes/ColorSource.h"
#include "pagx/nodes/Painter.h"
#include "pagx/types/BlendMode.h"
#include "pagx/types/LineCap.h"
#include "pagx/types/LineJoin.h"
#include "pagx/types/Placement.h"
#include "pagx/types/StrokeAlign.h"

namespace pagx {

/**
 * A stroke painter.
 */
class Stroke : public Painter {
 public:
  std::string color = {};
  std::unique_ptr<ColorSource> colorSource = nullptr;
  float width = 1;
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

  ElementType elementType() const override {
    return ElementType::Stroke;
  }
};

}  // namespace pagx
