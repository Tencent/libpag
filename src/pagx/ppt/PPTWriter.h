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

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include "base/utils/MathUtil.h"
#include "pagx/LayoutContext.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PPTExporter.h"
#include "pagx/TextLayout.h"
#include "pagx/nodes/BlendFilter.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/ColorStop.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/InnerShadowFilter.h"
#include "pagx/nodes/InnerShadowStyle.h"
#include "pagx/nodes/LayerStyle.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/ppt/PPTContourUtils.h"
#include "pagx/ppt/PPTGeomEmitter.h"
#include "pagx/ppt/PPTModifierResolver.h"
#include "pagx/ppt/PPTWriterContext.h"
#include "pagx/types/Rect.h"
#include "pagx/utils/ExporterUtils.h"
#include "pagx/xml/XMLBuilder.h"
#include "renderer/LayerBuilder.h"

namespace pagx {

//==============================================================================
// Shared coordinate / color / angle helpers. Header-local inlines so that each
// translation unit (PPTExporter.cpp, PPTStyleEmitter.cpp, PPTTextWriter.cpp)
// compiles them without creating multi-definition conflicts.
//==============================================================================

inline std::string ColorToHex6(const Color& c) {
  std::string buf(6, '\0');
  std::snprintf(buf.data(), 7, "%02X%02X%02X",
                std::clamp(static_cast<int>(std::round(c.red * 255.0f)), 0, 255),
                std::clamp(static_cast<int>(std::round(c.green * 255.0f)), 0, 255),
                std::clamp(static_cast<int>(std::round(c.blue * 255.0f)), 0, 255));
  return buf;
}

inline int AlphaToPct(float alpha) {
  return std::clamp(static_cast<int>(std::round(alpha * 100000.0f)), 0, 100000);
}

inline int AngleToPPT(float degrees) {
  int v = static_cast<int>(std::round(degrees * 60000.0));
  return ((v % 21600000) + 21600000) % 21600000;
}

inline int FontSizeToPPT(float px) {
  return std::max(100, static_cast<int>(std::round(px * 75.0)));
}

// Emits an `<a:srgbClr val="..."/>` element, expanding into
// `<a:srgbClr val="..."><a:alpha val="..."/></a:srgbClr>` when the effective
// alpha is below 1.0. Centralizes the alpha-emission idiom shared by gradient
// stops, solid fills, and blend overlays.
inline void WriteSrgbClr(XMLBuilder& out, const Color& color, float effectiveAlpha) {
  out.openElement("a:srgbClr").addRequiredAttribute("val", ColorToHex6(color));
  if (effectiveAlpha < 1.0f) {
    out.closeElementStart();
    out.openElement("a:alpha")
        .addRequiredAttribute("val", AlphaToPct(effectiveAlpha))
        .closeElementSelfClosing();
    out.closeElement();  // a:srgbClr
  } else {
    out.closeElementSelfClosing();
  }
}

inline size_t CountUTF8Characters(const std::string& str) {
  size_t count = 0;
  for (size_t i = 0; i < str.size(); ++i) {
    auto byte = static_cast<unsigned char>(str[i]);
    if ((byte & 0xC0) != 0x80) {
      count++;
    }
  }
  return count;
}

//==============================================================================
// Stroke-alignment geometry compensation
//==============================================================================

// PowerPoint's <a:ln> always centres the stroke on the path geometry, so the
// only way to emulate StrokeAlign::Inside / StrokeAlign::Outside is to inset
// (or outset) the geometry that backs the stroke painter by half the stroke
// width before emitting it.  Returns the per-side offset to apply: positive
// shrinks the geometry (Inside), negative grows it (Outside), and zero leaves
// the geometry unchanged (Center, no stroke, or zero width).
inline float StrokeAlignInset(const Stroke* stroke) {
  if (stroke == nullptr || stroke->width <= 0) {
    return 0.0f;
  }
  switch (stroke->align) {
    case StrokeAlign::Inside:
      return stroke->width / 2.0f;
    case StrokeAlign::Outside:
      return -stroke->width / 2.0f;
    case StrokeAlign::Center:
    default:
      return 0.0f;
  }
}

// Applies the stroke-inset to an axis-aligned shape rect. Clamps the inset so
// the geometry never collapses past the centre (the OOXML stroke would draw
// against zero-extent geometry otherwise). `roundness` is also reduced so
// rounded-rectangle corners stay visually consistent after the inset.
inline void ApplyStrokeBoxInset(const Stroke* stroke, float& x, float& y, float& w, float& h,
                                float* roundness = nullptr) {
  float inset = StrokeAlignInset(stroke);
  if (inset == 0.0f) {
    return;
  }
  float maxInset = std::min(w, h) / 2.0f;
  if (inset > maxInset) {
    inset = maxInset;
  }
  x += inset;
  y += inset;
  w -= inset * 2.0f;
  h -= inset * 2.0f;
  if (roundness) {
    *roundness = std::max(0.0f, *roundness - inset);
  }
}

//==============================================================================
// Dash pattern -> OOXML preset dash mapping
//==============================================================================

// Maps dash-to-stroke-width ratios to OOXML preset dash types (ISO/IEC 29500-1,
// §20.1.10.48 ST_PresetLineDashVal). The spec does not pin the exact segment
// geometry for each preset, so we classify by the ratio of the on-segment (dr)
// and gap (sr) to the stroke width:
//   dr ≤ 1.5×  → "dot" family   (sysDot if sr ≤ 2×, dot otherwise)
//   dr ≤ 4.5×  → "dash" family  (sysDash if sr ≤ 2×, dash otherwise)
//   dr > 4.5×  → "lgDash"
// The 1.5×/4.5× split separates short ticks from long strokes (PowerPoint's
// own renderings of "dot" and "dash" differ roughly at this ratio). The 2×
// gap split distinguishes PowerPoint's compact "sys*" variants (tight spacing)
// from their looser counterparts. Dash-dot (n==4) and dash-dot-dot (n==6)
// reuse the 4.5× threshold to pick between short and large variants.
inline const char* MatchPresetDash(const std::vector<float>& dashes, float strokeWidth) {
  if (dashes.empty() || strokeWidth <= 0) {
    return nullptr;
  }
  size_t n = dashes.size();
  if (n == 2) {
    float dr = dashes[0] / strokeWidth;
    float sr = dashes[1] / strokeWidth;
    if (dr <= 1.5f) {
      return (sr <= 2.0f) ? "sysDot" : "dot";
    }
    if (dr <= 4.5f) {
      return (sr <= 2.0f) ? "sysDash" : "dash";
    }
    return "lgDash";
  }
  if (n == 4) {
    float dr = dashes[0] / strokeWidth;
    return (dr > 4.5f) ? "lgDashDot" : "dashDot";
  }
  if (n == 6) {
    float dr = dashes[0] / strokeWidth;
    return (dr > 4.5f) ? "lgDashDotDot" : "sysDashDotDot";
  }
  return nullptr;
}

//==============================================================================
// ImagePatternRect — shared visible-area / srcRect computation
//==============================================================================

struct ImagePatternRect {
  float visL = 0;
  float visT = 0;
  float visR = 0;
  float visB = 0;
  int srcL = 0;
  int srcT = 0;
  int srcR = 0;
  int srcB = 0;
};

// Maps a gradient center point through its matrix and emits OOXML's
// <a:fillToRect> in 1/1000-percent insets relative to shapeBounds. When
// fitsToGeometry is true the center is already in normalized (0-1) space and
// maps directly to relative insets; when false the center is in document space
// and must be divided by shapeBounds. When the shape bounds are unknown (e.g. a
// text fill where the bounds aren't computed at the run level) the focus
// collapses to the geometric centre.
template <typename Gradient>
inline void WriteFillToRectFromCenter(XMLBuilder& out, const Gradient* grad,
                                      const Rect& shapeBounds) {
  int l = 50000;
  int t = 50000;
  int r = 50000;
  int b = 50000;
  if (shapeBounds.width > 0 && shapeBounds.height > 0) {
    Point centerMapped = grad->matrix.mapPoint(grad->center);
    float relX;
    float relY;
    if (grad->fitsToGeometry) {
      relX = centerMapped.x;
      relY = centerMapped.y;
    } else {
      relX = (centerMapped.x - shapeBounds.x) / shapeBounds.width;
      relY = (centerMapped.y - shapeBounds.y) / shapeBounds.height;
    }
    l = std::clamp(static_cast<int>(std::round(relX * 100000.0f)), 0, 100000);
    t = std::clamp(static_cast<int>(std::round(relY * 100000.0f)), 0, 100000);
    r = 100000 - l;
    b = 100000 - t;
  }
  out.openElement("a:fillToRect")
      .addRequiredAttribute("l", l)
      .addRequiredAttribute("t", t)
      .addRequiredAttribute("r", r)
      .addRequiredAttribute("b", b)
      .closeElementSelfClosing();
}

// Adds non-zero l/t/r/b inset attributes (in OOXML's 1/1000-percent units) to
// the currently-open element. Used by both <a:srcRect> and <a:fillRect> in
// image-pattern fills, where omitting a zero inset keeps the OOXML tidy.
struct LTRBInsets {
  int l = 0;
  int t = 0;
  int r = 0;
  int b = 0;
  bool any() const {
    return l != 0 || t != 0 || r != 0 || b != 0;
  }
};

inline void AddLTRBAttrs(XMLBuilder& out, const LTRBInsets& insets) {
  if (insets.l != 0) {
    out.addRequiredAttribute("l", insets.l);
  }
  if (insets.t != 0) {
    out.addRequiredAttribute("t", insets.t);
  }
  if (insets.r != 0) {
    out.addRequiredAttribute("r", insets.r);
  }
  if (insets.b != 0) {
    out.addRequiredAttribute("b", insets.b);
  }
}

// Computes the axis-aligned document rect occupied by the image after the
// pattern's matrix transform and scaleMode fit are applied. Returns false when
// the matrix has rotation/skew or negative scale — those cases have no faithful
// <p:pic>/<a:srcRect> representation and must fall back to a raster bake.
inline bool ComputePlacedImageRect(const ImagePattern* pattern, int imgW, int imgH,
                                   const Rect& shapeBounds, Rect* result) {
  const auto& M = pattern->matrix;
  if (std::fabs(M.b) > pag::FLOAT_NEARLY_ZERO || std::fabs(M.c) > pag::FLOAT_NEARLY_ZERO) {
    return false;
  }
  if (M.a <= 0 || M.d <= 0) {
    return false;
  }
  float transformedW = static_cast<float>(imgW) * M.a;
  float transformedH = static_cast<float>(imgH) * M.d;
  if (transformedW <= 0 || transformedH <= 0) {
    return false;
  }

  if (pattern->scaleMode == ScaleMode::None) {
    result->x = M.tx;
    result->y = M.ty;
    result->width = transformedW;
    result->height = transformedH;
    return true;
  }

  if (shapeBounds.isEmpty()) {
    return false;
  }

  float sx = shapeBounds.width / transformedW;
  float sy = shapeBounds.height / transformedH;
  switch (pattern->scaleMode) {
    case ScaleMode::Stretch:
      result->x = shapeBounds.x;
      result->y = shapeBounds.y;
      result->width = shapeBounds.width;
      result->height = shapeBounds.height;
      return true;
    case ScaleMode::LetterBox: {
      float scale = std::min(sx, sy);
      result->width = transformedW * scale;
      result->height = transformedH * scale;
      result->x = shapeBounds.x + (shapeBounds.width - result->width) / 2.0f;
      result->y = shapeBounds.y + (shapeBounds.height - result->height) / 2.0f;
      return true;
    }
    case ScaleMode::Zoom: {
      float scale = std::max(sx, sy);
      result->width = transformedW * scale;
      result->height = transformedH * scale;
      result->x = shapeBounds.x + (shapeBounds.width - result->width) / 2.0f;
      result->y = shapeBounds.y + (shapeBounds.height - result->height) / 2.0f;
      return true;
    }
    case ScaleMode::None:
      break;
  }
  return false;
}

inline bool ComputeImagePatternRect(const ImagePattern* pattern, int imgW, int imgH,
                                    const Rect& shapeBounds, ImagePatternRect* result) {
  Rect imageRect = {};
  if (!ComputePlacedImageRect(pattern, imgW, imgH, shapeBounds, &imageRect)) {
    return false;
  }
  if (imageRect.width <= 0 || imageRect.height <= 0) {
    return false;
  }
  result->visL = std::max(shapeBounds.x, imageRect.x);
  result->visT = std::max(shapeBounds.y, imageRect.y);
  result->visR = std::min(shapeBounds.x + shapeBounds.width, imageRect.x + imageRect.width);
  result->visB = std::min(shapeBounds.y + shapeBounds.height, imageRect.y + imageRect.height);
  if (result->visR <= result->visL || result->visB <= result->visT) {
    return false;
  }
  result->srcL =
      static_cast<int>(std::round((result->visL - imageRect.x) / imageRect.width * 100000.0f));
  result->srcT =
      static_cast<int>(std::round((result->visT - imageRect.y) / imageRect.height * 100000.0f));
  result->srcR = static_cast<int>(
      std::round((imageRect.x + imageRect.width - result->visR) / imageRect.width * 100000.0f));
  result->srcB = static_cast<int>(
      std::round((imageRect.y + imageRect.height - result->visB) / imageRect.height * 100000.0f));
  return true;
}

//==============================================================================
// Text run style
//==============================================================================

struct PPTRunStyle {
  const char* algn = nullptr;
  int fontSize = 0;
  int64_t letterSpc = 0;
  bool hasBold = false;
  bool hasItalic = false;
  bool hasLetterSpacing = false;
  bool hasFillColor = false;
  float fillAlpha = 0;
  const ColorSource* fillColor = nullptr;
  std::string typeface = {};
  // Stroke painter that paired with this run, if any. PowerPoint expresses a
  // glyph stroke via <a:ln> nested inside <a:rPr>; emitting it here lets a
  // single text shape carry both the fill and the outline (and avoids stacking
  // a stroke-only shape that would otherwise hide the fill in renderers that
  // treat a missing rPr fill as opaque black, e.g. LibreOffice).
  const Stroke* stroke = nullptr;
  float strokeAlpha = 1.0f;
};

// a:rPr supports a:solidFill and a:gradFill but not a:blipFill, so ImagePattern
// fills are skipped (the run falls back to the renderer default).
inline bool IsRunCompatibleColorSource(const ColorSource* color) {
  if (color == nullptr) {
    return false;
  }
  switch (color->nodeType()) {
    case NodeType::SolidColor:
    case NodeType::LinearGradient:
    case NodeType::RadialGradient:
    case NodeType::ConicGradient:
    case NodeType::DiamondGradient:
      return true;
    default:
      return false;
  }
}

// Builds the per-run style fields shared by writeNativeText and writeTextBoxGroup.
// Alignment lives on a:pPr (not a:rPr), so callers set `algn` separately when
// needed. `stroke` is the optional Stroke painter that should be folded into
// the same run (renders as <a:ln> inside <a:rPr>).
inline PPTRunStyle BuildRunStyle(const Text* text, const Fill* fill, const Stroke* stroke,
                                 float alpha) {
  PPTRunStyle style = {};
  // PowerPoint-first bold/italic mapping. A real Bold / Italic face (the
  // PAGX Text carries fontStyle="Bold" / "Italic" / "Bold Italic") is
  // encoded by appending the style to the typeface name (e.g. "Arial Bold"),
  // and the b="1" / i="1" attributes are suppressed. PowerPoint and Keynote
  // resolve this directly to the real bold/italic glyph face — emitting just
  // the bare family name plus b="1" instead made them apply faux-bold
  // synthesis on top of the regular face, producing a visibly thicker stroke
  // than the PAGX renderer (which loads the real Arial Bold face via
  // MakeFromName(fontFamily, fontStyle)).
  //
  // fauxBold / fauxItalic still surface as b="1" / i="1" — those flags exist
  // precisely so the renderer synthesizes the style instead of locking onto
  // a particular face. When both apply at once (a real Bold face plus an
  // additional fauxBold), the styled typeface picks the real Bold face and
  // the b="1" attribute layers the extra synthesis on top, mirroring tgfx's
  // setFauxBold-on-top-of-Bold-primary behaviour in PAGX's own renderer.
  //
  // Trade-off: fontconfig-based renderers (LibreOffice on Linux, some online
  // viewers) match "Arial Bold" against the *family* "Arial Bold" rather
  // than family "Arial" + Bold face. No such family exists, so they fall back
  // to a substitute font and lose the bold weight. This was the reason for
  // the previous bare-family approach; the call here is that PowerPoint
  // visual fidelity matters more than LibreOffice fallback behaviour.
  bool hasRealBold = text->fontStyle.find("Bold") != std::string::npos;
  bool hasRealItalic = text->fontStyle.find("Italic") != std::string::npos;
  style.hasBold = text->fauxBold;
  style.hasItalic = text->fauxItalic;
  // Use the layout-resolved font size. PAGX layout may shrink a Text internally
  // via a textScale factor to fit dual-axis constraints (e.g. `left`+`right`,
  // `width="100%"`); `renderFontSize()` carries that factor while `fontSize`
  // still holds the authored pre-scale value. Emitting the raw size here was
  // the cause of constrained text overflowing its container in PowerPoint.
  // Recover the same scale to apply it to letterSpacing, which the renderer
  // also multiplies by textScale during shaping.
  float effectiveFontSize = text->renderFontSize();
  float textScale = (text->fontSize > 0.0f) ? effectiveFontSize / text->fontSize : 1.0f;
  float effectiveLetterSpacing = text->letterSpacing * textScale;
  style.fontSize = FontSizeToPPT(effectiveFontSize);
  style.letterSpc = static_cast<int64_t>(std::round(effectiveLetterSpacing * 75.0));
  style.hasLetterSpacing = effectiveLetterSpacing != 0.0f;
  style.hasFillColor = fill && IsRunCompatibleColorSource(fill->color);
  style.fillAlpha = style.hasFillColor ? fill->alpha * alpha : 0;
  style.fillColor = style.hasFillColor ? fill->color : nullptr;
  // Build the typeface name. Real Bold / Italic styles get appended onto the
  // family ("Arial" -> "Arial Bold") so PowerPoint locks onto the real face;
  // fauxBold / fauxItalic stay on the bare family because they need
  // synthesis (handled via b/i above). Other style hints (e.g. "Light",
  // "Medium") cannot be expressed in basic OOXML rPr and collapse to the
  // regular face here — the same face would have been picked if we had
  // emitted "Arial Light" as the typeface name and the renderer failed to
  // resolve it.
  if (text->fontFamily.empty()) {
    style.typeface = std::string();
  } else {
    style.typeface = StripQuotes(text->fontFamily);
    if (hasRealBold && hasRealItalic) {
      style.typeface += " Bold Italic";
    } else if (hasRealBold) {
      style.typeface += " Bold";
    } else if (hasRealItalic) {
      style.typeface += " Italic";
    }
  }
  style.stroke = stroke;
  style.strokeAlpha = alpha;
  return style;
}

// PAGX `lineHeight` is an absolute pixel value; PPT's <a:lnSpc><a:spcPts>
// takes hundredths of a point. Convert px -> pt with the same 96 DPI ratio
// (x72/96 = x0.75) used elsewhere in this writer, then multiply by 100.
inline int64_t LineHeightToSpcPts(float lineHeightPx) {
  return lineHeightPx > 0 ? static_cast<int64_t>(std::round(lineHeightPx * 75.0)) : 0;
}

// PAGX's text layout advances each `\t` to the next multiple of `4 * fontSize`
// pixels (TextLayout.cpp: `tabWidth = effectiveFontSize * 4`). OOXML expresses
// the same idea via <a:pPr defTabSz="...">, in EMU. Without this override
// PowerPoint falls back to the master's defTabSz="914400" (1 inch ≈ 96px), so
// any text whose font size differs from 24px renders tabs at the wrong stops
// — text following the tab lands at a different X position than PAGX laid
// out, visibly drifting (or overflowing) the text box. Returns 0 for
// non-positive font sizes so the caller can skip emission.
inline int64_t PagxTabSizeEMU(float fontSizePx) {
  if (!(fontSizePx > 0.0f)) {
    return 0;
  }
  return static_cast<int64_t>(std::round(static_cast<double>(fontSizePx) * 4.0 * EMU_PER_PX));
}

// Emits an <a:pPr> with optional alignment, line-spacing, base direction, and
// default tab-stop interval. Skips emission entirely when no attribute is set,
// matching the previous inline blocks in writeNativeText / writeParagraph /
// writeTextBoxGroup. When `rtl` is true, emits rtl="1" so PowerPoint runs UBA
// with an RTL paragraph base direction (pPr@rtl=0 is the OOXML default and is
// therefore elided). When `defTabSzEMU` is positive, overrides the master's
// default tab-stop interval so paragraphs with `\t` characters land glyphs at
// the same stops PAGX used during layout.
inline void WriteParagraphProperties(XMLBuilder& out, const char* algn, int64_t lnSpcPts,
                                     bool rtl = false, int64_t defTabSzEMU = 0) {
  if (!algn && lnSpcPts <= 0 && !rtl && defTabSzEMU <= 0) {
    return;
  }
  auto& pPr = out.openElement("a:pPr");
  if (algn) {
    pPr.addRequiredAttribute("algn", algn);
  }
  if (defTabSzEMU > 0) {
    pPr.addRequiredAttribute("defTabSz", defTabSzEMU);
  }
  if (rtl) {
    pPr.addRequiredAttribute("rtl", "1");
  }
  if (lnSpcPts > 0) {
    pPr.closeElementStart();
    out.openElement("a:lnSpc").closeElementStart();
    out.openElement("a:spcPts").addRequiredAttribute("val", lnSpcPts).closeElementSelfClosing();
    out.closeElement();  // a:lnSpc
    out.closeElement();  // a:pPr
  } else {
    pPr.closeElementSelfClosing();
  }
}

// Adds the TextBox-derived attributes to a currently-open <a:bodyPr>: vertical
// writing mode, paragraph anchoring, and the vertical-mode anchorCtr override
// for TextAlign::Center. Returns true when the box uses vertical writing mode.
inline bool AddBodyPrAttrsForTextBox(XMLBuilder& out, const TextBox* box) {
  if (box == nullptr) {
    return false;
  }
  bool isVertical = (box->writingMode == WritingMode::Vertical);
  // CJK vertical writing: characters stay upright, columns stack right-to-left.
  // "eaVert" matches the WritingMode::Vertical contract (East Asian vertical
  // text). Latin "vert"/"vert270" rotate glyphs sideways which is wrong here.
  if (isVertical) {
    out.addRequiredAttribute("vert", "eaVert");
  }
  // OOXML's "anchor" describes alignment along the block-flow axis, which
  // matches paragraphAlign in both writing modes:
  //   - Horizontal: block axis is top->bottom (Near=top, Far=bottom).
  //   - Vertical (eaVert): block axis is right->left (Near=right column,
  //     Far=left column). PowerPoint maps t/ctr/b to start/center/end of
  //     that axis, so the same enum->string mapping applies.
  if (box->paragraphAlign == ParagraphAlign::Middle) {
    out.addRequiredAttribute("anchor", "ctr");
  } else if (box->paragraphAlign == ParagraphAlign::Far) {
    out.addRequiredAttribute("anchor", "b");
  }
  // In vertical writing mode the bodyPr@anchor controls placement perpendicular
  // to the text-flow axis (i.e. horizontal placement of the column block), so
  // textAlign="center" cannot be expressed via pPr@algn -- both PowerPoint and
  // LibreOffice ignore algn for vertical-axis centering of the text within a
  // column. anchorCtr="1" toggles the "center on the perpendicular axis" flag,
  // which in vertical mode produces the desired vertical centering of the text
  // within its column.
  if (isVertical && box->textAlign == TextAlign::Center) {
    out.addRequiredAttribute("anchorCtr", "1");
  }
  return isVertical;
}

//==============================================================================
// Blend mode / effect-source helpers (shared by PPTStyleEmitter.cpp only, but
// kept here so future writers can reuse them).
//==============================================================================

inline const char* BlendModeToPPT(BlendMode mode) {
  switch (mode) {
    case BlendMode::Normal:
      return "over";
    case BlendMode::Multiply:
      return "mult";
    case BlendMode::Screen:
      return "screen";
    case BlendMode::Darken:
      return "darken";
    case BlendMode::Lighten:
      return "lighten";
    default:
      return nullptr;
  }
}

// Grouped effect sources used by writeEffects. Collecting into one struct in
// a single pass over filters + styles avoids the original "scan twice to
// decide whether anything needs emitting, then scan again per effect type"
// pattern. Order of emission is fixed by OOXML §20.1.8.20 (blur → fillOverlay
// → innerShdw → outerShdw) and applied by the consumer, not by collection.
struct EffectSources {
  const BlurFilter* blur = nullptr;
  const BlendFilter* blend = nullptr;
  const InnerShadowFilter* innerShadowFilter = nullptr;
  const InnerShadowStyle* innerShadowStyle = nullptr;
  const DropShadowFilter* dropShadowFilter = nullptr;
  const DropShadowStyle* dropShadowStyle = nullptr;

