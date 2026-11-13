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

#include <string>
#include <vector>
#include "pagx/nodes/LayoutNode.h"
#include "pagx/nodes/Element.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/types/Color.h"
#include "pagx/types/Point.h"
#include "pagx/types/TextAnchor.h"
#include "pagx/types/TextBaseline.h"

namespace pagx {

/**
 * Text is a geometry element that produces a glyph list after text shaping, which accumulates into
 * the rendering context for subsequent modifiers or painters. It supports two rendering modes:
 * - Pre-shaped mode: Uses GlyphRun children with precomputed glyph IDs and positions, rendered
 *   with embedded fonts to ensure cross-platform consistency.
 * - Runtime shaping mode: Performs text shaping at runtime using the text content and font
 *   properties. Results may vary slightly across platforms due to font and shaping differences.
 */
class Text : public Element, public LayoutNode {
 public:
  /**
   * The text content to render. Supports newline characters (\n) for line breaks, which use the
   * font metrics line height (ascent + descent + leading).
   */
  std::string text = {};

  /**
   * The position of the text origin (x, y where y is the baseline). The default value is (0, 0).
   */
  Point position = {};

  /**
   * The font family name (e.g., "Arial", "Helvetica"). Used for runtime shaping.
   */
  std::string fontFamily = {};

  /**
   * The font style/variant name (e.g., "Regular", "Bold", "Italic", "Bold Italic"). This
   * corresponds to the specific font file variant. The default value is an empty string, which
   * typically resolves to "Regular" during font matching.
   */
  std::string fontStyle = {};

  /**
   * The font size in pixels. The default value is 12.
   */
  float fontSize = 12.0f;

  /**
   * The letter spacing (tracking) value that adjusts spacing between characters. The default value
   * is 0.
   */
  float letterSpacing = 0.0f;

  /**
   * Tracking in 1/1000 em (spec 6.5): independent of letterSpacing, effective spacing =
   * letterSpacing + tracking * fontSize / 1000. Unset when NaN (not authored). The default is 0.
   */
  float tracking = 0.0f;

  /**
   * The baseline shift in pixels for horizontal text. Positive values move every line's baseline
   * up, matching the legacy drawY = baseline - baselineShift convention. The default is 0.
   */
  float baselineShift = 0.0f;

  /**
   * The distance in pixels from the box top to the first line's baseline, replacing the default
   * linebox positioning of the first line. Only consumed for box text (a TextBox with an authored
   * height). Unset when NaN (not authored). The default is NaN.
   */
  float firstBaseLine = NAN;

  /**
   * The text background color, painted below the glyphs of the text block. The default is black.
   */
  Color backgroundColor = {};

  /**
   * The text background opacity in [0, 1]; 0 draws no background. The default is 0.
   */
  float backgroundAlpha = 0.0f;

  /**
   * Whether to apply algorithmic bolding. The default value is false.
   */
  bool fauxBold = false;

  /**
   * Whether to apply algorithmic slanting. The default value is false.
   */
  bool fauxItalic = false;

  /**
   * The text anchor alignment. Controls how text is positioned relative to its origin. Start means
   * the origin is at the text start, Center centers the text on the origin, and End places the text
   * ending at the origin. Ignored when a TextBox controls the layout. The default value is Start.
   */
  TextAnchor textAnchor = TextAnchor::Start;

  /**
   * Pre-shaped glyph runs. When present, these are used for rendering instead of runtime shaping.
   */
  std::vector<GlyphRun*> glyphRuns = {};

  /**
   * Specifies how position.y is interpreted. LineBox (default): position.y is the top of the
   * linebox (based on font metrics line height). Alphabetic: position.y is the alphabetic
   * baseline. When the Text is the first element of a TextBox, its baseline mode seeds the box
   * layout's anchoring.
   */
  TextBaseline baseline = TextBaseline::LineBox;

  ~Text() override;

  /** Returns the text position adjusted to the layout bounds. */
  Point renderPosition() const;

  /** Returns the effective font size after layout scaling. */
  float renderFontSize() const;

  NodeType nodeType() const override {
    return NodeType::Text;
  }

 protected:
  void onMeasure(LayoutContext* context) override;
  void setLayoutSize(LayoutContext* context, float targetWidth, float targetHeight) override;

 private:
  Text();

  struct GlyphData;
  GlyphData* glyphData;
  Rect textBounds = {};
  float textScale = 1.0f;

  friend class FontEmbedder;
  friend class GlyphRunRenderer;
  friend class HTMLWriter;
  friend class LayerBuilderContext;
  friend class PAGXDocument;
  friend class TextBox;
  friend class TextLayout;
};

}  // namespace pagx
