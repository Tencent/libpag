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
#include "pagx/types/ParagraphAlign.h"
#include "pagx/types/WritingMode.h"

namespace pagx {

/**
 * TextBox is a text layout node that controls typography for accumulated Text elements.
 * It re-layouts glyph positions and provides layout capabilities including:
 * - Automatic word wrapping when wordWrap is enabled
 * - Horizontal/vertical writing mode
 * - Overflow control (visible or hidden)
 *
 * Position and size define the text area. In vertical mode, the first column is positioned with
 * its right edge touching the right side, and columns flow from right to left.
 */
class TextBox : public Element {
 public:
  /**
   * The top-left corner of the text area. The default value is (0, 0).
   */
  Point position = {};

  /**
   * The size of the text box. When width or height is 0, text has no boundary in that dimension,
   * which may cause wordWrap or overflow to have no effect. The default value is (0, 0).
   */
  Size size = {};

  /**
   * The text alignment along the inline direction. In horizontal mode, this controls horizontal
   * alignment within each line. In vertical mode, this controls vertical alignment within each
   * column. The default value is Start.
   */
  TextAlign textAlign = TextAlign::Start;

  /**
   * The paragraph alignment along the block-flow direction. In horizontal mode, this controls
   * vertical positioning of lines. In vertical mode, this controls horizontal positioning of
   * columns. The default value is Near.
   */
  ParagraphAlign paragraphAlign = ParagraphAlign::Near;

  /**
   * The writing mode (horizontal or vertical text). Vertical mode uses right-to-left column
   * ordering (traditional CJK vertical text). The default value is Horizontal.
   */
  WritingMode writingMode = WritingMode::Horizontal;

  /**
   * The line height in pixels. When set to 0 (default), the line height is automatically calculated
   * from font metrics (ascent + descent + leading). When set to a positive value, all lines use
   * that fixed height. Following CSS Writing Modes conventions, in vertical mode this property
   * controls the column width instead of the line height, since line-height is a logical property
   * that always applies to the block-axis dimension of a line box.
   */
  float lineHeight = 0.0f;

  /**
   * Whether automatic word wrapping is enabled. When true, text wraps at the box width boundary
   * (horizontal mode) or height boundary (vertical mode). Has no effect when that dimension of
   * size is 0. The default value is true.
   */
  bool wordWrap = true;

  /**
   * The overflow behavior when text exceeds the box boundaries. When set to Hidden, entire lines
   * that exceed the box height (horizontal mode) or columns that exceed the box width (vertical
   * mode) are discarded during typesetting. Has no effect when that dimension of size is 0. The
   * default value is Visible.
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
