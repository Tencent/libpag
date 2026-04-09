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
#include "tgfx/core/Matrix.h"
#include "tgfx/core/TextBlob.h"
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
   */
  static void BuildTextBlob(Text* text, const tgfx::Matrix& inverseMatrix);

  /**
   * Builds a TextBlob from runtime layout glyph runs (TextLayoutGlyphRun), applying the inverse
   * matrix to convert from layout coordinates to Text local coordinates.
   */
  static std::shared_ptr<tgfx::TextBlob> BuildTextBlobFromLayoutRuns(
      const std::vector<TextLayoutGlyphRun>& runs, const tgfx::Matrix& inverseMatrix);

 private:
  static std::shared_ptr<tgfx::Typeface> BuildTypefaceFromFont(const Font* fontNode);
};

}  // namespace pagx
