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

#include "pagx/nodes/Group.h"
#include "pagx/types/Overflow.h"
#include "pagx/types/ParagraphAlign.h"
#include "pagx/types/TextAlign.h"
#include "pagx/types/WritingMode.h"

namespace pagx {

/**
 * TextBox is a text layout container that inherits from Group, providing typography control for
 * accumulated Text elements. It recalculates glyph positions and provides layout capabilities
 * including:
 * - Automatic word wrapping when wordWrap is enabled
 * - Horizontal/vertical writing mode
 * - Overflow control (visible or hidden)
 *
 * The inherited width and height define the text area dimensions. When width or height is NaN, the
 * TextBox has no boundary in that dimension (auto-sizing). In vertical mode, the first column is
 * positioned with its right edge touching the right side, and columns flow from right to left.
 *
 * After typesetting, TextBox behaves as a regular Group containing positioned Text elements.
 */
class TextBox : public Group {
 public:
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
   * (horizontal mode) or height boundary (vertical mode). Has no effect when that dimension is NaN
   * (auto-sizing). The default value is true.
   */
  bool wordWrap = true;

  /**
   * The overflow behavior when text exceeds the box boundaries. When set to Hidden, entire lines
   * that exceed the box height (horizontal mode) or columns that exceed the box width (vertical
   * mode) are discarded during typesetting. Has no effect when that dimension is NaN (auto-sizing).
   * The default value is Visible.
   */
  Overflow overflow = Overflow::Visible;

  NodeType nodeType() const override {
    return NodeType::TextBox;
  }

 protected:
  void onMeasure(LayoutContext* context) override;
  void setLayoutSize(LayoutContext* context, float width, float height) override;
  void updateLayout(LayoutContext* context) override;

 private:
  TextBox() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