  // BackgroundBlurStyle is intentionally not tracked: it has no faithful OOXML
  // primitive (a:blur blurs the shape itself, not the backdrop) and the writer
  // drops it on the vector path. When `bakeUnsupported` is enabled, the feature
  // probe bakes the layer (with backdrop) to a PNG patch instead.
  bool empty() const {
    return !blur && !blend && !innerShadowFilter && !innerShadowStyle && !dropShadowFilter &&
           !dropShadowStyle;
  }
};

inline EffectSources CollectEffectSources(const std::vector<LayerFilter*>& filters,
                                          const std::vector<LayerStyle*>& styles) {
  EffectSources sources;
  for (const auto* f : filters) {
    switch (f->nodeType()) {
      case NodeType::BlurFilter:
        if (!sources.blur) {
          sources.blur = static_cast<const BlurFilter*>(f);
        }
        break;
      case NodeType::BlendFilter:
        if (!sources.blend) {
          sources.blend = static_cast<const BlendFilter*>(f);
        }
        break;
      case NodeType::InnerShadowFilter:
        if (!sources.innerShadowFilter) {
          sources.innerShadowFilter = static_cast<const InnerShadowFilter*>(f);
        }
        break;
      case NodeType::DropShadowFilter:
        if (!sources.dropShadowFilter) {
          sources.dropShadowFilter = static_cast<const DropShadowFilter*>(f);
        }
        break;
      default:
        break;
    }
  }
  for (const auto* s : styles) {
    switch (s->nodeType()) {
      case NodeType::InnerShadowStyle:
        if (!sources.innerShadowStyle) {
          sources.innerShadowStyle = static_cast<const InnerShadowStyle*>(s);
        }
        break;
      case NodeType::DropShadowStyle:
        if (!sources.dropShadowStyle) {
          sources.dropShadowStyle = static_cast<const DropShadowStyle*>(s);
        }
        break;
      default:
        break;
    }
  }
  return sources;
}

//==============================================================================
// PPTWriter — converts PAGX nodes to PPTX XML.
//
// Implementation is split across three translation units to keep each file
// under ~1700 lines:
//   - PPTExporter.cpp    layer/scope plumbing, geometry shape writers, Xform,
//                        picture envelope, Zip packaging, top-level ToFile.
//   - PPTStyleEmitter.cpp  fill / stroke / effects / image-pattern output.
//   - PPTTextWriter.cpp    native-text and TextBox paragraph/run emission.
// All methods share the same private data by virtue of being on the same
// class; the public surface stays writeLayer + the ctor.
//==============================================================================

class PPTWriter {
 public:
  // One "run" inside a rich TextBox: a Text element together with the Fill
  // and Stroke painters that apply to it. Exposed publicly so anonymous-
  // namespace helpers in the text writer (CollectRichTextRuns) can reference
  // it; PPTWriter itself is only used from the PPT exporter sources.
  struct RichTextRun {
    const Text* text = nullptr;
    const Fill* fill = nullptr;
    const Stroke* stroke = nullptr;
  };

