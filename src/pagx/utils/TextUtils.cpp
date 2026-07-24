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

#include "pagx/utils/TextUtils.h"
#include <cmath>
#include <cstdio>
#include "base/utils/MathUtil.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/PathData.h"

namespace pagx {

const TextBox* FindModifierTextBox(const std::vector<Element*>& elements) {
  for (auto* e : elements) {
    if (e->nodeType() == NodeType::TextBox) {
      auto* tb = static_cast<const TextBox*>(e);
      if (tb->elements.empty()) {
        return tb;
      }
    }
  }
  return nullptr;
}

namespace {

struct PositionedGlyph {
  const Glyph* glyph = nullptr;
  Matrix transform;
};

// Resolves the layer-space pen position for glyph `i` of `run`. The GlyphRun
// exposes three positioning channels that take precedence in the order below;
// only the highest-priority channel present for this glyph contributes.
//   1. `positions[i]` — absolute offset from the run origin. When paired with
//      `xOffsets[i]` the x-offset is additive on top.
//   2. `xOffsets[i]` — absolute x offset from the run origin (y stays on the
//      run baseline).
//   3. Advance-accumulator fallback `currentX` — each glyph's pen position is
//      the sum of all prior glyph advances in the same run.
// `baseX`/`baseY` are the run origin (textPos + run->x / run->y) pre-hoisted
// by WalkGlyphs so we don't re-add them per glyph.
static void ResolveGlyphPosition(const GlyphRun* run, size_t i, float baseX, float baseY,
                                 float currentX, float* posX, float* posY) {
  if (i < run->positions.size()) {
    *posX = baseX + run->positions[i].x;
    *posY = baseY + run->positions[i].y;
    if (i < run->xOffsets.size()) {
      *posX += run->xOffsets[i];
    }
  } else if (i < run->xOffsets.size()) {
    *posX = baseX + run->xOffsets[i];
    *posY = baseY;
  } else {
    *posX = currentX;
    *posY = baseY;
  }
}

// Builds the layer-space transform for a single glyph, mirroring
// GlyphRunRenderer::BuildTextBlob: the font scale is applied first, then the
// pen translation, then (only if any of rotation/skew/glyph-scale is non-trivial)
// the rotation/skew/scale are composed around T(pos)*T(anchor). Anchor lives in
// user (post-font-scale) units. Matching this exact order is what keeps scaled
// glyphs from collapsing onto each other in PPT/SVG output.
static Matrix ComposeGlyphMatrix(const Glyph* glyph, const GlyphRun* run, size_t i, float scale,
                                 float posX, float posY) {
  Matrix glyphMatrix = Matrix::Scale(scale, scale);
  glyphMatrix = Matrix::Translate(posX, posY) * glyphMatrix;

  bool hasRotation = i < run->rotations.size() && run->rotations[i] != 0;
  bool hasGlyphScale = i < run->scales.size() && (run->scales[i].x != 1 || run->scales[i].y != 1);
  bool hasSkew = i < run->skews.size() && run->skews[i] != 0;

  if (!hasRotation && !hasGlyphScale && !hasSkew) {
    return glyphMatrix;
  }

  float anchorX = glyph->advance * 0.5f * scale;
  float anchorY = 0;
  if (i < run->anchors.size()) {
    anchorX += run->anchors[i].x;
    anchorY += run->anchors[i].y;
  }

  glyphMatrix = Matrix::Translate(anchorX, anchorY) * glyphMatrix;
  if (hasRotation) {
    glyphMatrix = Matrix::Rotate(run->rotations[i]) * glyphMatrix;
  }
  if (hasSkew) {
    // Match GlyphRunRenderer::BuildTextBlob: skewX = -tan(angle). The
    // sign is what makes positive skew tilt the glyph forward (top-right)
    // once it's combined with the ascender-negative-Y glyph path coords.
    Matrix shear = {};
    shear.c = -std::tan(pag::DegreesToRadians(run->skews[i]));
    glyphMatrix = shear * glyphMatrix;
  }
  if (hasGlyphScale) {
    glyphMatrix = Matrix::Scale(run->scales[i].x, run->scales[i].y) * glyphMatrix;
  }
  glyphMatrix = Matrix::Translate(-anchorX, -anchorY) * glyphMatrix;
  return glyphMatrix;
}

// Shared per-glyph walker — both ComputeGlyphPaths (vector outlines) and
// ComputeGlyphImages (bitmap glyphs) need exactly the same positioning logic
// (positions / xOffsets / per-glyph anchor + rotation / skew / scale) and only
// differ in which glyph kind they emit. Keeping the math in one place ensures
// path glyphs and image glyphs in the same GlyphRun stay perfectly aligned.
//
// Returns every non-zero glyph in document order with the layer-space glyph
// matrix already composed. Callers filter for the kind they need (path vs.
// image) instead of the walker knowing about either.
static std::vector<PositionedGlyph> WalkGlyphs(const Text& text, float textPosX, float textPosY) {
  std::vector<PositionedGlyph> result;
  size_t totalGlyphs = 0;
  for (const auto* run : text.glyphRuns) {
    totalGlyphs += run->glyphs.size();
  }
  result.reserve(totalGlyphs);
  for (const auto* run : text.glyphRuns) {
    if (!run->font || run->font->unitsPerEm <= 0 || run->glyphs.empty()) {
      continue;
    }
    float scale = run->fontSize / static_cast<float>(run->font->unitsPerEm);
    float baseX = textPosX + run->x;
    float baseY = textPosY + run->y;
    float currentX = baseX;
    for (size_t i = 0; i < run->glyphs.size(); i++) {
      uint16_t glyphID = run->glyphs[i];
      if (glyphID == 0) {
        continue;
      }
      auto glyphIndex = static_cast<size_t>(glyphID) - 1;
      if (glyphIndex >= run->font->glyphs.size()) {
        continue;
      }
      auto* glyph = run->font->glyphs[glyphIndex];
      if (!glyph) {
        continue;
      }

      float posX = 0;
      float posY = 0;
      ResolveGlyphPosition(run, i, baseX, baseY, currentX, &posX, &posY);
      // Advance the cursor for the no-positions / no-xOffsets fallback path
      // even when this particular glyph is later filtered out, so subsequent
      // glyphs land where the renderer expects them.
      currentX += glyph->advance * scale;

      result.push_back({glyph, ComposeGlyphMatrix(glyph, run, i, scale, posX, posY)});
    }
  }
  return result;
}

// Turns a PositionedGlyph into a GlyphImage entry if the glyph carries a
// bitmap, applying the design-space offset so pixel (0,0) maps to the image's
// top-left in layer space. Kept separate from the walker so ComputeGlyphImages
// and the combined API share identical image-placement math.
static bool TryMakeGlyphImage(const PositionedGlyph& entry, GlyphImage* out) {
  const auto* glyph = entry.glyph;
  if (!glyph->image) {
    return false;
  }
  Matrix imageMatrix = entry.transform;
  if (glyph->offset.x != 0 || glyph->offset.y != 0) {
    imageMatrix = imageMatrix * Matrix::Translate(glyph->offset.x, glyph->offset.y);
  }
  *out = {imageMatrix, glyph->image};
  return true;
}

static bool TryMakeGlyphPath(const PositionedGlyph& entry, GlyphPath* out) {
  if (!entry.glyph->path || entry.glyph->path->isEmpty()) {
    return false;
  }
  *out = {entry.transform, entry.glyph->path};
  return true;
}

}  // namespace

void ComputeGlyphPathsAndImages(const Text& text, float textPosX, float textPosY,
                                std::vector<GlyphPath>* paths, std::vector<GlyphImage>* images) {
  if (paths == nullptr && images == nullptr) {
    return;
  }
  auto positioned = WalkGlyphs(text, textPosX, textPosY);
  if (paths) {
    paths->reserve(paths->size() + positioned.size());
  }
  if (images) {
    images->reserve(images->size() + positioned.size());
  }
  for (const auto& entry : positioned) {
    if (paths) {
      GlyphPath path = {};
      if (TryMakeGlyphPath(entry, &path)) {
        paths->push_back(path);
      }
    }
    if (images) {
      GlyphImage image = {};
      if (TryMakeGlyphImage(entry, &image)) {
        images->push_back(image);
      }
    }
  }
}

std::vector<GlyphPath> ComputeGlyphPaths(const Text& text, float textPosX, float textPosY) {
  std::vector<GlyphPath> result;
  ComputeGlyphPathsAndImages(text, textPosX, textPosY, &result, nullptr);
  return result;
}

std::vector<GlyphImage> ComputeGlyphImages(const Text& text, float textPosX, float textPosY) {
  std::vector<GlyphImage> result;
  ComputeGlyphPathsAndImages(text, textPosX, textPosY, nullptr, &result);
  return result;
}

bool HasNonASCII(const std::string& str) {
  for (unsigned char c : str) {
    if (c > 127) {
      return true;
    }
  }
  return false;
}

namespace {

// Visitor used by DetectTextLang. Stores the first non-ASCII script tag
// encountered so the caller can return the early-exit result. Defined as a
// struct (not a lambda) per project convention.
struct DetectLangVisitor {
  const char* result = nullptr;
  bool operator()(uint32_t cp) {
    if ((cp >= 0x3040 && cp <= 0x30FF) ||    // Hiragana + Katakana
        (cp >= 0x3400 && cp <= 0x4DBF) ||    // CJK Extension A
        (cp >= 0x4E00 && cp <= 0x9FFF) ||    // CJK Unified Ideographs
        (cp >= 0xAC00 && cp <= 0xD7A3) ||    // Hangul Syllables
        (cp >= 0xF900 && cp <= 0xFAFF) ||    // CJK Compatibility Ideographs
        (cp >= 0xFF00 && cp <= 0xFFEF) ||    // Halfwidth / fullwidth forms
        (cp >= 0x20000 && cp <= 0x2FFFF)) {  // CJK Extensions B..F + suppl.
      result = "zh-CN";
      return false;
    }
    if (cp >= 0x0590 && cp <= 0x05FF) {  // Hebrew
      result = "he-IL";
      return false;
    }
    if ((cp >= 0x0600 && cp <= 0x06FF) ||  // Arabic
        (cp >= 0x0750 && cp <= 0x077F) ||  // Arabic Supplement
        (cp >= 0x08A0 && cp <= 0x08FF) ||  // Arabic Extended-A
        (cp >= 0xFB50 && cp <= 0xFDFF) ||  // Arabic Presentation Forms-A
        (cp >= 0xFE70 && cp <= 0xFEFF)) {  // Arabic Presentation Forms-B
      result = "ar-SA";
      return false;
    }
    return true;
  }
};

}  // namespace

// Heuristic classification of a text run into an OOXML xml:lang tag. PowerPoint
// paints a spellcheck squiggle under any run whose declared language doesn't
// match its glyphs, so emitting "en-US" for CJK/Hebrew/Arabic text yields
// visible red underlines on a correctly rendered document. We walk the UTF-8
// bytes and pick the first non-ASCII script encountered: CJK -> "zh-CN",
// Hebrew -> "he-IL", Arabic -> "ar-SA"; runs with no recognised non-ASCII
// script stay on "en-US", matching PowerPoint's own default.
std::string DetectTextLang(const std::string& utf8) {
  DetectLangVisitor visitor;
  IterateUTF8Codepoints(utf8, visitor);
  return visitor.result ? std::string(visitor.result) : "en-US";
}

namespace {

// Visitor used by HasRTLParagraphBase. Reports the first strong-directional
// character it sees through `result` (true = RTL, false = LTR) and stops the
// walk. Codepoints with weak / neutral directionality leave `decided` false so
// the caller falls back to the default LTR.
struct ParagraphBaseDirectionVisitor {
  bool decided = false;
  bool result = false;
  bool operator()(uint32_t cp) {
    // Strong RTL: Hebrew, Arabic, Syriac, Thaana, NKo, and the Arabic
    // presentation forms. Covers the common cases the PAGX exporter sees.
    if ((cp >= 0x0590 && cp <= 0x05FF) ||  // Hebrew
        (cp >= 0x0600 && cp <= 0x06FF) ||  // Arabic
        (cp >= 0x0700 && cp <= 0x074F) ||  // Syriac
        (cp >= 0x0750 && cp <= 0x077F) ||  // Arabic Supplement
        (cp >= 0x0780 && cp <= 0x07BF) ||  // Thaana
        (cp >= 0x07C0 && cp <= 0x07FF) ||  // NKo
        (cp >= 0x08A0 && cp <= 0x08FF) ||  // Arabic Extended-A
        (cp >= 0xFB1D && cp <= 0xFDFF) ||  // Hebrew / Arabic Presentation Forms-A
        (cp >= 0xFE70 && cp <= 0xFEFF)) {  // Arabic Presentation Forms-B
      decided = true;
      result = true;
      return false;
    }
    // Strong LTR: ASCII letters, Latin-1 letters, Greek / Cyrillic / Armenian,
    // and the common CJK blocks reused above. Anything identified here halts
    // the scan with an LTR result.
    if ((cp >= 0x0041 && cp <= 0x005A) ||    // A-Z
        (cp >= 0x0061 && cp <= 0x007A) ||    // a-z
        (cp >= 0x00C0 && cp <= 0x024F) ||    // Latin-1 Suppl + Extended-A/B
        (cp >= 0x0370 && cp <= 0x03FF) ||    // Greek
        (cp >= 0x0400 && cp <= 0x04FF) ||    // Cyrillic
        (cp >= 0x0530 && cp <= 0x058F) ||    // Armenian
        (cp >= 0x3040 && cp <= 0x30FF) ||    // Hiragana + Katakana
        (cp >= 0x3400 && cp <= 0x4DBF) ||    // CJK Extension A
        (cp >= 0x4E00 && cp <= 0x9FFF) ||    // CJK Unified Ideographs
        (cp >= 0xAC00 && cp <= 0xD7A3) ||    // Hangul Syllables
        (cp >= 0xF900 && cp <= 0xFAFF) ||    // CJK Compatibility Ideographs
        (cp >= 0x20000 && cp <= 0x2FFFF)) {  // CJK Extensions B..F + suppl.
      decided = true;
      result = false;
      return false;
    }
    return true;
  }
};

}  // namespace

// Paragraph base direction per UBA rules P2/P3: scan code points in order,
// take the direction of the first strong-directional character. Hebrew and
// Arabic code points in the RTL blocks are strong-R (or strong-AL for Arabic).
// ASCII letters and CJK are strong-L. Everything else (digits, punctuation,
// whitespace) is weak/neutral and does not determine the base direction. If
// no strong character is found the paragraph defaults to LTR.
bool HasRTLParagraphBase(const std::string& utf8) {
  ParagraphBaseDirectionVisitor visitor;
  IterateUTF8Codepoints(utf8, visitor);
  return visitor.decided ? visitor.result : false;
}

std::string UTF8ToUTF16BEHex(const std::string& utf8) {
  std::string hex;
  hex.reserve(utf8.size() * 6);
  size_t i = 0;
  while (i < utf8.size()) {
    uint32_t cp = 0;
    auto c = static_cast<unsigned char>(utf8[i]);
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
    for (size_t j = 1; j < bytes && (i + j) < utf8.size(); j++) {
      cp = (cp << 6) | (static_cast<unsigned char>(utf8[i + j]) & 0x3Fu);
    }
    i += bytes;
    char buf[9];
    if (cp <= 0xFFFF) {
      snprintf(buf, sizeof(buf), "%04X", cp);
    } else {
      cp -= 0x10000;
      snprintf(buf, sizeof(buf), "%04X%04X", 0xD800 + (cp >> 10), 0xDC00 + (cp & 0x3FF));
    }
    hex += buf;
  }
  return hex;
}

float EffectiveTextBoxWidth(const TextBox* box) {
  if (!std::isnan(box->width)) {
    return box->width;
  }
  // Use the layout-resolved width (set by the constraint layout pass) so TextBoxes
  // sized via left/right/centerX still wrap and lay out as the author intended.
  float resolved = box->layoutBounds().width;
  return resolved > 0 ? resolved : NAN;
}

float EffectiveTextBoxHeight(const TextBox* box) {
  if (!std::isnan(box->height)) {
    return box->height;
  }
  float resolved = box->layoutBounds().height;
  return resolved > 0 ? resolved : NAN;
}

TextLayoutParams MakeTextBoxParams(const TextBox* box) {
  bool hasPadding = !box->padding.isZero();
  float boxW = EffectiveTextBoxWidth(box);
  float boxH = EffectiveTextBoxHeight(box);
  if (hasPadding) {
    if (!std::isnan(boxW)) {
      boxW = std::max(0.0f, boxW - box->padding.left - box->padding.right);
    }
    if (!std::isnan(boxH)) {
      boxH = std::max(0.0f, boxH - box->padding.top - box->padding.bottom);
    }
  }
  TextLayoutParams params = {};
  params.boxWidth = boxW;
  params.boxHeight = boxH;
  params.textAlign = box->textAlign;
  params.paragraphAlign = box->paragraphAlign;
  params.writingMode = box->writingMode;
  params.lineHeight = box->lineHeight;
  params.wordWrap = box->wordWrap;
  params.overflow = box->overflow;
  return params;
}

TextLayoutParams MakeStandaloneParams(const Text* text) {
  TextLayoutParams params = {};
  params.baseline = text->baseline;
  switch (text->textAnchor) {
    case TextAnchor::Start:
      params.textAlign = TextAlign::Start;
      break;
    case TextAnchor::Center:
      params.textAlign = TextAlign::Center;
      break;
    case TextAnchor::End:
      params.textAlign = TextAlign::End;
      break;
  }
  // Forward the textScale applied by LayoutNode::setLayoutSize when fitting the Text into a
  // dual-axis constraint (e.g. left+right, width="100%"). Without this, re-running TextLayout
  // here uses the authored fontSize while WriteSharedTextAttrs emits the scaled renderFontSize(),
  // so baselineY/lineWidth are computed at the wrong scale and the exported text overflows its
  // container. renderFontSize() is the public mirror of fontSize*textScale; derive the scale from
  // it so we don't need to expose Text::textScale.
  if (text->fontSize > 0.0f) {
    params.textScale = text->renderFontSize() / text->fontSize;
  }
  return params;
}

TextGlyphParams MakeGlyphParams(const Text* text) {
  TextGlyphParams params = {};
  params.text = text->text;
  params.fontFamily = text->fontFamily;
  params.fontStyle = text->fontStyle;
  params.fontSize = text->fontSize;
  params.letterSpacing = text->letterSpacing;
  params.fauxBold = text->fauxBold;
  params.fauxItalic = text->fauxItalic;
  return params;
}

std::string StripQuotes(const std::string& s) {
  if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
    return s.substr(1, s.size() - 2);
  }
  return s;
}

}  // namespace pagx
