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

#include "pagx/model/Element.h"
#include "pagx/model/types/TrimType.h"

namespace pagx {

/**
 * TrimPath is a path modifier that trims paths to a specified range. It can be used to animate
 * path drawing or reveal effects by adjusting the start and end values.
 */
class TrimPath : public Element {
 public:
  /**
   * The starting point of the trim as a percentage of the path length, ranging from 0 to 1. The
   * default value is 0.
   */
  float start = 0;

  /**
   * The ending point of the trim as a percentage of the path length, ranging from 0 to 1. The
   * default value is 1.
   */
  float end = 1;

  /**
   * The offset to shift the trim range along the path, where 1 represents a full path length. The
   * default value is 0.
   */
  float offset = 0;

  /**
   * The trim type that determines how multiple paths are trimmed. Separate trims each path
   * individually, while Simultaneous trims all paths as one continuous path. The default value is
   * Separate.
   */
  TrimType trimType = TrimType::Separate;

  ElementType type() const override {
    return ElementType::TrimPath;
  }
};

}  // namespace pagx
