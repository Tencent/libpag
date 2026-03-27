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

#include <memory>
#include <string>
#include "pagx/FontConfig.h"
#include "pagx/types/Rect.h"
#include "pagx/types/TextBaseline.h"
#include "tgfx/core/Rect.h"

namespace pagx {

class LayoutContext;
struct ShapedText;
class Text;
class TextBox;

/**
 * TextLayout performs text shaping and layout, converting Text elements into positioned glyph
 * data (TextBlob). It handles font matching, fallback, text shaping, and layout (alignment, line
 * breaking, etc.).
 */
class TextLayout {
 public:
  /**
   * Measures a TextBox element, returning linebox dimensions.
   */
  static Rect MeasureTextBox(const TextBox* textBox, float boxWidth, float boxHeight,
                             FontConfig* fontConfig = nullptr);

  /**
   * Finds a typeface matching the given font family and style.
   */
  static std::shared_ptr<tgfx::Typeface> FindTypeface(const std::string& fontFamily,
                                                      const std::string& fontStyle,
                                                      FontConfig* fontConfig = nullptr);

  /** Measures a TextBox using LayoutContext for font lookup. */
  static Rect MeasureTextBox(const TextBox* textBox, float boxWidth, float boxHeight,
                             const LayoutContext& context);

  /**
   * Builds TextBlob for a standalone Text element. Returns the text bounds where horizontal extent
   * uses advance width (0 to advance end) and vertical extent uses tight pixel bounds. The baseline
   * parameter controls whether the TextBlob includes the visual-top-to-baseline y offset.
   */
  static tgfx::Rect LayoutText(Text* text, const LayoutContext& context, TextBaseline baseline);

  /** Performs text layout for all Text elements inside a TextBox. */
  static void LayoutTextBox(TextBox* textBox, float boxWidth, float boxHeight,
                            const LayoutContext& context);

  /** Writes shaped text data directly to the Text node. */
  static void StoreShapedText(Text* text, ShapedText&& shapedText);
};

}  // namespace pagx
