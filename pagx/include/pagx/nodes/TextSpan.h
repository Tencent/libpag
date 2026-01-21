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
#include "pagx/nodes/Geometry.h"
#include "pagx/types/FontStyle.h"

namespace pagx {

/**
 * A text span.
 */
struct TextSpan : public Geometry {
  float x = 0;
  float y = 0;
  std::string font = {};
  float fontSize = 12;
  int fontWeight = 400;
  FontStyle fontStyle = FontStyle::Normal;
  float tracking = 0;
  float baselineShift = 0;
  std::string text = {};

  NodeType type() const override {
    return NodeType::TextSpan;
  }
};

}  // namespace pagx
