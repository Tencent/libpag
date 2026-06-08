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
#include <vector>
#include "tgfx/core/Font.h"
#include "tgfx/core/Matrix.h"
#include "tgfx/core/Point.h"
#include "tgfx/core/TextBlob.h"
#include "tgfx/core/TextBlobBuilder.h"
#include "tgfx/core/Typeface.h"

namespace pagx {

class GlyphRun;
class Font;
class Text;
struct TextLayoutGlyphRun;

/**
 * GlyphRunRenderer converts glyph run data (in layout coordinate system) into tgfx::TextBlob
 * (in Text local coordinate system) by applying the inverse transform matrix. This is a render-time
 * operation — the layout engine produces glyph runs in a unified coordinate system, and the
 * renderer converts them to Text-local TextBlobs just before building the tgfx layer tree.
 *
 * Two input formats are supported:
 * - TextLayoutGlyphRun: runtime layout output with source fonts (tgfx::Font).
 * - pagx::GlyphRun: embedded/precomposed format with embedded fonts (pagx::Font).
 */
class GlyphRunRenderer {
 public:
  /**
   * Builds a TextBlob from a Text element's embedded GlyphRuns (pagx::GlyphRun), applying the
   * inverse matrix to convert from layout coordinates to Text local coordinates. Writes the
   * resulting TextBlob and per-glyph anchors directly into text->glyphData.
   *
   * Side effect: lazily caches a built tgfx::Typeface on each referenced pagx::Font node, so
   * concurrent calls touching overlapping Font nodes are not safe; callers must serialize per
   * PAGXDocument.
   */
  static void BuildTextBlob(Text* text, const tgfx::Matrix& inverseMatrix);

  /**
   * Builds a TextBlob from runtime layout glyph runs (TextLayoutGlyphRun), applying the inverse
   * matrix to convert from layout coordinates to Text local coordinates.
   */
  static std::shared_ptr<tgfx::TextBlob> BuildTextBlobFromLayoutRuns(
      const std::vector<TextLayoutGlyphRun>& runs, const tgfx::Matrix& inverseMatrix);

  /**
   * Read-only accessor for the layout glyph runs already computed by
   * Text::onMeasure (via TextLayout). Used by PAG v2 TextBaker (Phase 8) to
   * serialise runtime-shaped text without re-running TextLayout. Returns an
   * empty vector when the Text has not been laid out (e.g. the caller skipped
   * PAGXDocument::applyLayout()) or when it is in pre-shaped mode.
   */
  static const std::vector<TextLayoutGlyphRun>& GetLayoutRuns(const Text* text);

  /**
   * Composes per-glyph transform matrices for a single GlyphRun using the
   * anchor + scale + skew + rotate + position composition shared between
   * PAGX-native rendering and the PAG v2 inflater. Each input array follows
   * the PAGX <GlyphRun> semantics: a count of 0 means "field absent"; index
   * out of range falls back to the per-glyph identity (scale=1, rotation=0,
   * skew=0). `font` provides per-glyph advance for fallback positioning when
   * neither positions nor xOffsets cover a glyph index. `postMatrix` is
   * applied after the per-glyph composition (the PAGX-native path passes the
   * Text's inverse layout matrix here; the PAG inflater passes Identity since
   * runs already carry Text-local absolute coordinates). Output is written
   * verbatim into `glyphMatrices`, sized to glyphCount.
   */
  static void ComposeGlyphMatrices(const tgfx::Font& font, const tgfx::GlyphID* glyphs,
                                   size_t glyphCount, float runX, float runY, const float* xOffsets,
                                   size_t xOffsetCount, const tgfx::Point* positions,
                                   size_t positionCount, const tgfx::Point* anchors,
                                   size_t anchorCount, const tgfx::Point* scales, size_t scaleCount,
                                   const float* rotations, size_t rotationCount, const float* skews,
                                   size_t skewCount, const tgfx::Matrix& postMatrix,
                                   std::vector<tgfx::Matrix>* glyphMatrices);

  /**
   * Writes a glyph run into a TextBlobBuilder, classifying the per-glyph
   * matrix set into the most compact mode (Point → RSXform → Matrix). Pairs
   * with ComposeGlyphMatrices and is shared by the PAGX-native renderer and
   * the PAG v2 inflater so the two paths stay pixel-aligned.
   */
  static void AppendGlyphsToBlobBuilder(tgfx::TextBlobBuilder& builder, const tgfx::Font& font,
                                        const tgfx::GlyphID* glyphs, size_t glyphCount,
                                        const std::vector<tgfx::Matrix>& glyphMatrices);

 private:
  static std::shared_ptr<tgfx::Typeface> BuildTypefaceFromFont(Font* fontNode);
};

}  // namespace pagx
