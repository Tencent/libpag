/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "pagx/nodes/Element.h"
#include "pagx/types/Point.h"
#include "pagx/types/RepeaterOrder.h"

namespace pagx {

/**
 * Repeater is a modifier that creates multiple copies of preceding elements with progressive
 * transformations. Each copy can have an incremental offset in position, rotation, scale, and
 * opacity.
 */
class Repeater : public Element {
 public:
  /**
   * The number of copies to create. The default value is 3.
   */
  float copies = 3;

  /**
   * The offset applied to the copy index, allowing fractional copies. The default value is 0.
   */
  float offset = 0;

  /**
   * The stacking order of copies (BelowOriginal or AboveOriginal). The default value is
   * BelowOriginal.
   */
  RepeaterOrder order = RepeaterOrder::BelowOriginal;

  /**
   * The anchor point for transformations.
   */
  Point anchor = {};

  /**
   * The position offset applied between each copy. The default value is {100, 100}.
   */
  Point position = {100, 100};

  /**
   * The rotation angle in degrees applied between each copy. The default value is 0.
   */
  float rotation = 0;

  /**
   * The scale factor applied between each copy. The default value is {1, 1}.
   */
  Point scale = {1, 1};

  /**
   * The starting opacity for the first copy, ranging from 0 to 1. The default value is 1.
   */
  float startAlpha = 1;

  /**
   * The ending opacity for the last copy, ranging from 0 to 1. The default value is 1.
   */
  float endAlpha = 1;

  NodeType nodeType() const override {
    return NodeType::Repeater;
  }

 private:
  Repeater() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
