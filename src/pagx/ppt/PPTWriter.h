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
#include "pagx/nodes/BackgroundBlurStyle.h"
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

inline bool ComputeImagePatternRect(const ImagePattern* pattern, int imgW, int imgH,
                                    const Rect& shapeBounds, ImagePatternRect* result) {
  const auto& M = pattern->matrix;
  float scaleX = std::sqrt(M.a * M.a + M.b * M.b);
  float scaleY = std::sqrt(M.c * M.c + M.d * M.d);
  if (scaleX <= 0 || scaleY <= 0) {
    return false;
  }
  float imageDocW = static_cast<float>(imgW) * scaleX;
  float imageDocH = static_cast<float>(imgH) * scaleY;
  result->visL = std::max(shapeBounds.x, M.tx);
  result->visT = std::max(shapeBounds.y, M.ty);
  result->visR = std::min(shapeBounds.x + shapeBounds.width, M.tx + imageDocW);
  result->visB = std::min(shapeBounds.y + shapeBounds.height, M.ty + imageDocH);
  if (result->visR <= result->visL || result->visB <= result->visT) {
    return false;
  }
  result->srcL = static_cast<int>(std::round((result->visL - M.tx) / imageDocW * 100000.0f));
  result->srcT = static_cast<int>(std::round((result->visT - M.ty) / imageDocH * 100000.0f));
  result->srcR =
      static_cast<int>(std::round((M.tx + imageDocW - result->visR) / imageDocW * 100000.0f));
  result->srcB =
      static_cast<int>(std::round((M.ty + imageDocH - result->visB) / imageDocH * 100000.0f));
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
  style.hasBold = text->fauxBold || text->fontStyle.find("Bold") != std::string::npos;
  style.hasItalic = text->fauxItalic || text->fontStyle.find("Italic") != std::string::npos;
  style.fontSize = FontSizeToPPT(text->fontSize);
  style.letterSpc = static_cast<int64_t>(std::round(text->letterSpacing * 75.0));
  style.hasLetterSpacing = text->letterSpacing != 0.0f;
  style.hasFillColor = fill && IsRunCompatibleColorSource(fill->color);
  style.fillAlpha = style.hasFillColor ? fill->alpha * alpha : 0;
  style.fillColor = style.hasFillColor ? fill->color : nullptr;
  style.typeface = text->fontFamily.empty() ? std::string() : StripQuotes(text->fontFamily);
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

// Emits an <a:pPr> with optional alignment and line-spacing. Skips emission
// entirely when neither attribute is set, matching the previous inline blocks
// in writeNativeText / writeParagraph / writeTextBoxGroup.
inline void WriteParagraphProperties(XMLBuilder& out, const char* algn, int64_t lnSpcPts) {
  if (!algn && lnSpcPts <= 0) {
    return;
  }
  auto& pPr = out.openElement("a:pPr");
  if (algn) {
    pPr.addRequiredAttribute("algn", algn);
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
  const BackgroundBlurStyle* backgroundBlur = nullptr;
  const BlendFilter* blend = nullptr;
  const InnerShadowFilter* innerShadowFilter = nullptr;
  const InnerShadowStyle* innerShadowStyle = nullptr;
  const DropShadowFilter* dropShadowFilter = nullptr;
  const DropShadowStyle* dropShadowStyle = nullptr;

  bool empty() const {
    return !blur && !backgroundBlur && !blend && !innerShadowFilter && !innerShadowStyle &&
           !dropShadowFilter && !dropShadowStyle;
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
      case NodeType::BackgroundBlurStyle:
        if (!sources.backgroundBlur) {
          sources.backgroundBlur = static_cast<const BackgroundBlurStyle*>(s);
        }
        break;
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
        _rasterizeUnsupported(options.rasterizeUnsupported), _rasterDPI(options.rasterDPI),
        _layoutContext(layoutContext), _resolver(doc) {
  }

  void writeLayer(XMLBuilder& out, const Layer* layer, const Matrix& parentMatrix = {},
                  float parentAlpha = 1.0f, const std::vector<LayerFilter*>& inheritedFilters = {},
                  const std::vector<LayerStyle*>& inheritedStyles = {});

 private:
  // Returns true iff the layer was successfully rasterized and emitted as p:pic.
  // When `withBackdrop` is true, the whole scene below `layer` is rendered into the
  // PNG (clipped to the layer's global bounds) so that a non-Normal blend mode on
  // the layer composites against the backdrop correctly; otherwise only the layer
  // itself is rendered (used for mask / scrollRect fallbacks that don't depend on
  // the backdrop pixels).
  bool rasterizeLayerAsPicture(XMLBuilder& out, const Layer* layer, bool withBackdrop = false);

  PPTWriterContext* _ctx = nullptr;
  PAGXDocument* _doc = nullptr;
  bool _convertTextToPath = true;
  bool _bridgeContours = false;
  bool _resolveModifiers = true;
  bool _rasterizeUnsupported = false;
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
                          int64_t lnSpcPts, bool useLineLayout,
                          const std::vector<LayerFilter*>& filters,
                          const std::vector<LayerStyle*>& styles);
  void writeParagraph(XMLBuilder& out, const std::string& lineText, const PPTRunStyle& style,
                      const std::vector<LayerFilter*>& filters,
                      const std::vector<LayerStyle*>& styles, int64_t lnSpcPts = 0);
  void writeParagraphRun(XMLBuilder& out, const std::string& runText, const PPTRunStyle& style,
                         const std::vector<LayerFilter*>& filters,
                         const std::vector<LayerStyle*>& styles);
  // Soft line break inside an a:p. The rPr is needed so the line break inherits
  // the same font/size as the following run (otherwise PowerPoint uses its
  // default size and the line-height of that break is wrong).
  void writeLineBreak(XMLBuilder& out, const PPTRunStyle& style);
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
  void WriteContourGeom(XMLBuilder& out, std::vector<PathContour>& contours, int64_t pathWidth,
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
  static Xform decomposeXform(float localX, float localY, float localW, float localH,
                              const Matrix& m);
  static void WriteXfrm(XMLBuilder& out, const Xform& xf);

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
