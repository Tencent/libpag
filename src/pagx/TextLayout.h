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
#include "tgfx/core/TextBlob.h"

namespace pagx {

/**
 * A run of positioned glyphs with the same font, produced by the text layout engine.
 * Coordinates are in the layout coordinate system: TextBox coordinates for TextBox children,
 * Text local coordinates for standalone Text. GlyphIDs reference the original (source) font.
 */
struct TextLayoutGlyphRun {
  tgfx::Font font = {};
  std::vector<tgfx::GlyphID> glyphs = {};
  std::vector<tgfx::Point> positions = {};
  std::vector<tgfx::RSXform> xforms = {};
  // Per-glyph vertical advance (vg.height, i.e. upright vertical advance for CJK glyphs or
  // horizontal advance for rotated Latin glyphs). Only populated for vertical-writing-mode
  // layouts; horizontal layouts leave this empty.
  std::vector<float> advances = {};
  // Per-glyph UAX-14 "can break before this glyph" flag used by the layout justify pass.
  // Only populated for vertical-writing-mode layouts.
  std::vector<bool> canBreakBefore = {};
};

struct Text::GlyphData {
  std::shared_ptr<tgfx::TextBlob> textBlob = nullptr;
  std::vector<tgfx::Point> anchors = {};
  std::vector<TextLayoutGlyphRun> layoutRuns = {};
  float fontLineHeight = 0;
  float fontAscent = 0;
};

class Element;
class LayoutContext;
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
  // Per-glyph vertical-axis advance (the `vg.height` from the layout pass). Equals the natural
  // upright vertical advance for CJK glyphs or the horizontal advance for rotated Latin glyphs,
  // plus letterSpacing. Used by the HTML exporter's vertical justify path to pin inline-block
  // heights so Chromium reproduces tgfx's per-glyph spacing (including justifyGap).
  float advance = 0;
  // UAX-14 "can a line break appear before this glyph" flag from the layout pass. justifyGap
  // is added by tgfx before every glyph with this flag set; the HTML exporter applies the same
  // gap as margin to keep token spacing aligned with tgfx.
  bool canBreakBefore = false;
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
   * Returns the max font metrics line height for a specific Text element.
   * This is the maximum fontLineHeight (|ascent| + descent + leading) across all glyphs
   * shaped for the given Text element. Returns 0 if not found.
   */
  float getFontLineHeight(Text* text) const;

  /**
   * Returns the max font ascent (|ascent|) for a specific Text element across all shaped glyphs.
   * Returns 0 if not found.
   */
  float getFontAscent(Text* text) const;

  /**
   * Returns the layout glyph runs for a specific Text element. Runs are in layout coordinate
   * system and grouped by font. GlyphIDs reference the original (source) font.
   */
  const std::vector<TextLayoutGlyphRun>* getGlyphRuns(Text* text) const;

  /**
   * Extracts layout glyph runs for a specific Text element using move semantics.
   * The runs are removed from this result after extraction.
   */
  std::vector<TextLayoutGlyphRun> extractLayoutRuns(Text* text);

 private:
  std::unordered_map<Text*, std::vector<PositionedGlyph>> horizontalGlyphs = {};
  std::unordered_map<Text*, std::vector<VerticalPositionedGlyph>> verticalGlyphs = {};
  std::unordered_map<Text*, Rect> perTextBounds = {};
  std::unordered_map<Text*, std::vector<TextLayoutGlyphRun>> layoutGlyphRuns = {};
  std::unordered_map<Text*, float> perTextFontLineHeight = {};
  std::unordered_map<Text*, float> perTextFontAscent = {};
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
   * Performs text layout and returns TextLayoutResult with bounds and positioned glyph runs.
   * Shapes text, computes line/column breaks, but does not build TextBlob. The caller uses
   * GlyphRunRenderer to convert layout glyph runs to TextBlob with the appropriate inverse matrix.
   */
  static TextLayoutResult Layout(const std::vector<Text*>& textElements,
                                 const TextLayoutParams& params, LayoutContext* context);

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
};

}  // namespace pagx
