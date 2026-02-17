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
#include "pagx/types/Overflow.h"
#include "pagx/types/Point.h"
#include "pagx/types/Size.h"
#include "pagx/types/TextAlign.h"
#include "pagx/types/VerticalAlign.h"
#include "pagx/types/WritingMode.h"

namespace pagx {

/**
 * TextBox is a text layout node that controls typography for accumulated Text elements.
 * It re-layouts glyph positions and provides layout capabilities including:
 * - Automatic word wrapping when wordWrap is enabled
 * - Horizontal/vertical writing mode
 * - Overflow control (visible or hidden)
 *
 * The default vertical alignment is Baseline, where position.y represents the first line's baseline
 * coordinate directly. When verticalAlign is Top, the first line is positioned with its ascent
 * touching the top of the text area. For vertical mode, the first column is positioned with its
 * right edge touching the right side, and columns flow from right to left.
 */
class TextBox : public Element {
 public:
  /**
   * The position of the text box, representing the top-left corner of the text area. When size is
   * (0, 0), this serves as the alignment reference point. The default value is (0, 0).
   */
  Point position = {};

  /**
   * The size of the text box. When width or height is 0, text has no boundary in that dimension
   * (wordWrap wraps each character individually, alignment uses position as the reference point).
   * The default value is (0, 0).
   */
  Size size = {};

  /**
   * The horizontal text alignment. When width is 0, alignment is relative to position.x.
   * When width > 0, alignment is within the box width. The default value is Start.
   */
  TextAlign textAlign = TextAlign::Start;

  /**
   * The vertical text alignment. Default is Baseline, where position.y represents the first line's
   * baseline coordinate directly. When height is 0, alignment is relative to position.y.
   * When height > 0, alignment is within the box height.
   */
  VerticalAlign verticalAlign = VerticalAlign::Baseline;

  /**
   * The writing mode (horizontal or vertical text). Vertical mode uses right-to-left column
   * ordering (traditional CJK vertical text). The default value is Horizontal.
   */
  WritingMode writingMode = WritingMode::Horizontal;

  /**
   * The line height in pixels. When set to 0 (default), the line height is automatically calculated
   * from font metrics (ascent + descent + leading). When set to a positive value, all lines use
   * that fixed height. In vertical mode, this controls the column width instead.
   */
  float lineHeight = 0;

  /**
   * Whether automatic word wrapping is enabled. When true, text wraps at the box width boundary
   * (horizontal mode) or height boundary (vertical mode). When width/height is 0, each character
   * wraps individually. The default value is false.
   */
  bool wordWrap = false;

  /**
   * The overflow behavior when text exceeds the box boundaries. When set to Hidden, entire lines
   * (horizontal mode) or columns (vertical mode) that exceed the boundary are discarded during
   * typesetting. The default value is Visible.
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
