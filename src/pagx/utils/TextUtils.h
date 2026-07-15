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

#include <cstdint>
#include <string>
#include <vector>
#include "pagx/TextGlyphParams.h"
#include "pagx/TextLayoutParams.h"
#include "pagx/nodes/Element.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/types/Matrix.h"
#include "pagx/utils/ExporterUtils.h"

namespace pagx {

class Image;
class PathData;

struct GlyphPath {
  Matrix transform;
  const PathData* pathData = nullptr;
};

struct GlyphImage {
  // Maps the image's pixel-coord rect (0,0,imageWidth,imageHeight) to
  // layer-space coords. The glyph's design-space `offset` is already baked into
  // this matrix so callers don't need to know about it.
  Matrix transform;
  const Image* image = nullptr;
};

/**
 * Returns the first modifier-only <TextBox> in `elements` (i.e. a TextBox with
 * empty `elements`) so its layout / typography parameters can be associated
 * with sibling Text geometry. Container TextBoxes (those with children) are
 * handled separately as scopes by the writers' processVectorScope walks and
 * are NOT returned by this helper. Mirrors the legacy
 * CollectFillStroke().textBox rule that both SVG and PPT exporters share.
 */
const TextBox* FindModifierTextBox(const std::vector<Element*>& elements);

/**
 * Walks `elements` in source order and pairs every Text with its nearest
 * enclosing Fill / Stroke painter. Direct Text children inherit `parentFill` /
 * `parentStroke`; Texts nested inside a Group use that Group's locally-collected
 * Fill / Stroke (falling back to the parent's when the Group supplies none).
 *
 * The Run type must be aggregate-initialisable from `{const Text*, const Fill*,
 * const Stroke*}` (e.g. `pagx::PPTWriter::RichTextRun` and the SVG-side
 * `SVGRichTextRun`). Using a template lets both exporters share the same walk
 * without forcing them to share a struct definition with unrelated members.
 */
template <typename Run>
void CollectRichTextRuns(const std::vector<Element*>& elements, const Fill* parentFill,
                         const Stroke* parentStroke, std::vector<Run>& outRuns) {
  for (auto* element : elements) {
    auto type = element->nodeType();
    if (type == NodeType::Text) {
      auto* t = static_cast<const Text*>(element);
      if (!t->text.empty()) {
        outRuns.push_back({t, parentFill, parentStroke});
      }
    } else if (type == NodeType::Group) {
      auto* g = static_cast<const Group*>(element);
      auto groupFs = CollectFillStroke(g->elements);
      const Fill* effectiveFill = groupFs.fill ? groupFs.fill : parentFill;
      const Stroke* effectiveStroke = groupFs.stroke ? groupFs.stroke : parentStroke;
      CollectRichTextRuns(g->elements, effectiveFill, effectiveStroke, outRuns);
    }
  }
}

/**
 * Stable-sort comparator for any line-entry struct that exposes a `baselineY`
 * member. Both SVG and PPT use this to sort entries before grouping by visual
 * line; defining it as a named function (not a lambda) per project convention.
 */
template <typename Entry>
bool LineEntryBaselineYLess(const Entry& a, const Entry& b) {
  return a.baselineY < b.baselineY;
}

/**
 * Converts text glyph runs into a list of vector glyph paths with transform matrices.
 * Image-only glyphs are skipped — use ComputeGlyphImages for those.
 * textPosX/textPosY are the base position for glyph placement.
 */
std::vector<GlyphPath> ComputeGlyphPaths(const Text& text, float textPosX, float textPosY);

/**
 * Converts text glyph runs into a list of bitmap glyphs with transform matrices.
 * Vector glyphs (without an `image`) are skipped — use ComputeGlyphPaths for those.
 * textPosX/textPosY are the base position for glyph placement (same conventions
 * as ComputeGlyphPaths so vector and bitmap glyphs in the same run line up).
 */
std::vector<GlyphImage> ComputeGlyphImages(const Text& text, float textPosX, float textPosY);

/**
 * Computes both vector glyph paths and bitmap glyph images in a single traversal
 * of `text`. Equivalent to calling ComputeGlyphPaths and ComputeGlyphImages in
 * sequence, but walks the glyph runs once — use this when a caller needs both
 * kinds from the same text element.
 */
void ComputeGlyphPathsAndImages(const Text& text, float textPosX, float textPosY,
                                std::vector<GlyphPath>* paths, std::vector<GlyphImage>* images);

bool HasNonASCII(const std::string& str);

/**
 * Walks `utf8` codepoint by codepoint and invokes `visitor(uint32_t)` for each.
 * The visitor must return true to continue, false to stop. Skips trailing bytes
 * of a truncated multi-byte sequence (the source must end on a complete code
 * point boundary; if it does not, the trailing partial sequence is dropped).
 * Decoding logic is centralized here so DetectTextLang and HasRTLParagraphBase
 * share the same UTF-8 traversal — match their previous (loop-local) decoder
 * exactly, including the early-exit pattern callers expect. Visitor is taken by
 * forwarding reference so callers can pass either a named local or an rvalue
 * struct without wrapping; functor state stays observable through the lvalue
 * reference path used by the migrated callers.
 */
template <typename Visitor>
void IterateUTF8Codepoints(const std::string& utf8, Visitor&& visitor) {
  size_t i = 0;
  while (i < utf8.size()) {
    auto c = static_cast<unsigned char>(utf8[i]);
    uint32_t cp = 0;
    size_t bytes = 1;
    if (c < 0x80) {
      cp = c;
    } else if (c < 0xE0) {
      cp = c & 0x1Fu;
      bytes = 2;
    } else if (c < 0xF0) {
      cp = c & 0x0Fu;
      bytes = 3;
    } else {
      cp = c & 0x07u;
      bytes = 4;
    }
    if (i + bytes > utf8.size()) {
      break;
    }
    for (size_t k = 1; k < bytes; k++) {
      cp = (cp << 6) | (static_cast<unsigned char>(utf8[i + k]) & 0x3Fu);
    }
    i += bytes;
    if (!visitor(cp)) {
      return;
    }
  }
}

/**
 * Classifies a UTF-8 text run into an OOXML xml:lang tag. Returns "zh-CN" for
 * runs containing CJK code points, "he-IL" / "ar-SA" for runs containing
 * Hebrew / Arabic code points, and "en-US" otherwise. Scans in source order and
 * returns the first non-ASCII script encountered so mixed-script runs still pick
 * up a reasonable dominant language. PowerPoint paints a spellcheck squiggle
 * under any run whose declared language doesn't match its glyphs, so this
 * heuristic keeps the common cases free of red underlines without introducing a
 * dependency on a real language detector.
 */
std::string DetectTextLang(const std::string& utf8);

/**
 * Returns true when the UTF-8 text's paragraph base direction is right-to-left
 * per the Unicode Bidirectional Algorithm rules P2/P3: the direction is taken
 * from the first strong directional character (R, AL, or L). Text that contains
 * no strong directional character returns false (default LTR). Used by the PPT
 * exporter to emit pPr@rtl so PowerPoint runs UBA with the correct base
 * direction and reproduces the same visual ordering as the PAGX renderer.
 */
bool HasRTLParagraphBase(const std::string& utf8);

std::string UTF8ToUTF16BEHex(const std::string& utf8);

/**
 * Returns the effective width/height of a TextBox for typesetting and shape sizing.
 * Falls back to the layout-resolved dimensions when no explicit width/height was supplied
 * (e.g. when the TextBox is positioned via left/right/top/bottom constraints), and returns
 * NaN when neither an explicit nor a resolved value is available (auto-sized in that axis).
 */
float EffectiveTextBoxWidth(const TextBox* box);
float EffectiveTextBoxHeight(const TextBox* box);

/**
 * Builds TextLayoutParams from a TextBox's attributes with padding-adjusted content dimensions.
 */
TextLayoutParams MakeTextBoxParams(const TextBox* box);

/**
 * Builds TextLayoutParams from a standalone Text element's attributes.
 */
TextLayoutParams MakeStandaloneParams(const Text* text);

/**
 * Builds per-Text shaping attributes (text content, font, size, spacing, faux flags) from a Text
 * element's document fields. Used to feed the shaper without reading the Text node directly.
 */
TextGlyphParams MakeGlyphParams(const Text* text);

/**
 * Logical Start / End describe the *paragraph-relative* edges; the physical
 * visual edge a renderer should hit depends on the paragraph base direction
 * (UBA P2/P3). For RTL paragraphs Start sits on the visual right edge and End
 * on the visual left, so swap the two before mapping to the writer-specific
 * anchor / alignment representation. Center and Justify are direction-symmetric.
 * Shared by both the SVG and PPT exporters so logical-to-physical mapping
 * stays consistent across formats.
 */
inline TextAnchor ResolveLogicalAnchor(TextAnchor logical, bool rtl) {
  if (!rtl) {
    return logical;
  }
  if (logical == TextAnchor::Start) {
    return TextAnchor::End;
  }
  if (logical == TextAnchor::End) {
    return TextAnchor::Start;
  }
  return logical;
}

inline TextAlign ResolveLogicalAlign(TextAlign logical, bool rtl) {
  if (!rtl) {
    return logical;
  }
  if (logical == TextAlign::Start) {
    return TextAlign::End;
  }
  if (logical == TextAlign::End) {
    return TextAlign::Start;
  }
  return logical;
}

/**
 * Strips surrounding double-quote characters from a string.
 */
std::string StripQuotes(const std::string& s);

/**
 * Builds the styled typeface name shared by SVG (`font-family` attribute) and
 * PPT (`<a:latin>` typeface) — strips quotes and appends " Bold", " Italic", or
 * " Bold Italic" when `fontStyle` declares a real Bold / Italic face. Returns
 * an empty string when `fontFamily` is empty so callers can skip emission. The
 * suffix lets PowerPoint and Safari/Chromium/CoreText resolve the styled family
 * directly to the matching real face instead of synthesising bold/italic on top
 * of the regular face — see the comments at the call sites for the trade-off
 * against fontconfig-based renderers (LibreOffice, librsvg, Inkscape on Linux).
 */
inline std::string BuildStyledTypeface(const std::string& fontFamily,
                                       const std::string& fontStyle) {
  if (fontFamily.empty()) {
    return std::string();
  }
  bool hasRealBold = fontStyle.find("Bold") != std::string::npos;
  bool hasRealItalic = fontStyle.find("Italic") != std::string::npos;
  std::string typeface = StripQuotes(fontFamily);
  if (hasRealBold && hasRealItalic) {
    typeface += " Bold Italic";
  } else if (hasRealBold) {
    typeface += " Bold";
  } else if (hasRealItalic) {
    typeface += " Italic";
  }
  return typeface;
}

}  // namespace pagx
