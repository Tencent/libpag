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
#include <vector>
#include "pagx/FontConfig.h"
#include "pagx/types/Rect.h"

namespace pagx {

class Element;
class LayoutContext;
struct ShapedText;
struct TextLayoutParams;
class Text;

/**
 * TextLayout performs text shaping and layout, converting Text elements into positioned glyph
 * data (TextBlob). It handles font matching, fallback, text shaping, and layout (alignment, line
 * breaking, etc.). Both standalone Text and TextBox share the same code path through
 * TextLayoutParams — a standalone Text is treated as a single-element TextBox with NaN box
 * dimensions.
 */
class TextLayout {
 public:
  /**
   * Measures text elements and returns linebox bounds. For standalone Text, pass a single-element
   * vector with default TextLayoutParams. For TextBox, pass all child Text elements with params
   * from TextBox attributes. When all Text elements have embedded GlyphRuns with bounds, the
   * bounds are read directly without font shaping.
   */
  static Rect Measure(const std::vector<Text*>& textElements, const TextLayoutParams& params,
                      FontConfig* fontConfig = nullptr);

  /**
   * Measures text elements using LayoutContext for font lookup.
   */
  static Rect Measure(const std::vector<Text*>& textElements, const TextLayoutParams& params,
                      const LayoutContext& context);

  /**
   * Performs text layout and returns linebox bounds. Shapes text, computes line/column breaks,
   * builds TextBlob for each Text element. The baseline parameter in TextLayoutParams controls
   * the y-offset model (LineBox vs Alphabetic).
   */
  static Rect Layout(const std::vector<Text*>& textElements, const TextLayoutParams& params,
                     const LayoutContext& context);

  /**
   * Collects all Text elements from an element list (including nested Groups).
   */
  static void CollectTextElements(const std::vector<Element*>& elements,
                                  std::vector<Text*>& outText);

  /**
   * Finds a typeface matching the given font family and style.
   */
  static std::shared_ptr<tgfx::Typeface> FindTypeface(const std::string& fontFamily,
                                                      const std::string& fontStyle,
                                                      FontConfig* fontConfig = nullptr);

  /** Writes shaped text data directly to the Text node. */
  static void StoreShapedText(Text* text, ShapedText&& shapedText);
};

}  // namespace pagx
