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
#include <unordered_map>
#include <vector>
#include "pagx/FontConfig.h"
#include "pagx/nodes/Text.h"
#include "pagx/types/Rect.h"
#include "tgfx/core/Font.h"
#include "tgfx/core/Matrix.h"
#include "tgfx/core/Point.h"
#include "tgfx/core/RSXform.h"

namespace pagx {

class Element;
class LayoutContext;
struct ShapedText;
struct TextLayoutParams;
class TextLayoutContext;

struct PositionedGlyph {
  tgfx::GlyphID glyphID = 0;
  tgfx::Font font = {};
  float x = 0;
  float y = 0;
};

struct VerticalPositionedGlyph {
  tgfx::GlyphID glyphID = 0;
  tgfx::Font font = {};
  bool useRSXform = false;
  tgfx::Point position = {};
  tgfx::RSXform xform = {};
};

class TextLayoutResult {
 public:
  Rect bounds = {};

  /**
   * Returns the linebox bounds for a specific Text element in the layout coordinate system.
   * For TextBox layouts, this is in TextBox coordinates (before inverse matrix transform).
   * For standalone Text, this is in Text local coordinates.
   */
  Rect getTextBounds(Text* text) const;

  /**
   * Returns the layout glyph runs for a specific Text element. Runs are in layout coordinate
   * system and grouped by font. GlyphIDs reference the original (source) font.
   */
  const std::vector<TextLayoutGlyphRun>* getGlyphRuns(Text* text) const;

 private:
  std::unordered_map<Text*, std::vector<PositionedGlyph>> horizontalGlyphs = {};
  std::unordered_map<Text*, std::vector<VerticalPositionedGlyph>> verticalGlyphs = {};
  std::unordered_map<Text*, Rect> perTextBounds = {};
  std::unordered_map<Text*, std::vector<TextLayoutGlyphRun>> layoutGlyphRuns = {};
  friend class TextLayoutContext;
};

/**
 * TextLayout performs text shaping and layout, converting Text elements into positioned glyph
 * runs (TextLayoutGlyphRun). It handles font matching, fallback, text shaping, and layout
 * (alignment, line breaking, etc.). Both standalone Text and TextBox share the same code path
 * through TextLayoutParams — a standalone Text is treated as a single-element TextBox with NaN
 * box dimensions. GlyphRunRenderer converts layout results to TextBlob at render time.
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
   * Performs text layout and returns TextLayoutResult with bounds and positioned glyph runs.
   * Shapes text, computes line/column breaks, but does not build TextBlob. The caller uses
   * GlyphRunRenderer to convert layout glyph runs to TextBlob with the appropriate inverse matrix.
   */
  static TextLayoutResult Layout(const std::vector<Text*>& textElements,
                                 const TextLayoutParams& params, const LayoutContext& context);

  /**
   * Collects all Text elements from an element list (including nested Groups).
   */
  static void CollectTextElements(const std::vector<Element*>& elements,
                                  std::vector<Text*>& outText);

  /**
   * Collects all Text elements along with their accumulated transformation matrices from root.
   */
  static void CollectTextElements(const std::vector<Element*>& elements,
                                  std::vector<Text*>& outText,
                                  std::vector<tgfx::Matrix>& outMatrices);

  /**
   * Finds a typeface matching the given font family and style.
   */
  static std::shared_ptr<tgfx::Typeface> FindTypeface(const std::string& fontFamily,
                                                      const std::string& fontStyle,
                                                      FontConfig* fontConfig = nullptr);

  /** Writes shaped text data directly to the Text node. */
  static void StoreShapedText(Text* text, ShapedText&& shapedText);

  /** Writes linebox bounds to the Text node (in layout coordinate system). */
  static void StoreTextBounds(Text* text, const Rect& bounds);

  /** Writes layout glyph runs to the Text node (in layout coordinate system, source font). */
  static void StoreLayoutRuns(Text* text, std::vector<TextLayoutGlyphRun>&& runs);
};

}  // namespace pagx