  // One entry in the PAGX-authoritative line list consumed by
  // writeTextBoxGroup. `runIndex` points back into the RichTextRun array, and
  // `byteStart`/`byteEnd` are byte offsets into that run's UTF-8 text.
  // Public for the same reason as RichTextRun.
  struct LineEntry {
    size_t runIndex = 0;
    float baselineY = 0;
    uint32_t byteStart = 0;
    uint32_t byteEnd = 0;
  };

  PPTWriter(PPTWriterContext* ctx, PAGXDocument* doc, const PPTExporter::Options& options,
            LayoutContext* layoutContext)
      : _ctx(ctx), _doc(doc), _convertTextToPath(options.convertTextToPath),
        _bridgeContours(options.bridgeContours), _resolveModifiers(options.resolveModifiers),
        _bakeUnsupported(options.bakeUnsupported), _rasterDPI(options.rasterDPI),
        _layoutContext(layoutContext), _resolver(doc) {
  }

  // Top-level entry: walks every layer in `_doc->layers`, pairing each with its corresponding
  // top-level tgfx::Layer from the build-time root, so writeLayer's positional descent into
  // tgfxLayer->children() resolves the correct per-instance subtree for any Composition reference.
  void writeDocument(XMLBuilder& out);

  void writeLayer(XMLBuilder& out, const Layer* layer,
                  const std::shared_ptr<tgfx::Layer>& tgfxLayer, const Matrix& parentMatrix = {},
                  float parentAlpha = 1.0f, const std::vector<LayerFilter*>& inheritedFilters = {},
                  const std::vector<LayerStyle*>& inheritedStyles = {});

