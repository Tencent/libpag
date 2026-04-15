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
};

struct Text::GlyphData {
  std::shared_ptr<tgfx::TextBlob> textBlob = nullptr;
  std::vector<tgfx::Point> anchors = {};
  std::vector<TextLayoutGlyphRun> layoutRuns = {};
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
};

/**
 * Line-level metadata for a single Text element within a layout. Records the baseline position,
 * alignment-adjusted x offset, and byte range in the source Text::text string.
 */
struct TextLayoutLineInfo {
  float baselineY = 0;
  float startX = 0;
  uint32_t byteStart = 0;
  uint32_t byteEnd = 0;
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

  /**
   * Extracts layout glyph runs for a specific Text element using move semantics.
   * The runs are removed from this result after extraction.
   */
  std::vector<TextLayoutGlyphRun> extractLayoutRuns(Text* text);

 /**
  * Returns the line-level metadata for a specific Text element. Each entry corresponds to one
  * line of text with baseline position, alignment offset, and byte range in the source string.
  * Returns nullptr if no line info is available for this text (e.g. vertical layout or embedded
  * glyph runs).
  */
 const std::vector<TextLayoutLineInfo>* getTextLines(Text* text) const;

 private:
  std::unordered_map<Text*, std::vector<PositionedGlyph>> horizontalGlyphs = {};
  std::unordered_map<Text*, std::vector<VerticalPositionedGlyph>> verticalGlyphs = {};
  std::unordered_map<Text*, Rect> perTextBounds = {};
  std::unordered_map<Text*, std::vector<TextLayoutGlyphRun>> layoutGlyphRuns = {};
  std::unordered_map<Text*, std::vector<TextLayoutLineInfo>> textLines = {};
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
