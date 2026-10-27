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

namespace pagx {

// Resolves a CSS font-weight string and a CSS font-style string into the PAGX `fontStyle`
// attribute value. Supported font-weight inputs are numeric values (1-1000) and the
// keywords `normal`, `bold`, `bolder`, `lighter`. Non-standard numeric weights are
// rounded to the nearest multiple of 100. Regular weight (400) and `normal` produce an
// empty weight portion. Italic and oblique styles are folded onto the result.
//
// Examples:
//   ("900", "italic")  -> "Black Italic"
//   ("700", "")        -> "Bold"
//   ("500", "italic")  -> "Medium Italic"
//   ("400", "")        -> ""
//   ("normal", "italic") -> "Italic"
std::string ResolveFontStyleName(const std::string& cssFontWeight, const std::string& cssFontStyle);

// Splits a CSS font-weight / font-style request into the real face style the renderer should
// resolve plus the synthetic (faux) italic axis it may emboss on top.
//
// Both the weight and slant axes are emitted as real-face style labels (Bold / SemiBold / Black /
// Italic / Medium Italic ... per the numeric weight rounded to the nearest hundred; 400 leaves the
// weight portion empty), never as a faux-bold flag. This lets the renderer resolve the authored
// face when it is installed or embedded, preserving the distinction between SemiBold, Bold and
// Black. If the requested face is unavailable, normal font lookup fallback applies without faux
// emboldening; the importer favours precise real-face selection over synthesising a missing weight.
//
// Italic additionally keeps `fauxItalic` true as a synthesis fallback: an oblique slant can be
// synthesised on top of any upright face, so it survives even when the styled italic face is
// unavailable. Text layout drops the flag when the resolved typeface already provides a real
// italic face, so the slant is never applied twice.
//
// Examples:
//   ("900", "italic") -> {fontStyleName: "Black Italic",    fauxBold: false, fauxItalic: true}
//   ("700", "")       -> {fontStyleName: "Bold",            fauxBold: false, fauxItalic: false}
//   ("600", "italic") -> {fontStyleName: "SemiBold Italic", fauxBold: false, fauxItalic: true}
//   ("500", "italic") -> {fontStyleName: "Medium Italic",   fauxBold: false, fauxItalic: true}
//   ("300", "")       -> {fontStyleName: "Light",           fauxBold: false, fauxItalic: false}
//   ("400", "italic") -> {fontStyleName: "Italic",          fauxBold: false, fauxItalic: true}
//   ("400", "")       -> {fontStyleName: "",                fauxBold: false, fauxItalic: false}
struct FontStyleSynthesis {
  std::string fontStyleName = {};
  bool fauxBold = false;
  bool fauxItalic = false;
};
FontStyleSynthesis ResolveFontStyleSynthesis(const std::string& cssFontWeight,
                                             const std::string& cssFontStyle);

// Parsed view of a PAGX/CSS font-style label.
struct ParsedFontStyle {
  // Numeric weight on the CSS scale (100..900). 400 means Regular when the input did not
  // mention a weight token.
  int weight = 400;
  // True when the input contains `italic` or `oblique` tokens.
  bool italic = false;
};

// Parses a PAGX `fontStyle` attribute value (e.g. "Bold Italic", "Black", "Italic", "Regular",
// or "") into a numeric weight + italic flag. Tokens are whitespace separated and
// case-insensitive. Recognised weight keywords are Thin / ExtraLight / Light / Regular(Normal) /
// Medium / SemiBold / Bold / ExtraBold / Black. Bare numeric weights such as "700" are accepted
// and clamped to [100, 900]. Unknown tokens are silently ignored. An empty input yields
// {weight=400, italic=false}.
ParsedFontStyle ParseFontStyleName(const std::string& fontStyleName);

}  // namespace pagx