 private:
  // Returns true iff the layer was successfully rasterized and emitted as p:pic.
  // When `withBackdrop` is true, the whole scene below `layer` is rendered into the
  // PNG (clipped to the layer's global bounds) so that a non-Normal blend mode on
  // the layer composites against the backdrop correctly; otherwise only the layer
  // itself is rendered (used for mask / scrollRect fallbacks that don't depend on
  // the backdrop pixels).
  // `tgfxLayer` is the live tgfx instance built from `layer`; the caller (writeLayer) supplies it
  // because layerMap collapses multiple Composition references to the same key.
  bool rasterizeLayerAsPicture(XMLBuilder& out, const Layer* layer,
                               const std::shared_ptr<tgfx::Layer>& tgfxLayer,
                               bool withBackdrop = false);

  PPTWriterContext* _ctx = nullptr;
  PAGXDocument* _doc = nullptr;
  bool _convertTextToPath = false;
  bool _bridgeContours = false;
  bool _resolveModifiers = true;
  bool _bakeUnsupported = true;
  // Ratio of raster DPI to the 96 DPI logical coordinate space. Drives the
  // off-screen Surface size of every PNG bake (masked layer, scrollRect bake,
  // blend/wide-gamut fallback, tiled pattern). The placed <p:pic>/<a:blipFill>
  // keeps using logical EMU dimensions so the consumer stretches the denser
  // bitmap over the same visible extent, yielding a crisper result.
  int _rasterDPI = 192;
  LayoutContext* _layoutContext = nullptr;
  GPUContext _gpu;
  LayerBuildResult _buildResult = {};
  bool _buildResultReady = false;
  PPTModifierResolver _resolver;

