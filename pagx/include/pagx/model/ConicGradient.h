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
#include <vector>
#include "pagx/model/ColorSource.h"
#include "pagx/model/ColorStop.h"
#include "pagx/model/types/Types.h"

namespace pagx {

/**
 * A conic (sweep) gradient.
 */
class ConicGradient : public ColorSource {
 public:
  std::string id = {};
  Point center = {};
  float startAngle = 0;
  float endAngle = 360;
  Matrix matrix = {};
  std::vector<ColorStop> colorStops = {};

  ColorSourceType colorSourceType() const override {
    return ColorSourceType::ConicGradient;
  }

  ResourceType resourceType() const override {
    return ResourceType::ConicGradient;
  }

  const std::string& resourceId() const override {
    return id;
  }

  NodeType type() const override {
    return NodeType::ConicGradient;
  }
};

}  // namespace pagx
