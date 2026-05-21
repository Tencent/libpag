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

#include <cmath>
#include <string>
#include <vector>
#include "pagx/types/Color.h"
#include "pagx/types/ColorSpace.h"
#include "pagx/types/Padding.h"

namespace pagx {

/**
 * Maximum nesting depth tolerated during HTML traversal. Mirrors the SVG importer.
 */
static constexpr int MAX_HTML_RECURSION_DEPTH = 128;

/**
 * Default CSS font-size used for HTML text leaves before any user style is applied.
 */
static constexpr float HTML_DEFAULT_FONT_SIZE = 14.0f;

/**
 * Default font family used when no `font-family` is inherited.
 */
static constexpr const char* HTML_DEFAULT_FONT_FAMILY = "Arial";

/**
 * Default text colour used when no `color` is inherited; matches the CSS default for
 * <body> in `ElementDefaults()`.
 */
static constexpr const char* HTML_DEFAULT_TEXT_COLOR = "#1E293B";

/**
 * Pixel tolerance used when comparing image and wrapper geometry for the rounded-image
 * fold optimisation in `foldRoundedImageWrapper`.
 */
static constexpr float HTML_IMAGE_WRAPPER_TOLERANCE_PX = 0.5f;

/**
 * Inherited CSS style properties that cascade down the element tree during HTML
 * traversal. Empty strings denote "not set" (preserved so that descendants can know
 * whether to inherit further or use their own defaults).
 */
struct HTMLInheritedStyle {
  std::string color = {};
  std::string fontFamily = {};
  std::string fontSize = {};
  std::string fontWeight = {};
  std::string fontStyle = {};
  std::string fontStyleName = {};  // computed combination, e.g. "Bold Italic"
  std::string letterSpacing = {};
  std::string lineHeight = {};
  std::string textAlign = {};
  std::string textDecoration = {};
  std::string textDecorationColor = {};
  std::string whiteSpace = {};
  std::string writingMode = {};
  // Gradient string ("linear-gradient(...)" / "radial-gradient(...)" / "conic-gradient(...)")
  // inherited from the nearest ancestor that combined `background-clip: text` with a gradient
  // `background-image`. Empty means descendants paint text with their own solid `color`.
  std::string textFillImage = {};

  // Pre-resolved numeric forms of the cascade. Kept in lock-step with the string fields by
  // `computeInherited` so text-leaf conversion doesn't re-parse the same `font-size` /
  // `letter-spacing` / `color` strings for every fragment.
  float fontSizePx = HTML_DEFAULT_FONT_SIZE;
  float letterSpacingPx = 0.0f;
  Color resolvedTextColor = {0, 0, 0, 1, ColorSpace::SRGB};

  // Pre-resolved CSS font-family stack. Kept in lock-step with `fontFamily` by
  // `computeInherited`. `primaryFontFamily` is the first concrete name (after surrounding
  // quotes are stripped and generic keywords are mapped to platform fonts) and gets
  // written to `Text::fontFamily`. `fontFamilyChain` holds the same first name followed by
  // every additional concrete name from the stack, in CSS order; the importer unions all
  // chains across the document and registers the union as user fallback fonts on the
  // document's FontConfig so per-glyph fallback in LayoutContext can pick them up.
  std::string primaryFontFamily = {};
  std::vector<std::string> fontFamilyChain = {};
};

/**
 * Resolved box-model attributes for a single HTML element. Anything left as NaN / empty
 * is "not specified" and falls back to PAGX defaults (which mirror CSS defaults for the
 * accepted subset).
 */
struct HTMLBoxAttributes {
  // Sizing
  float widthPx = NAN;    // explicit px width
  float widthPct = NAN;   // explicit % width (0-100)
  float heightPx = NAN;   // explicit px height
  float heightPct = NAN;  // explicit % height (0-100)

  // Positioning (only when position: absolute)
  bool absolute = false;
  float leftPx = NAN;
  float rightPx = NAN;
  float topPx = NAN;
  float bottomPx = NAN;

  // Layout
  bool displayFlex = false;
  bool flexRow = true;  // default of CSS flex
  float gapPx = 0.0f;
  bool gapSet = false;
  Padding padding = {};
  bool paddingSet = false;
  std::string alignItems = {};
  std::string justifyContent = {};
  float flexGrow = 0.0f;
  bool flexGrowSet = false;

  // Effects
  Color backgroundColor = {0, 0, 0, 0, ColorSpace::SRGB};
  bool backgroundColorSet = false;
  std::string backgroundImage = {};
  // True when computed style had `background-clip: text`. Only meaningful with a gradient
  // `background-image`; the gradient is then routed onto descendant text fills instead of
  // painting a rectangle on this element.
  bool backgroundClipText = false;

  float borderRadiusPx = 0.0f;
  bool borderRadiusSet = false;

  float borderWidthPx = 0.0f;
  Color borderColor = {0, 0, 0, 1, ColorSpace::SRGB};
  bool borderSet = false;

  std::string boxShadow = {};
  std::string filter = {};
  std::string backdropFilter = {};

  float opacity = 1.0f;
  bool opacitySet = false;

  std::string mixBlendMode = {};

  bool clipOverflow = false;

  // Lower-cased CSS `object-fit` keyword for replaced elements (currently only
  // `<img>`). Empty means "not set" and should fall back to the CSS default
  // `fill`, which corresponds to PAGX `ScaleMode::Stretch`.
  std::string objectFit = {};
};

}  // namespace pagx