  const LayerBuildResult& ensureBuildResult();

  // One geometry instance captured during the scope walk in writeElements.
  // Transform/alpha are baked at collection time so that later painters can
  // emit the geometry without knowing about the surrounding Group/TextBox
  // stack. `textBox` carries the in-scope <TextBox> modifier so Text
  // geometry still picks up box-level layout when rendered by a downstream
  // Fill or Stroke (matches the legacy CollectFillStroke().textBox rule).
  struct AccumulatedGeometry {
    const Element* element = nullptr;
    Matrix transform = {};
    float alpha = 1.0f;
    const TextBox* textBox = nullptr;
  };

  void writeElements(XMLBuilder& out, const std::vector<Element*>& elements,
                     const Matrix& transform, float alpha, const std::vector<LayerFilter*>& filters,
                     const std::vector<LayerStyle*>& styles,
                     const TextBox* parentTextBox = nullptr);

  void processVectorScope(XMLBuilder& out, const std::vector<Element*>& elements,
                          const Matrix& transform, float alpha,
                          const std::vector<LayerFilter*>& filters,
                          const std::vector<LayerStyle*>& styles, const TextBox* parentTextBox,
                          std::vector<AccumulatedGeometry>& accumulator, size_t scopeStart);

  void emitGeometryWithFs(XMLBuilder& out, const AccumulatedGeometry& entry,
                          const FillStrokeInfo& fs, const std::vector<LayerFilter*>& filters,
                          const std::vector<LayerStyle*>& styles);

