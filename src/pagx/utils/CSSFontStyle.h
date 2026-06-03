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
// resolve plus the synthetic (faux) axes it must emboss on top. This lets an importer bake the
// authored weight / slant into a `.pagx` without relying on render-time face introspection:
// uninstalled web faces (the common case for HTML / SVG imports such as "Noto Sans SC Black
// Italic") still render at the authored weight and slant via faux synthesis.
//
// The synthesised axes are *removed* from `fontStyleName` rather than kept alongside the faux
// flags. A host that does ship the real heavy / italic face must not be emboldened twice
// (faux-on-top-of-real, which the renderer layers additively), and keeping the styled name would
// trigger exactly that whenever the styled face is resolvable. Stripping it makes the rendered
// result identical whether or not the styled face is installed. Weights below the bold threshold
// (Thin / ExtraLight / Light / Medium) cannot be synthesised — faux only adds weight, never
// removes it — so those keywords are preserved in `fontStyleName` and carry no faux flag.
//
// Examples (threshold mirrors CSS bold synthesis at weight >= 600):
//   ("900", "italic") -> {fontStyleName: "",       fauxBold: true,  fauxItalic: true}
//   ("700", "")       -> {fontStyleName: "",       fauxBold: true,  fauxItalic: false}
//   ("600", "italic") -> {fontStyleName: "",       fauxBold: true,  fauxItalic: true}
//   ("500", "italic") -> {fontStyleName: "Medium", fauxBold: false, fauxItalic: true}
//   ("300", "")       -> {fontStyleName: "Light",  fauxBold: false, fauxItalic: false}
//   ("400", "italic") -> {fontStyleName: "",       fauxBold: false, fauxItalic: true}
//   ("400", "")       -> {fontStyleName: "",       fauxBold: false, fauxItalic: false}
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
