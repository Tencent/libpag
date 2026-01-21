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

#include "pagx/nodes/Node.h"
#include "pagx/types/Overflow.h"
#include "pagx/types/TextAlign.h"
#include "pagx/types/VerticalAlign.h"

namespace pagx {

/**
 * Text layout modifier.
 */
struct TextLayout : public Node {
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
};

}  // namespace pagx