  void writeRectangle(XMLBuilder& out, const Rectangle* rect, const FillStrokeInfo& fs,
                      const Matrix& m, float alpha, const std::vector<LayerFilter*>& filters,
                      const std::vector<LayerStyle*>& styles);
  void writeEllipse(XMLBuilder& out, const Ellipse* ellipse, const FillStrokeInfo& fs,
                    const Matrix& m, float alpha, const std::vector<LayerFilter*>& filters,
                    const std::vector<LayerStyle*>& styles);
  void writePath(XMLBuilder& out, const Path* path, const FillStrokeInfo& fs, const Matrix& m,
                 float alpha, const std::vector<LayerFilter*>& filters,
                 const std::vector<LayerStyle*>& styles);
  void writeTextAsPath(XMLBuilder& out, const Text* text, const FillStrokeInfo& fs, const Matrix& m,
                       float alpha, const std::vector<LayerFilter*>& filters,
                       const std::vector<LayerStyle*>& styles);
  void writeNativeText(XMLBuilder& out, const Text* text, const FillStrokeInfo& fs, const Matrix& m,
                       float alpha, const std::vector<LayerFilter*>& filters,
                       const std::vector<LayerStyle*>& styles,
                       const TextLayoutResult* precomputed = nullptr);

  // Geometry inputs needed to build the native-text shape frame: the shape's
  // top-left, its estimated content size, and whether the source has a text
  // box (controls wrap="square" vs. "none"). Populated by
  // computeNativeTextGeometry and consumed by emitNativeTextShapeFrame.
  struct NativeTextGeometry {
    float posX = 0;
    float posY = 0;
    float estWidth = 0;
    float estHeight = 0;
    bool hasTextBox = false;
  };

