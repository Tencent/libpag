/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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
#include "pagx/types/Size.h"
#include "pagx/types/TextAlign.h"
#include "pagx/types/TextBoxTypes.h"

namespace pagx {

/**
 * TextBox is a text modifier that controls text layout and alignment for accumulated Text elements.
 * It overrides the position of Text elements and provides layout capabilities including:
 * - Anchor mode (no size): position serves as an anchor point for alignment
 * - Box mode (with size): defines a rectangular area for text layout
 * - Automatic word wrapping when wordWrap is enabled
 * - Horizontal/vertical writing mode
 * - Overflow control (visible or hidden)
 *
 * The position always represents the top-left corner of the text area (or anchor point when size
 * is zero). The first line of text is positioned with its ascent touching the top of the box.
 */
class TextBox : public Element {
 public:
  /**
   * The position of the text box. When size is (0, 0), this serves as the anchor point for text
   * alignment. Otherwise, it is the top-left corner of the text box. The default value is (0, 0).
   */
  Point position = {};

  /**
   * The size of the text box. A value of 0 for width or height means auto-sizing in that
   * dimension. When both are 0, the text box operates in anchor mode. The default value is (0, 0).
   */
  Size size = {};

  /**
   * The horizontal text alignment. In anchor mode (size = 0,0), alignment is relative to the
   * anchor point. In box mode, alignment is within the box width. The default value is Start.
   */
  TextAlign textAlign = TextAlign::Start;

  /**
   * The vertical text alignment. In anchor mode with height = 0, alignment is relative to the
   * anchor point (e.g., center = vertically centered on anchor). In box mode with height > 0,
   * alignment is within the box height. The default value is Top.
   */
  VerticalAlign verticalAlign = VerticalAlign::Top;

  /**
   * The writing mode (horizontal or vertical text). Vertical mode uses right-to-left column
   * ordering (traditional CJK vertical text). The default value is Horizontal.
   */
  WritingMode writingMode = WritingMode::Horizontal;

  /**
   * The line height multiplier. Applied to font size to calculate line spacing. The default value
   * is 1.2.
   */
  float lineHeight = 1.2f;

  /**
   * Whether automatic word wrapping is enabled. When true and width > 0, text wraps at the box
   * width boundary. When false, text only breaks at explicit newline characters. The default value
   * is false.
   */
  bool wordWrap = false;

  /**
   * The overflow behavior when text exceeds the box boundaries. The default value is Visible.
   */
  Overflow overflow = Overflow::Visible;

  NodeType nodeType() const override {
    return NodeType::TextBox;
  }

 private:
  TextBox() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
