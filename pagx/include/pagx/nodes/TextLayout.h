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
#include "pagx/types/Point.h"
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
 * TextLayout is a text modifier that controls text layout and alignment for accumulated Text
 * elements. It overrides the position of Text elements and provides layout capabilities including:
 * - Point text mode (no width): position and alignment relative to anchor point
 * - Paragraph text mode (with width): automatic line wrapping within bounds
 * - Vertical alignment (when height is specified)
 * - Horizontal/vertical writing mode
 */
class TextLayout : public Element {
 public:
  /**
   * The position of the layout origin. The default value is (0, 0).
   */
  Point position = {};

  /**
   * The width of the layout area in pixels. When specified (> 0), enables automatic line wrapping
   * (paragraph text mode). A value of 0 or negative means point text mode (no wrapping).
   * The default value is 0.
   */
  float width = 0;

  /**
   * The height of the layout area in pixels. When specified (> 0), enables vertical alignment.
   * A value of 0 or negative means auto-height. The default value is 0.
   */
  float height = 0;

  /**
   * The horizontal text alignment. The default value is Start.
   */
  TextAlign textAlign = TextAlign::Start;

  /**
   * The vertical text alignment (only effective when height > 0). The default value is Top.
   */
  VerticalAlign verticalAlign = VerticalAlign::Top;

  /**
   * The writing mode (horizontal or vertical text). The default value is Horizontal. Vertical mode
   * uses right-to-left column ordering (traditional CJK vertical text).
   */
  WritingMode writingMode = WritingMode::Horizontal;

  /**
   * The line height multiplier. Applied to font size to calculate line spacing. The default value
   * is 1.2.
   */
  float lineHeight = 1.2f;

  NodeType nodeType() const override {
    return NodeType::TextLayout;
  }

 private:
  TextLayout() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