  NativeTextGeometry computeNativeTextGeometry(const Text* text, Text* mutableText,
                                               const FillStrokeInfo& fs,
                                               const TextLayoutResult* precomputed);
  void emitNativeTextShapeFrame(XMLBuilder& out, const Matrix& m, const NativeTextGeometry& geom,
                                const TextBox* textBox, bool useLineLayout);
  void emitNativeTextBody(XMLBuilder& out, const Text* text,
                          const std::vector<TextLayoutLineInfo>* lines, const PPTRunStyle& style,
                          int64_t lnSpcPts, bool rtl, bool useLineLayout, int64_t defTabSzEMU,
                          const std::vector<LayerFilter*>& filters,
                          const std::vector<LayerStyle*>& styles);
  void writeParagraph(XMLBuilder& out, const std::string& lineText, const PPTRunStyle& style,
                      const std::vector<LayerFilter*>& filters,
                      const std::vector<LayerStyle*>& styles, int64_t lnSpcPts = 0,
                      bool rtl = false, int64_t defTabSzEMU = 0);
  void writeParagraphRun(XMLBuilder& out, const std::string& runText, const PPTRunStyle& style,
                         const std::vector<LayerFilter*>& filters,
                         const std::vector<LayerStyle*>& styles);
  // Soft line break inside an a:p. The rPr is needed so the line break inherits
  // the same font/size as the following run (otherwise PowerPoint uses its
  // default size and the line-height of that break is wrong).
  void writeLineBreak(XMLBuilder& out, const PPTRunStyle& style);

  // Walks the byte range [start, end) of `source` and emits one <a:br/> for
  // each '\n' encountered. Returns the number of breaks emitted so the caller
  // can distinguish "explicit \n in source" from "PAGX-driven auto-wrap".
  // Used to reconstruct empty paragraphs that TextLayout's line metadata
  // collapses (consecutive '\n's). Replaces the equivalent lambda inline
  // because project convention forbids lambda expressions.
  size_t writeNewlineBreaksInRange(XMLBuilder& out, const std::string& source, size_t start,
                                   size_t end, const PPTRunStyle& style);
  void writeTextBoxGroup(XMLBuilder& out, const Group* textBox,
                         const std::vector<Element*>& elements, const Matrix& transform,
                         float alpha, const std::vector<LayerFilter*>& filters,
                         const std::vector<LayerStyle*>& styles);

  // Tracks the open/close state of an <a:p> element while writeTextBoxGroup
  // streams runs into paragraphs. Replaces the previous lambda-based approach
  // so the helpers become explicit methods (project convention forbids
  // lambdas). Lifetime is tied to a single writeTextBoxGroup invocation; all
  // reference members alias data owned by that call frame.
  struct ParagraphEmitter {
    PPTWriter* writer;
    XMLBuilder& out;
    const char* algn;
    int64_t lnSpcPts;
    bool rtl;
    int64_t defTabSzEMU;
    const std::vector<LayerFilter*>& filters;
    const std::vector<LayerStyle*>& styles;
    bool paragraphOpen = false;

