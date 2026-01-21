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

#include "pagx/nodes/Element.h"
#include "pagx/nodes/TextAlign.h"

namespace pagx {

/**
 * Text vertical alignment.
 */
enum class VerticalAlign {
  Top,
  Center,
  Bottom
};

/**
 * Text direction (horizontal or vertical writing mode).
 */
enum class TextDirection {
  Horizontal,
  Vertical
};

/**
 * TextLayout is a text modifier that controls text layout and alignment. It supports two modes:
 * - Point Text mode (width=0): Single-line text with anchor-based alignment at position (x, y).
 * - Box Text mode (width>0): Multi-line text within a bounding box with word wrapping.
 */
class TextLayout : public Element {
 public:
  /**
   * The x position of the text layout origin. The default value is 0.
   */
  float x = 0;

  /**
   * The y position of the text layout origin. The default value is 0.
   */
  float y = 0;

  /**
   * The width of the text box in pixels. A value of 0 enables Point Text mode (single-line,
   * anchor-based alignment). A value greater than 0 enables Box Text mode (multi-line with word
   * wrapping). The default value is 0.
   */
  float width = 0;

  /**
   * The height of the text box in pixels. A value of 0 means auto-height. The default value is 0.
   */
  float height = 0;

  /**
   * The horizontal text alignment. The default value is Start.
   */
  TextAlign textAlign = TextAlign::Start;

  /**
   * The alignment of the last line when textAlign is Justify. The default value is Start.
   */
  TextAlign textAlignLast = TextAlign::Start;

  /**
   * The vertical text alignment (only effective when height > 0). The default value is Top.
   */
  VerticalAlign verticalAlign = VerticalAlign::Top;

  /**
   * The line height multiplier. The default value is 1.2.
   */
  float lineHeight = 1.2f;

  /**
   * The text direction (horizontal or vertical writing mode). The default value is Horizontal.
   */
  TextDirection direction = TextDirection::Horizontal;

  NodeType nodeType() const override {
    return NodeType::TextLayout;
  }
};

}  // namespace pagx
