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

namespace pagx {

/**
 * Text vertical alignment within the layout area.
 */
enum class VerticalAlign {
  /**
   * Align text to the top of the layout area.
   */
  Top,
  /**
   * Align text to the vertical center of the layout area.
   */
  Center,
  /**
   * Align text to the bottom of the layout area.
   */
  Bottom
};

/**
 * Text writing mode (horizontal or vertical).
 */
enum class WritingMode {
  /**
   * Horizontal text layout. Lines flow from top to bottom. This is the default mode for most
   * languages.
   */
  Horizontal,
  /**
   * Vertical text layout. Characters are arranged vertically from top to bottom, and columns flow
   * from right to left. This is the traditional writing mode for Chinese, Japanese, and Korean
   * (CJK) text.
   */
  Vertical
};

/**
 * Text overflow behavior when text exceeds the box boundaries.
 */
enum class Overflow {
  /**
   * Text that exceeds the box boundaries is still rendered (default).
   */
  Visible,
  /**
   * Text that exceeds the box boundaries is clipped and not rendered.
   */
  Hidden
};

/**
 * TextBox is a text layout node that controls typography for accumulated Text elements.
 * It re-layouts glyph positions and provides layout capabilities including:
 * - Automatic word wrapping when wordWrap is enabled
 * - Horizontal/vertical writing mode
 * - Overflow control (visible or hidden)
 *
 * The position represents the top-left corner of the text area. When size is (0, 0), position
 * serves as the alignment reference point. For horizontal mode, the first line is positioned with
 * its ascent touching the top of the text area. For vertical mode, the first column is positioned
 * with its right edge touching the right side, and columns flow from right to left.
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
   * The vertical text alignment. When height is 0, alignment is relative to position.y.
   * When height > 0, alignment is within the box height. The default value is Top.
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