    void writePPr();
    void openParagraph();
    void closeParagraph();
    void emitRun(const std::string& fragment, const PPTRunStyle& style);
    void emitLineBreak(const PPTRunStyle& style);
  };

  void emitTextBoxShapeFrame(XMLBuilder& out, const TextBox* box, const Matrix& transform,
                             float estWidth, float estHeight, bool useLineLayout, bool hasBoxWidth);
  void emitTextBoxBody(const std::vector<RichTextRun>& runs,
                       const std::vector<PPTRunStyle>& runStyles,
                       std::vector<LineEntry>& lineEntries, bool useLineLayout,
                       ParagraphEmitter& emitter);

  // Walks the source-text range [startRun:startByte, endRun:endByte) across
  // adjacent RichTextRun entries and emits one <a:br/> per '\n' encountered.
  // Each break carries the rPr of the run that contains the '\n', so a 40pt
  // run followed by a 16pt run preserves the 40pt height for the empty line
  // sitting inside the 40pt run. Returns the number of breaks emitted so the
  // caller can distinguish "explicit \n in source" from "PAGX-driven auto
  // wrap with no \n in source". Replaces the equivalent lambda inline because
  // project convention forbids lambda expressions.
  size_t writeNewlineBreaksAcrossRuns(ParagraphEmitter& emitter,
                                      const std::vector<RichTextRun>& runs,
                                      const std::vector<PPTRunStyle>& runStyles, size_t startRun,
                                      size_t startByte, size_t endRun, size_t endByte);

  // Shape envelope helpers
  void beginShape(XMLBuilder& out, const char* name, int64_t offX, int64_t offY, int64_t extCX,
                  int64_t extCY, int rot = 0);
  void endShape(XMLBuilder& out);

  // Fill / stroke / effects
  void writeFill(XMLBuilder& out, const Fill* fill, float alpha, const Rect& shapeBounds = {});
  void writeColorSource(XMLBuilder& out, const ColorSource* source, float alpha,
                        const Rect& shapeBounds = {});
  void writeSolidColorFill(XMLBuilder& out, const Color& color, float alpha);
  void writeImagePatternFill(XMLBuilder& out, const ImagePattern* pattern, float alpha,
                             const Rect& shapeBounds);
  void writeGradientStops(XMLBuilder& out, const std::vector<ColorStop*>& stops, float alpha);
  void writeStroke(XMLBuilder& out, const Stroke* stroke, float alpha);
  void writeEffects(XMLBuilder& out, const std::vector<LayerFilter*>& filters,
                    const std::vector<LayerStyle*>& styles = {});
  void writeShadowElement(XMLBuilder& out, const char* tag, float blurX, float blurY, float offsetX,
                          float offsetY, const Color& color, bool includeAlign);

  static void WriteBlip(XMLBuilder& out, const std::string& relId, float alpha);
  static void WriteDefaultStretch(XMLBuilder& out);

  void writeGlyphShape(XMLBuilder& out, const Fill* fill, float alpha);

  void writeShapeTail(XMLBuilder& out, const FillStrokeInfo& fs, float alpha,
                      const Rect& shapeBounds, bool imageWritten,
                      const std::vector<LayerFilter*>& filters,
                      const std::vector<LayerStyle*>& styles = {});

  // Shared contour-to-custGeom emitter used by writePath and writeTextAsPath
  // for the non-bridged or single-group case (when callers haven't already
  // prepared per-group emission themselves).
  void writeContourGeom(XMLBuilder& out, std::vector<PathContour>& contours, int64_t pathWidth,
                        int64_t pathHeight, float scaleX, float scaleY, float scaledOfsX,
                        float scaledOfsY, FillRule fillRule);

  // Transform decomposition
  struct Xform {
    int64_t offX = 0;
    int64_t offY = 0;
    int64_t extCX = 0;
    int64_t extCY = 0;
    int rotation = 0;
  };
  static Xform DecomposeXform(float localX, float localY, float localW, float localH,
                              const Matrix& m);
  static void WriteXfrm(XMLBuilder& out, const Xform& xf);

  // Emits the common <p:sp> / <p:spPr> / <p:txBody><a:bodyPr> / <a:lstStyle/>
  // scaffold that both native-text and TextBox shape frames share. The caller
  // supplies the decomposed Xform, the in-scope TextBox (for bodyPr paragraph
  // attributes), and the pre-computed wrap value ("square" vs "none"). Leaves
  // <p:txBody> open so the caller can stream <a:p> children into it.
  void emitTextShapeEnvelope(XMLBuilder& out, const Xform& xf, const TextBox* textBox,
                             const char* wrap);

  // p:pic helpers (declared after Xform)
  void beginPicture(XMLBuilder& out, const char* name);
  void endPicture(XMLBuilder& out, const Xform& xf);

  // Rasterized image as p:pic element
  void writePicture(XMLBuilder& out, const std::string& relId, int64_t offX, int64_t offY,
                    int64_t extCX, int64_t extCY);

  // Write non-tiling ImagePattern fill as a separate p:pic element.
  // Returns true if the image was written; caller should use a:noFill for the shape.
  bool writeImagePatternAsPicture(XMLBuilder& out, const Fill* fill, const Rect& shapeBounds,
                                  const Matrix& m, float alpha);
};

}  // namespace pagx
