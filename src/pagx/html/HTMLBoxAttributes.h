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
