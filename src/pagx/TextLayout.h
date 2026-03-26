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
#include "ShapedText.h"
#include "pagx/FontConfig.h"
#include "pagx/PAGXDocument.h"
#include "pagx/types/Rect.h"

namespace pagx {

class LayoutContext;
class TextBox;

/**
 * TextLayout performs text layout on PAGXDocument, converting Text elements into positioned glyph
 * data (TextBlob). It handles font matching, fallback, text shaping, and layout (alignment, line
 * breaking, etc.).
 */
class TextLayout {
 public:
  /**
   * Performs text layout for all Text nodes in the document.
   * @param document The document containing Text elements to layout.
   * @param fontConfig Optional font config for font matching. If nullptr, only system fonts
   *                   are available.
   * @return A TextLayoutResult containing shaped text data for each Text element.
   */
  static TextLayoutResult Layout(PAGXDocument* document, FontConfig* fontConfig = nullptr);

  /**
   * Precisely measures the bounds of a Text element. Uses embedded GlyphRun data if available,
   * otherwise constructs a TextBlob from the text content and font for accurate measurement.
   * @param text The Text element to measure.
   * @param fontConfig Optional font config for font matching.
   * @return The bounding rectangle of the text.
   */
  static Rect MeasureText(const Text* text, FontConfig* fontConfig = nullptr);

  /**
   * Measures the content bounds of a TextBox by performing a full typesetting pass on its child
   * Text elements. Uses the TextBox's width/height (NaN means no boundary) and alignment settings.
   * Returns the tight bounds of the typeset result.
   * @param textBox The TextBox to measure.
   * @param fontConfig Optional font config for font matching.
   * @return The tight bounding rectangle of the typeset text.
   */
  static Rect MeasureTextBox(const TextBox* textBox, FontConfig* fontConfig = nullptr);

  /**
   * Finds a typeface matching the given font family and style.
   * @param fontFamily The font family name.
   * @param fontStyle The font style name.
   * @param fontConfig Optional font config. If nullptr, uses system font lookup.
   * @return The matching typeface, or nullptr if not found.
   */
  static std::shared_ptr<tgfx::Typeface> FindTypeface(const std::string& fontFamily,
                                                      const std::string& fontStyle,
                                                      FontConfig* fontConfig = nullptr);

  /** Measures a Text element using LayoutContext for font lookup. */
  static Rect MeasureText(const Text* text, const LayoutContext& context);

  /** Measures a TextBox using LayoutContext for font lookup. */
  static Rect MeasureTextBox(const TextBox* textBox, const LayoutContext& context);

  /** Builds TextBlob for a standalone Text element (not inside TextBox). */
  static void LayoutText(Text* text, const LayoutContext& context);

  /** Performs text layout for all Text elements inside a TextBox. */
  static void LayoutTextBox(TextBox* textBox, const LayoutContext& context);
};

}  // namespace pagx
