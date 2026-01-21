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
#include "pagx/model/types/Overflow.h"
#include "pagx/model/types/TextAlign.h"
#include "pagx/model/types/VerticalAlign.h"

namespace pagx {

/**
 * TextLayout is a text animator that controls text layout within a bounding box. It provides
 * options for text alignment, line height, indentation, and overflow handling.
 */
class TextLayout : public Element {
 public:
  /**
   * The width of the text box in pixels. A value of 0 means auto-width. The default value is 0.
   */
  float width = 0;

  /**
   * The height of the text box in pixels. A value of 0 means auto-height. The default value is 0.
   */
  float height = 0;

  /**
   * The horizontal text alignment (Left, Center, Right, or Justify). The default value is Left.
   */
  TextAlign textAlign = TextAlign::Left;

  /**
   * The vertical text alignment (Top, Middle, or Bottom). The default value is Top.
   */
  VerticalAlign verticalAlign = VerticalAlign::Top;

  /**
   * The line height multiplier. The default value is 1.2.
   */
  float lineHeight = 1.2f;

  /**
   * The first-line indent in pixels. The default value is 0.
   */
  float indent = 0;

  /**
   * The overflow behavior when text exceeds the bounding box (Clip, Visible, or Scroll). The
   * default value is Clip.
   */
  Overflow overflow = Overflow::Clip;

  ElementType elementType() const override {
    return ElementType::TextLayout;
  }

  NodeType type() const override {
    return NodeType::TextLayout;
  }
};

}  // namespace pagx
