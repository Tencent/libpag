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
#include "pagx/nodes/TextAnimator.h"
#include "pagx/types/TextPathAlign.h"

namespace pagx {

/**
 * Text path modifier.
 */
class TextPath : public TextAnimator {
 public:
  std::string path = {};
  TextPathAlign pathAlign = TextPathAlign::Start;
  float firstMargin = 0;
  float lastMargin = 0;
  bool perpendicularToPath = true;
  bool reversed = false;
  bool forceAlignment = false;

  NodeType type() const override {
    return NodeType::TextPath;
  }

  ElementType elementType() const override {
    return ElementType::TextPath;
  }
};

}  // namespace pagx
