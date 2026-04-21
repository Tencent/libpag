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

#include "pagx/PPTExporter.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <string>
#include <vector>
#include "base/utils/MathUtil.h"
#include "pagx/LayoutContext.h"
#include "pagx/PAGXDocument.h"
#include "pagx/TextLayout.h"
#include "pagx/TextLayoutParams.h"
#include "pagx/nodes/BackgroundBlurStyle.h"
#include "pagx/nodes/BlendFilter.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/ColorMatrixFilter.h"
#include "pagx/nodes/ColorStop.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/DiamondGradient.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/InnerShadowFilter.h"
#include "pagx/nodes/InnerShadowStyle.h"
#include "pagx/nodes/LayerStyle.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/ppt/PPTBoilerplate.h"
#include "pagx/ppt/PPTContourUtils.h"
#include "pagx/ppt/PPTFeatureProbe.h"
#include "pagx/ppt/PPTGeomEmitter.h"
#include "pagx/ppt/PPTModifierResolver.h"
#include "pagx/ppt/PPTRasterizer.h"
#include "pagx/ppt/PPTWriterContext.h"
#include "pagx/types/Rect.h"
#include "pagx/utils/ExporterUtils.h"
#include "pagx/utils/StringParser.h"
#include "pagx/xml/XMLBuilder.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/layers/DisplayList.h"
#include "zip.h"

namespace pagx {

using pag::FloatNearlyZero;
using pag::RadiansToDegrees;

//==============================================================================
// Coordinate / color / angle helpers
//==============================================================================

static std::string ColorToHex6(const Color& c) {
  std::string buf(6, '\0');
  snprintf(buf.data(), 7, "%02X%02X%02X",
           std::clamp(static_cast<int>(std::round(c.red * 255.0f)), 0, 255),
           std::clamp(static_cast<int>(std::round(c.green * 255.0f)), 0, 255),
           std::clamp(static_cast<int>(std::round(c.blue * 255.0f)), 0, 255));
  return buf;
}

static int AlphaToPct(float alpha) {
  return std::clamp(static_cast<int>(std::round(alpha * 100000.0f)), 0, 100000);
}

static int AngleToPPT(float degrees) {
  int v = static_cast<int>(std::round(degrees * 60000.0));
  return ((v % 21600000) + 21600000) % 21600000;
}

static int FontSizeToPPT(float px) {
  return std::max(100, static_cast<int>(std::round(px * 75.0)));
}

// Emits an `<a:srgbClr val="..."/>` element, expanding into
// `<a:srgbClr val="..."><a:alpha val="..."/></a:srgbClr>` when the effective
// alpha is below 1.0. Centralizes the alpha-emission idiom shared by gradient
// stops, solid fills, and blend overlays.
static void WriteSrgbClr(XMLBuilder& out, const Color& color, float effectiveAlpha) {
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

static size_t CountUTF8Characters(const std::string& str) {
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
static float StrokeAlignInset(const Stroke* stroke) {
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
static void ApplyStrokeBoxInset(const Stroke* stroke, float& x, float& y, float& w, float& h,
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
// Dash pattern → OOXML preset dash mapping
//==============================================================================

// Maps dash-to-stroke-width ratios to OOXML preset dash types (ISO/IEC 29500-1,
// §20.1.10.48 ST_PresetLineDashVal).  Thresholds approximate the boundary between
// dot (≤1.5×), dash (≤4.5×), and long-dash (>4.5×) categories.
static const char* MatchPresetDash(const std::vector<float>& dashes, float strokeWidth) {
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
// ImagePatternRect – shared visible-area / srcRect computation
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
// <a:fillToRect> in 1/1000-percent insets relative to shapeBounds. When the
// shape bounds are unknown (e.g. a text fill where the bounds aren't computed
// at the run level) the focus collapses to the geometric centre.
template <typename Gradient>
static void WriteFillToRectFromCenter(XMLBuilder& out, const Gradient* grad,
                                      const Rect& shapeBounds) {
  int l = 50000;
  int t = 50000;
  int r = 50000;
  int b = 50000;
  if (shapeBounds.width > 0 && shapeBounds.height > 0) {
    Point centerDoc = grad->matrix.mapPoint(grad->center);
    float relX = (centerDoc.x - shapeBounds.x) / shapeBounds.width;
    float relY = (centerDoc.y - shapeBounds.y) / shapeBounds.height;
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

static void AddLTRBAttrs(XMLBuilder& out, const LTRBInsets& insets) {
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

static bool ComputeImagePatternRect(const ImagePattern* pattern, int imgW, int imgH,
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
// PPTWriter – converts PAGX nodes to PPTX XML
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
};

// a:rPr supports a:solidFill and a:gradFill but not a:blipFill, so ImagePattern
// fills are skipped (the run falls back to the renderer default).
static bool IsRunCompatibleColorSource(const ColorSource* color) {
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
// needed.
static PPTRunStyle BuildRunStyle(const Text* text, const Fill* fill, float alpha) {
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
  return style;
}

// PAGX `lineHeight` is an absolute pixel value; PPT's <a:lnSpc><a:spcPts>
// takes hundredths of a point. Convert px -> pt with the same 96 DPI ratio
// (x72/96 = x0.75) used elsewhere in this writer, then multiply by 100.
static int64_t LineHeightToSpcPts(float lineHeightPx) {
  return lineHeightPx > 0 ? static_cast<int64_t>(std::round(lineHeightPx * 75.0)) : 0;
}

// Emits an <a:pPr> with optional alignment and line-spacing. Skips emission
// entirely when neither attribute is set, matching the previous inline blocks
// in writeNativeText / writeParagraph / writeTextBoxGroup.
static void WriteParagraphProperties(XMLBuilder& out, const char* algn, int64_t lnSpcPts) {
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
static bool AddBodyPrAttrsForTextBox(XMLBuilder& out, const TextBox* box) {
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

class PPTWriter {
 public:
  PPTWriter(PPTWriterContext* ctx, PAGXDocument* doc, const PPTExporter::Options& options,
            LayoutContext* layoutContext)
      : _ctx(ctx), _doc(doc), _convertTextToPath(options.convertTextToPath),
        _bakeMask(options.bakeMask), _bakeScrollRect(options.bakeScrollRect),
        _bakeTiledPattern(options.bakeTiledPattern), _bridgeContours(options.bridgeContours),
        _resolveModifiers(options.resolveModifiers),
        _rasterizeUnsupportedBlend(options.rasterizeUnsupportedBlend),
        _rasterizeWideGamut(options.rasterizeWideGamut), _rasterDPI(options.rasterDPI),
        _layoutContext(layoutContext), _resolver(doc) {
  }

  void writeLayer(XMLBuilder& out, const Layer* layer, const Matrix& parentMatrix = {},
                  float parentAlpha = 1.0f);

 private:
  // Returns true iff the layer was successfully rasterized and emitted as p:pic.
  bool rasterizeLayerAsPicture(XMLBuilder& out, const Layer* layer);

  PPTWriterContext* _ctx = nullptr;
  PAGXDocument* _doc = nullptr;
  bool _convertTextToPath = true;
  bool _bakeMask = true;
  bool _bakeScrollRect = true;
  bool _bakeTiledPattern = true;
  bool _bridgeContours = true;
  bool _resolveModifiers = true;
  bool _rasterizeUnsupportedBlend = true;
  bool _rasterizeWideGamut = true;
  // _rasterDPI is wired through to PPTRasterizer via the GPUContext but the
  // current rasterization path always uses the GPU surface's native scale; the
  // option is retained for forward compatibility.
  [[maybe_unused]] int _rasterDPI = 192;
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
  void writeGradientStops(XMLBuilder& out, const std::vector<ColorStop*>& stops);
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

// ── Transform decomposition ────────────────────────────────────────────────

PPTWriter::Xform PPTWriter::decomposeXform(float localX, float localY, float localW, float localH,
                                           const Matrix& m) {
  float cx = localX + localW / 2.0f;
  float cy = localY + localH / 2.0f;
  float tcx = m.a * cx + m.c * cy + m.tx;
  float tcy = m.b * cx + m.d * cy + m.ty;

  float sx = 0;
  float sy = 0;
  DecomposeScale(m, &sx, &sy);

  float tw = localW * sx;
  float th = localH * sy;

  float theta = RadiansToDegrees(std::atan2(m.b, m.a));

  Xform xf;
  xf.offX = PxToEMU(tcx - tw / 2.0f);
  xf.offY = PxToEMU(tcy - th / 2.0f);
  xf.extCX = std::max(int64_t(1), PxToEMU(tw));
  xf.extCY = std::max(int64_t(1), PxToEMU(th));
  xf.rotation = FloatNearlyZero(theta) ? 0 : AngleToPPT(theta);
  return xf;
}

void PPTWriter::WriteXfrm(XMLBuilder& out, const Xform& xf) {
  if (xf.rotation != 0) {
    out.openElement("a:xfrm").addRequiredAttribute("rot", xf.rotation).closeElementStart();
  } else {
    out.openElement("a:xfrm").closeElementStart();
  }
  out.openElement("a:off")
      .addRequiredAttribute("x", xf.offX)
      .addRequiredAttribute("y", xf.offY)
      .closeElementSelfClosing();
  out.openElement("a:ext")
      .addRequiredAttribute("cx", xf.extCX)
      .addRequiredAttribute("cy", xf.extCY)
      .closeElementSelfClosing();
  out.closeElement();  // a:xfrm
}

// ── Shape envelope ─────────────────────────────────────────────────────────

void PPTWriter::beginShape(XMLBuilder& out, const char* name, int64_t offX, int64_t offY,
                           int64_t extCX, int64_t extCY, int rot) {
  int id = _ctx->nextShapeId();
  out.openElement("p:sp").closeElementStart();

  out.openElement("p:nvSpPr").closeElementStart();
  out.openElement("p:cNvPr")
      .addRequiredAttribute("id", id)
      .addRequiredAttribute("name", name)
      .closeElementSelfClosing();
  out.openElement("p:cNvSpPr").closeElementSelfClosing();
  out.openElement("p:nvPr").closeElementSelfClosing();
  out.closeElement();  // p:nvSpPr

  out.openElement("p:spPr").closeElementStart();
  WriteXfrm(out, {offX, offY, extCX, extCY, rot});
}

void PPTWriter::endShape(XMLBuilder& out) {
  out.closeElement();  // p:spPr
  out.openElement("p:txBody").closeElementStart();
  out.openElement("a:bodyPr").closeElementSelfClosing();
  out.openElement("a:lstStyle").closeElementSelfClosing();
  out.openElement("a:p").closeElementStart();
  // Default to en-US as a language-neutral fallback.
  out.openElement("a:endParaRPr").addRequiredAttribute("lang", "en-US").closeElementSelfClosing();
  out.closeElement();  // a:p
  out.closeElement();  // p:txBody
  out.closeElement();  // p:sp
}

void PPTWriter::writeGlyphShape(XMLBuilder& out, const Fill* fill, float alpha) {
  writeFill(out, fill, alpha);
  out.openElement("a:ln").closeElementStart();
  out.openElement("a:noFill").closeElementSelfClosing();
  out.closeElement();
  endShape(out);
}

void PPTWriter::writeShapeTail(XMLBuilder& out, const FillStrokeInfo& fs, float alpha,
                               const Rect& shapeBounds, bool imageWritten,
                               const std::vector<LayerFilter*>& filters,
                               const std::vector<LayerStyle*>& styles) {
  if (imageWritten) {
    out.openElement("a:noFill").closeElementSelfClosing();
  } else {
    writeFill(out, fs.fill, alpha, shapeBounds);
  }
  writeStroke(out, fs.stroke, alpha);
  writeEffects(out, filters, styles);
  endShape(out);
}

// ── Lazy build result ───────────────────────────────────────────────────────

const LayerBuildResult& PPTWriter::ensureBuildResult() {
  if (!_buildResultReady) {
    _buildResult = LayerBuilder::BuildWithMap(_doc);
    _buildResultReady = true;
  }
  return _buildResult;
}

// ── Shared XML helpers ──────────────────────────────────────────────────────

void PPTWriter::WriteBlip(XMLBuilder& out, const std::string& relId, float alpha) {
  out.openElement("a:blip").addRequiredAttribute("r:embed", relId);
  if (alpha < 1.0f) {
    out.closeElementStart();
    out.openElement("a:alphaModFix")
        .addRequiredAttribute("amt", AlphaToPct(alpha))
        .closeElementSelfClosing();
    out.closeElement();  // a:blip
  } else {
    out.closeElementSelfClosing();
  }
}

void PPTWriter::WriteDefaultStretch(XMLBuilder& out) {
  out.openElement("a:stretch").closeElementStart();
  out.openElement("a:fillRect").closeElementSelfClosing();
  out.closeElement();  // a:stretch
}

// ── Picture helpers ─────────────────────────────────────────────────────────

void PPTWriter::beginPicture(XMLBuilder& out, const char* name) {
  int id = _ctx->nextShapeId();
  out.openElement("p:pic").closeElementStart();

  out.openElement("p:nvPicPr").closeElementStart();
  out.openElement("p:cNvPr")
      .addRequiredAttribute("id", id)
      .addRequiredAttribute("name", name)
      .closeElementSelfClosing();
  out.openElement("p:cNvPicPr").closeElementStart();
  out.openElement("a:picLocks")
      .addRequiredAttribute("noChangeAspect", "1")
      .closeElementSelfClosing();
  out.closeElement();  // p:cNvPicPr
  out.openElement("p:nvPr").closeElementSelfClosing();
  out.closeElement();  // p:nvPicPr
}

void PPTWriter::endPicture(XMLBuilder& out, const Xform& xf) {
  out.openElement("p:spPr").closeElementStart();
  WriteXfrm(out, xf);
  out.openElement("a:prstGeom").addRequiredAttribute("prst", "rect").closeElementStart();
  out.openElement("a:avLst").closeElementSelfClosing();
  out.closeElement();  // a:prstGeom
  out.closeElement();  // p:spPr

  out.closeElement();  // p:pic
}

// ── Rasterized picture ──────────────────────────────────────────────────────

void PPTWriter::writePicture(XMLBuilder& out, const std::string& relId, int64_t offX, int64_t offY,
                             int64_t extCX, int64_t extCY) {
  beginPicture(out, "MaskedImage");

  out.openElement("p:blipFill").closeElementStart();
  WriteBlip(out, relId, 1.0f);
  WriteDefaultStretch(out);
  out.closeElement();  // p:blipFill

  endPicture(out, {offX, offY, extCX, extCY, 0});
}

// ── ImagePattern as p:pic element ──────────────────────────────────────────

bool PPTWriter::writeImagePatternAsPicture(XMLBuilder& out, const Fill* fill,
                                           const Rect& shapeBounds, const Matrix& m, float alpha) {
  if (!fill || !fill->color || fill->color->nodeType() != NodeType::ImagePattern) {
    return false;
  }
  auto* pattern = static_cast<const ImagePattern*>(fill->color);
  if (!pattern->image) {
    return false;
  }
  // Repeat/Mirror need OOXML <a:tile> (or a baked tiled PNG); Clamp needs the
  // image's edge pixels to extend across the whole shape, which can't be
  // expressed by drawing a single <p:pic> at the natural extent.  Only Decal
  // (transparent outside the natural extent) maps cleanly onto a <p:pic>.
  if (pattern->tileModeX != TileMode::Decal || pattern->tileModeY != TileMode::Decal) {
    return false;
  }

  int imgW = 0;
  int imgH = 0;
  if (!GetImageDimensions(pattern->image, &imgW, &imgH) || shapeBounds.isEmpty()) {
    return false;
  }

  ImagePatternRect ipr = {};
  if (!ComputeImagePatternRect(pattern, imgW, imgH, shapeBounds, &ipr)) {
    return false;
  }

  std::string relId = _ctx->addImage(pattern->image);
  if (relId.empty()) {
    return false;
  }

  float effectiveAlpha = fill->alpha * alpha;

  bool hasSrcRect = (ipr.srcL != 0 || ipr.srcT != 0 || ipr.srcR != 0 || ipr.srcB != 0);

  auto xf = decomposeXform(ipr.visL, ipr.visT, ipr.visR - ipr.visL, ipr.visB - ipr.visT, m);

  beginPicture(out, "Image");

  out.openElement("p:blipFill").closeElementStart();
  WriteBlip(out, relId, effectiveAlpha);
  if (hasSrcRect) {
    out.openElement("a:srcRect");
    AddLTRBAttrs(out, {ipr.srcL, ipr.srcT, ipr.srcR, ipr.srcB});
    out.closeElementSelfClosing();
  }
  WriteDefaultStretch(out);
  out.closeElement();  // p:blipFill

  endPicture(out, xf);
  return true;
}

// ── Color source / gradient ────────────────────────────────────────────────

void PPTWriter::writeGradientStops(XMLBuilder& out, const std::vector<ColorStop*>& stops) {
  out.openElement("a:gsLst").closeElementStart();
  for (const auto* stop : stops) {
    int pos = std::clamp(static_cast<int>(std::round(stop->offset * 100000.0f)), 0, 100000);
    out.openElement("a:gs").addRequiredAttribute("pos", pos).closeElementStart();
    WriteSrgbClr(out, stop->color, stop->color.alpha);
    out.closeElement();  // a:gs
  }
  out.closeElement();  // a:gsLst
}

void PPTWriter::writeColorSource(XMLBuilder& out, const ColorSource* source, float alpha,
                                 const Rect& shapeBounds) {
  if (!source) {
    out.openElement("a:noFill").closeElementSelfClosing();
    return;
  }

  switch (source->nodeType()) {
    case NodeType::SolidColor: {
      auto* solid = static_cast<const SolidColor*>(source);
      writeSolidColorFill(out, solid->color, alpha);
      break;
    }
    case NodeType::LinearGradient: {
      auto* grad = static_cast<const LinearGradient*>(source);
      Point sp = grad->matrix.mapPoint(grad->startPoint);
      Point ep = grad->matrix.mapPoint(grad->endPoint);
      float dx = ep.x - sp.x;
      float dy = ep.y - sp.y;
      float angleDeg = RadiansToDegrees(std::atan2(dy, dx));
      int ang = AngleToPPT(angleDeg);

      out.openElement("a:gradFill").closeElementStart();
      writeGradientStops(out, grad->colorStops);
      out.openElement("a:lin")
          .addRequiredAttribute("ang", ang)
          .addRequiredAttribute("scaled", "1")
          .closeElementSelfClosing();
      out.closeElement();  // a:gradFill
      break;
    }
    case NodeType::RadialGradient: {
      // Map the gradient center through its matrix to document space and convert to
      // fillToRect insets (1/1000 percent) relative to the shape bounds.  OOXML
      // radial gradients always span the whole bounding box, so radius cannot be
      // honoured exactly; only the focus position is preserved.  When shapeBounds
      // is empty (e.g. text fills) fall back to the shape center.
      auto* grad = static_cast<const RadialGradient*>(source);
      out.openElement("a:gradFill").closeElementStart();
      writeGradientStops(out, grad->colorStops);
      out.openElement("a:path").addRequiredAttribute("path", "circle").closeElementStart();
      WriteFillToRectFromCenter(out, grad, shapeBounds);
      out.closeElement();  // a:path
      out.closeElement();  // a:gradFill
      break;
    }
    case NodeType::ConicGradient: {
      // OOXML has no conic/sweep gradient primitive. Layers containing one are
      // normally escalated to a layer-level rasterization fallback by
      // PPTFeatureProbe; this branch is only reached when the rasterizer is
      // unavailable (e.g. no GPU) and emits a circular path gradient as a
      // last-resort fallback so something visible still ends up on the slide.
      auto* grad = static_cast<const ConicGradient*>(source);
      out.openElement("a:gradFill").addRequiredAttribute("rotWithShape", "1").closeElementStart();
      writeGradientStops(out, grad->colorStops);
      out.openElement("a:path").addRequiredAttribute("path", "circle").closeElementStart();
      WriteFillToRectFromCenter(out, grad, shapeBounds);
      out.closeElement();  // a:path
      out.closeElement();  // a:gradFill
      break;
    }
    case NodeType::DiamondGradient: {
      // OOXML has no diamond gradient primitive. The closest preset is a
      // rectangular path gradient, which produces an axis-aligned diamond-like
      // pattern when the focus rect is collapsed to the center point.
      auto* grad = static_cast<const DiamondGradient*>(source);
      out.openElement("a:gradFill").addRequiredAttribute("rotWithShape", "1").closeElementStart();
      writeGradientStops(out, grad->colorStops);
      out.openElement("a:path").addRequiredAttribute("path", "rect").closeElementStart();
      WriteFillToRectFromCenter(out, grad, shapeBounds);
      out.closeElement();  // a:path
      out.closeElement();  // a:gradFill
      break;
    }
    case NodeType::ImagePattern:
      writeImagePatternFill(out, static_cast<const ImagePattern*>(source), alpha, shapeBounds);
      break;
    default:
      out.openElement("a:noFill").closeElementSelfClosing();
      break;
  }
}

void PPTWriter::writeImagePatternFill(XMLBuilder& out, const ImagePattern* pattern, float alpha,
                                      const Rect& shapeBounds) {
  if (!pattern->image) {
    out.openElement("a:noFill").closeElementSelfClosing();
    return;
  }

  bool needsNativeTile =
      (pattern->tileModeX == TileMode::Repeat || pattern->tileModeX == TileMode::Mirror ||
       pattern->tileModeY == TileMode::Repeat || pattern->tileModeY == TileMode::Mirror);
  // Clamp also needs the bake path: OOXML can't express "extend edge pixels"
  // natively, and emitting just <a:srcRect>/<a:fillRect> would either tile
  // (wrong) or stretch the whole image (also wrong).  Repeat/Mirror prefer the
  // bake too because it preserves the pattern's shader matrix exactly.
  bool needsBake =
      needsNativeTile || pattern->tileModeX == TileMode::Clamp ||
      pattern->tileModeY == TileMode::Clamp;

  if (needsBake && _bakeTiledPattern && !shapeBounds.isEmpty()) {
    int w = static_cast<int>(ceilf(shapeBounds.width));
    int h = static_cast<int>(ceilf(shapeBounds.height));
    float offsetX = pattern->matrix.tx - shapeBounds.x;
    float offsetY = pattern->matrix.ty - shapeBounds.y;
    auto tiledPng = RenderTiledPattern(&_gpu, pattern, w, h, offsetX, offsetY);
    if (tiledPng) {
      auto relId = _ctx->addRawImage(std::move(tiledPng));
      out.openElement("a:blipFill").closeElementStart();
      WriteBlip(out, relId, alpha);
      WriteDefaultStretch(out);
      out.closeElement();  // a:blipFill
      return;
    }
  }

  std::string relId = _ctx->addImage(pattern->image);
  if (relId.empty()) {
    out.openElement("a:noFill").closeElementSelfClosing();
    return;
  }

  int imgW = 0;
  int imgH = 0;
  bool hasDimensions = GetImageDimensions(pattern->image, &imgW, &imgH);

  out.openElement("a:blipFill").closeElementStart();
  WriteBlip(out, relId, alpha);

  if (needsNativeTile && hasDimensions && !shapeBounds.isEmpty()) {
    const auto& M = pattern->matrix;
    float imgDpiX = 72.0f;
    float imgDpiY = 72.0f;
    GetImageDPI(pattern->image, &imgDpiX, &imgDpiY);
    double dpiCorrX = static_cast<double>(imgDpiX) / 96.0;
    double dpiCorrY = static_cast<double>(imgDpiY) / 96.0;
    auto sx = static_cast<int>(std::round(std::sqrt(M.a * M.a + M.b * M.b) * dpiCorrX * 100000.0));
    auto sy = static_cast<int>(std::round(std::sqrt(M.c * M.c + M.d * M.d) * dpiCorrY * 100000.0));
    auto tx = PxToEMU(M.tx - shapeBounds.x);
    auto ty = PxToEMU(M.ty - shapeBounds.y);
    bool flipX = (pattern->tileModeX == TileMode::Mirror);
    bool flipY = (pattern->tileModeY == TileMode::Mirror);
    const char* flip = "none";
    if (flipX && flipY) {
      flip = "xy";
    } else if (flipX) {
      flip = "x";
    } else if (flipY) {
      flip = "y";
    }
    out.openElement("a:tile")
        .addRequiredAttribute("tx", tx)
        .addRequiredAttribute("ty", ty)
        .addRequiredAttribute("sx", sx)
        .addRequiredAttribute("sy", sy)
        .addRequiredAttribute("flip", flip)
        .addRequiredAttribute("algn", "tl")
        .closeElementSelfClosing();
  } else {
    bool hasTransform = hasDimensions && !shapeBounds.isEmpty() && !pattern->matrix.isIdentity();
    ImagePatternRect ipr = {};
    if (hasTransform && ComputeImagePatternRect(pattern, imgW, imgH, shapeBounds, &ipr)) {
      out.openElement("a:srcRect");
      AddLTRBAttrs(out, {ipr.srcL, ipr.srcT, ipr.srcR, ipr.srcB});
      out.closeElementSelfClosing();
      LTRBInsets fill;
      fill.l =
          static_cast<int>(std::round((ipr.visL - shapeBounds.x) / shapeBounds.width * 100000.0f));
      fill.t =
          static_cast<int>(std::round((ipr.visT - shapeBounds.y) / shapeBounds.height * 100000.0f));
      fill.r = static_cast<int>(std::round((shapeBounds.x + shapeBounds.width - ipr.visR) /
                                           shapeBounds.width * 100000.0f));
      fill.b = static_cast<int>(std::round((shapeBounds.y + shapeBounds.height - ipr.visB) /
                                           shapeBounds.height * 100000.0f));
      out.openElement("a:stretch").closeElementStart();
      out.openElement("a:fillRect");
      AddLTRBAttrs(out, fill);
      out.closeElementSelfClosing();
      out.closeElement();  // a:stretch
    } else {
      WriteDefaultStretch(out);
    }
  }

  out.closeElement();  // a:blipFill
}

// ── Fill / stroke ──────────────────────────────────────────────────────────

void PPTWriter::writeSolidColorFill(XMLBuilder& out, const Color& color, float alpha) {
  out.openElement("a:solidFill").closeElementStart();
  WriteSrgbClr(out, color, color.alpha * alpha);
  out.closeElement();  // a:solidFill
}

void PPTWriter::writeFill(XMLBuilder& out, const Fill* fill, float alpha, const Rect& shapeBounds) {
  if (!fill) {
    out.openElement("a:noFill").closeElementSelfClosing();
    return;
  }
  float effectiveAlpha = fill->alpha * alpha;
  if (!fill->color) {
    // Per PAGX spec, Fill defaults to opaque black (#000000) when no color is specified.
    writeSolidColorFill(out, Color{}, effectiveAlpha);
    return;
  }
  writeColorSource(out, fill->color, effectiveAlpha, shapeBounds);
}

void PPTWriter::writeStroke(XMLBuilder& out, const Stroke* stroke, float alpha) {
  if (!stroke) {
    out.openElement("a:ln").closeElementStart();
    out.openElement("a:noFill").closeElementSelfClosing();
    out.closeElement();
    return;
  }

  int64_t w = PxToEMU(stroke->width);
  out.openElement("a:ln").addRequiredAttribute("w", w);
  if (stroke->cap == LineCap::Round) {
    out.addRequiredAttribute("cap", "rnd");
  } else if (stroke->cap == LineCap::Square) {
    out.addRequiredAttribute("cap", "sq");
  }
  out.closeElementStart();

  float effectiveAlpha = stroke->alpha * alpha;
  if (stroke->color) {
    writeColorSource(out, stroke->color, effectiveAlpha);
  } else {
    // Per PAGX spec, Stroke defaults to opaque black (#000000) when no color is specified.
    writeSolidColorFill(out, Color{}, effectiveAlpha);
  }

  if (!stroke->dashes.empty()) {
    float sw = (stroke->width > 0) ? stroke->width : 1.0f;
    const char* preset = MatchPresetDash(stroke->dashes, sw);
    if (preset) {
      out.openElement("a:prstDash").addRequiredAttribute("val", preset).closeElementSelfClosing();
    } else {
      out.openElement("a:custDash").closeElementStart();
      for (size_t i = 0; i + 1 < stroke->dashes.size(); i += 2) {
        int d = std::max(1, static_cast<int>(std::round(stroke->dashes[i] / sw * 100000.0)));
        int sp = std::max(1, static_cast<int>(std::round(stroke->dashes[i + 1] / sw * 100000.0)));
        out.openElement("a:ds")
            .addRequiredAttribute("d", d)
            .addRequiredAttribute("sp", sp)
            .closeElementSelfClosing();
      }
      if (stroke->dashes.size() % 2 != 0) {
        int d = std::max(1, static_cast<int>(std::round(stroke->dashes.back() / sw * 100000.0)));
        out.openElement("a:ds")
            .addRequiredAttribute("d", d)
            .addRequiredAttribute("sp", d)
            .closeElementSelfClosing();
      }
      out.closeElement();  // a:custDash
    }
  }

  if (stroke->join == LineJoin::Round) {
    out.openElement("a:round").closeElementSelfClosing();
  } else if (stroke->join == LineJoin::Bevel) {
    out.openElement("a:bevel").closeElementSelfClosing();
  } else {
    int lim = static_cast<int>(std::round(stroke->miterLimit * 100000.0f));
    out.openElement("a:miter").addRequiredAttribute("lim", lim).closeElementSelfClosing();
  }

  out.closeElement();  // a:ln
}

// ── Effects (shadow) ───────────────────────────────────────────────────────

void PPTWriter::writeShadowElement(XMLBuilder& out, const char* tag, float blurX, float blurY,
                                   float offsetX, float offsetY, const Color& color,
                                   bool includeAlign) {
  float blur = (blurX + blurY) / 2.0f;
  float dist = std::sqrt(offsetX * offsetX + offsetY * offsetY);
  float dir = RadiansToDegrees(std::atan2(offsetY, offsetX));
  auto& builder = out.openElement(tag)
                      .addRequiredAttribute("blurRad", PxToEMU(blur))
                      .addRequiredAttribute("dist", PxToEMU(dist))
                      .addRequiredAttribute("dir", AngleToPPT(dir + 90.0f));
  if (includeAlign) {
    builder.addRequiredAttribute("algn", "ctr").addRequiredAttribute("rotWithShape", "0");
  }
  builder.closeElementStart();
  out.openElement("a:srgbClr").addRequiredAttribute("val", ColorToHex6(color)).closeElementStart();
  out.openElement("a:alpha")
      .addRequiredAttribute("val", AlphaToPct(color.alpha))
      .closeElementSelfClosing();
  out.closeElement();  // a:srgbClr
  out.closeElement();  // tag
}

static const char* BlendModeToPPT(BlendMode mode) {
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

void PPTWriter::writeEffects(XMLBuilder& out, const std::vector<LayerFilter*>& filters,
                             const std::vector<LayerStyle*>& styles) {
  bool hasEffects = false;
  for (const auto* f : filters) {
    auto type = f->nodeType();
    if (type == NodeType::DropShadowFilter || type == NodeType::InnerShadowFilter ||
        type == NodeType::BlurFilter || type == NodeType::BlendFilter) {
      hasEffects = true;
      break;
    }
  }
  if (!hasEffects) {
    for (const auto* s : styles) {
      auto type = s->nodeType();
      if (type == NodeType::DropShadowStyle || type == NodeType::InnerShadowStyle ||
          type == NodeType::BackgroundBlurStyle) {
        hasEffects = true;
        break;
      }
    }
  }
  if (!hasEffects) {
    return;
  }

  out.openElement("a:effectLst").closeElementStart();

  // OOXML effectLst requires this element order (§20.1.8.20):
  // blur, fillOverlay, glow, innerShdw, outerShdw, prstShdw, reflection, softEdge

  // BlurFilter (filter) takes precedence over BackgroundBlurStyle. PPT has no native
  // background-blur primitive; we emit a:blur as a best-effort approximation that
  // blurs the shape itself rather than the background behind it.
  bool blurEmitted = false;
  for (const auto* f : filters) {
    if (f->nodeType() == NodeType::BlurFilter) {
      auto* blur = static_cast<const BlurFilter*>(f);
      float avgBlur = (blur->blurX + blur->blurY) / 2.0f;
      if (avgBlur > 0) {
        out.openElement("a:blur")
            .addRequiredAttribute("rad", PxToEMU(avgBlur))
            .addRequiredAttribute("grow", "0")
            .closeElementSelfClosing();
        blurEmitted = true;
      }
      break;
    }
  }
  if (!blurEmitted) {
    for (const auto* s : styles) {
      if (s->nodeType() == NodeType::BackgroundBlurStyle) {
        auto* bg = static_cast<const BackgroundBlurStyle*>(s);
        float avgBlur = (bg->blurX + bg->blurY) / 2.0f;
        if (avgBlur > 0) {
          out.openElement("a:blur")
              .addRequiredAttribute("rad", PxToEMU(avgBlur))
              .addRequiredAttribute("grow", "0")
              .closeElementSelfClosing();
        }
        break;
      }
    }
  }

  for (const auto* f : filters) {
    if (f->nodeType() == NodeType::BlendFilter) {
      auto* blend = static_cast<const BlendFilter*>(f);
      const char* blendStr = BlendModeToPPT(blend->blendMode);
      if (blendStr) {
        out.openElement("a:fillOverlay")
            .addRequiredAttribute("blend", blendStr)
            .closeElementStart();
        out.openElement("a:solidFill").closeElementStart();
        WriteSrgbClr(out, blend->color, blend->color.alpha);
        out.closeElement();  // a:solidFill
        out.closeElement();  // a:fillOverlay
      }
      break;
    }
  }

  for (const auto* f : filters) {
    if (f->nodeType() == NodeType::InnerShadowFilter) {
      auto* s = static_cast<const InnerShadowFilter*>(f);
      writeShadowElement(out, "a:innerShdw", s->blurX, s->blurY, s->offsetX, s->offsetY, s->color,
                         false);
    }
  }
  for (const auto* s : styles) {
    if (s->nodeType() == NodeType::InnerShadowStyle) {
      auto* st = static_cast<const InnerShadowStyle*>(s);
      writeShadowElement(out, "a:innerShdw", st->blurX, st->blurY, st->offsetX, st->offsetY,
                         st->color, false);
    }
  }
  for (const auto* f : filters) {
    if (f->nodeType() == NodeType::DropShadowFilter) {
      auto* s = static_cast<const DropShadowFilter*>(f);
      writeShadowElement(out, "a:outerShdw", s->blurX, s->blurY, s->offsetX, s->offsetY, s->color,
                         true);
    }
  }
  for (const auto* s : styles) {
    if (s->nodeType() == NodeType::DropShadowStyle) {
      auto* st = static_cast<const DropShadowStyle*>(s);
      writeShadowElement(out, "a:outerShdw", st->blurX, st->blurY, st->offsetX, st->offsetY,
                         st->color, true);
    }
  }

  out.closeElement();  // a:effectLst
}

// ── Custom geometry ────────────────────────────────────────────────────────

void PPTWriter::WriteContourGeom(XMLBuilder& out, std::vector<PathContour>& contours,
                                 int64_t pathWidth, int64_t pathHeight, float scaleX, float scaleY,
                                 float scaledOfsX, float scaledOfsY, FillRule fillRule) {
  if (_bridgeContours && contours.size() > 1) {
    auto groups = PrepareContourGroups(contours, fillRule);
    EmitContourGeomFromGroups(out, contours, groups, pathWidth, pathHeight, scaleX, scaleY,
                              scaledOfsX, scaledOfsY);
    return;
  }
  EmitFlatContourGeom(out, contours, pathWidth, pathHeight, scaleX, scaleY, scaledOfsX,
                      scaledOfsY);
}

// ── Shape writers ──────────────────────────────────────────────────────────

void PPTWriter::writeRectangle(XMLBuilder& out, const Rectangle* rect, const FillStrokeInfo& fs,
                               const Matrix& m, float alpha,
                               const std::vector<LayerFilter*>& filters,
                               const std::vector<LayerStyle*>& styles) {
  float x = rect->position.x - rect->size.width / 2.0f;
  float y = rect->position.y - rect->size.height / 2.0f;
  float w = rect->size.width;
  float h = rect->size.height;
  float roundness = rect->roundness;

  // OOXML strokes are always centre-aligned on the geometry, so to mimic
  // StrokeAlign::Inside / Outside we shift this stroke painter's geometry in /
  // out by half the stroke width.  Fill-only painters skip this branch (they
  // see fs.stroke == nullptr from processVectorScope), so the fill geometry
  // remains untouched.
  ApplyStrokeBoxInset(fs.stroke, x, y, w, h, &roundness);

  Rect shapeBounds = Rect::MakeXYWH(x, y, w, h);

  bool imageWritten = writeImagePatternAsPicture(out, fs.fill, shapeBounds, m, alpha);
  if (imageWritten && !fs.stroke && filters.empty() && styles.empty()) {
    return;
  }

  auto xf = decomposeXform(x, y, w, h, m);
  beginShape(out, "Rectangle", xf.offX, xf.offY, xf.extCX, xf.extCY, xf.rotation);

  if (roundness > 0) {
    float minSide = std::min(w, h);
    int adj =
        (minSide > 0) ? std::clamp(static_cast<int>(roundness * 100000.0f / minSide), 0, 50000) : 0;
    out.openElement("a:prstGeom").addRequiredAttribute("prst", "roundRect").closeElementStart();
    out.openElement("a:avLst").closeElementStart();
    out.openElement("a:gd")
        .addRequiredAttribute("name", "adj")
        .addRequiredAttribute("fmla", "val " + std::to_string(adj))
        .closeElementSelfClosing();
    out.closeElement();  // a:avLst
    out.closeElement();  // a:prstGeom
  } else {
    out.openElement("a:prstGeom").addRequiredAttribute("prst", "rect").closeElementStart();
    out.openElement("a:avLst").closeElementSelfClosing();
    out.closeElement();
  }

  writeShapeTail(out, fs, alpha, shapeBounds, imageWritten, filters, styles);
}

void PPTWriter::writeEllipse(XMLBuilder& out, const Ellipse* ellipse, const FillStrokeInfo& fs,
                             const Matrix& m, float alpha, const std::vector<LayerFilter*>& filters,
                             const std::vector<LayerStyle*>& styles) {
  float rx = ellipse->size.width / 2.0f;
  float ry = ellipse->size.height / 2.0f;
  float x = ellipse->position.x - rx;
  float y = ellipse->position.y - ry;
  float w = ellipse->size.width;
  float h = ellipse->size.height;

  // See writeRectangle: emulate StrokeAlign::Inside / Outside by inset/outset
  // because OOXML can only draw centre-aligned strokes on the geometry.
  ApplyStrokeBoxInset(fs.stroke, x, y, w, h);

  Rect shapeBounds = Rect::MakeXYWH(x, y, w, h);

  bool imageWritten = writeImagePatternAsPicture(out, fs.fill, shapeBounds, m, alpha);
  if (imageWritten && !fs.stroke && filters.empty() && styles.empty()) {
    return;
  }

  auto xf = decomposeXform(x, y, w, h, m);
  beginShape(out, "Ellipse", xf.offX, xf.offY, xf.extCX, xf.extCY, xf.rotation);

  out.openElement("a:prstGeom").addRequiredAttribute("prst", "ellipse").closeElementStart();
  out.openElement("a:avLst").closeElementSelfClosing();
  out.closeElement();

  writeShapeTail(out, fs, alpha, shapeBounds, imageWritten, filters, styles);
}

void PPTWriter::writePath(XMLBuilder& out, const Path* path, const FillStrokeInfo& fs,
                          const Matrix& m, float alpha, const std::vector<LayerFilter*>& filters,
                          const std::vector<LayerStyle*>& styles) {
  if (!path->data || path->data->isEmpty()) {
    return;
  }
  Rect bounds = path->data->getBounds();
  if (bounds.width <= 0 && bounds.height <= 0) {
    return;
  }

  float strokePad = (fs.stroke && fs.stroke->width > 0) ? fs.stroke->width : 0;
  float minDim = std::max(1.0f, strokePad);
  float adjustedW = std::max(bounds.width, minDim);
  float adjustedH = std::max(bounds.height, minDim);
  float adjustedX = bounds.x - (adjustedW - bounds.width) / 2.0f;
  float adjustedY = bounds.y - (adjustedH - bounds.height) / 2.0f;
  Rect shapeBounds = Rect::MakeXYWH(adjustedX, adjustedY, adjustedW, adjustedH);

  bool imageWritten = writeImagePatternAsPicture(out, fs.fill, shapeBounds, m, alpha);
  if (imageWritten && !fs.stroke && filters.empty() && styles.empty()) {
    return;
  }

  auto xf = decomposeXform(adjustedX, adjustedY, adjustedW, adjustedH, m);
  FillRule fillRule = (fs.fill) ? fs.fill->fillRule : FillRule::Winding;

  auto contours = ParsePathContours(path->data);
  int64_t pw = std::max(int64_t(1), PxToEMU(adjustedW));
  int64_t ph = std::max(int64_t(1), PxToEMU(adjustedH));

  if (_bridgeContours && contours.size() > 1) {
    auto groups = PrepareContourGroups(contours, fillRule);
    if (groups.size() > 1) {
      for (const auto& group : groups) {
        beginShape(out, "Path", xf.offX, xf.offY, xf.extCX, xf.extCY, xf.rotation);
        EmitGroupCustGeom(out, contours, group, pw, ph, 1.0f, 1.0f, adjustedX, adjustedY);
        writeShapeTail(out, fs, alpha, shapeBounds, imageWritten, filters, styles);
      }
      return;
    }
    // Single bridged group: skip the redundant grouping pass that
    // WriteContourGeom would otherwise perform and emit directly.
    beginShape(out, "Path", xf.offX, xf.offY, xf.extCX, xf.extCY, xf.rotation);
    EmitContourGeomFromGroups(out, contours, groups, pw, ph, 1.0f, 1.0f, adjustedX, adjustedY);
    writeShapeTail(out, fs, alpha, shapeBounds, imageWritten, filters, styles);
    return;
  }

  beginShape(out, "Path", xf.offX, xf.offY, xf.extCX, xf.extCY, xf.rotation);
  WriteContourGeom(out, contours, pw, ph, 1.0f, 1.0f, adjustedX, adjustedY, fillRule);
  writeShapeTail(out, fs, alpha, shapeBounds, imageWritten, filters, styles);
}

void PPTWriter::writeTextAsPath(XMLBuilder& out, const Text* text, const FillStrokeInfo& fs,
                                const Matrix& m, float alpha,
                                const std::vector<LayerFilter*>& /*filters*/,
                                const std::vector<LayerStyle*>& /*styles*/) {
  auto glyphPaths = ComputeGlyphPaths(*text, text->position.x, text->position.y);
  auto glyphImages = ComputeGlyphImages(*text, text->position.x, text->position.y);
  if (glyphPaths.empty() && glyphImages.empty()) {
    return;
  }

  // ── Bitmap glyphs ─────────────────────────────────────────────────────────
  // Each bitmap glyph is emitted as its own p:pic; the glyph's image lives in
  // pixel coords, so we compose the per-glyph transform with the outer matrix
  // and let decomposeXform turn the image rect into <a:xfrm>. Per-glyph skew
  // can't be expressed in OOXML and is silently dropped (matches the behaviour
  // for other unsupported transform components elsewhere in this exporter).
  for (const auto& gi : glyphImages) {
    if (!gi.image) {
      continue;
    }
    int imgW = 0;
    int imgH = 0;
    if (!GetImageDimensions(gi.image, &imgW, &imgH) || imgW <= 0 || imgH <= 0) {
      continue;
    }
    std::string relId = _ctx->addImage(gi.image);
    if (relId.empty()) {
      continue;
    }
    Matrix combined = m * gi.transform;
    auto xf =
        decomposeXform(0.0f, 0.0f, static_cast<float>(imgW), static_cast<float>(imgH), combined);
    beginPicture(out, "GlyphImage");
    out.openElement("p:blipFill").closeElementStart();
    WriteBlip(out, relId, alpha);
    WriteDefaultStretch(out);
    out.closeElement();  // p:blipFill
    endPicture(out, xf);
  }

  if (glyphPaths.empty()) {
    return;
  }

  // Merge all glyph paths into a single compound shape to guarantee baseline
  // alignment.  Per-glyph shapes suffer from independent EMU rounding of offY
  // and extCY, which causes visible baseline jitter on some mobile renderers.
  std::vector<PathContour> allContours;
  for (const auto& gp : glyphPaths) {
    if (!gp.pathData || gp.pathData->isEmpty()) {
      continue;
    }
    AppendTransformedContours(allContours, gp.pathData, gp.transform);
  }

  if (allContours.empty()) {
    return;
  }

  // Compute bounding box using segment endpoints only (polygon approximation).
  // For cubic bezier segments the actual curve may exceed the endpoint hull, but
  // glyph outlines are dense enough that the deviation is negligible in practice.
  float minX = std::numeric_limits<float>::max();
  float minY = std::numeric_limits<float>::max();
  float maxX = std::numeric_limits<float>::lowest();
  float maxY = std::numeric_limits<float>::lowest();
  for (const auto& contour : allContours) {
    ExpandBounds(minX, minY, maxX, maxY, contour.start);
    for (const auto& seg : contour.segs) {
      int n = PathData::PointsPerVerb(seg.verb);
      for (int i = 0; i < n; i++) {
        ExpandBounds(minX, minY, maxX, maxY, seg.pts[i]);
      }
    }
  }

  if (maxX <= minX || maxY <= minY) {
    return;
  }

  float boundsW = maxX - minX;
  float boundsH = maxY - minY;

  float sx = 0;
  float sy = 0;
  DecomposeScale(m, &sx, &sy);
  if (sx <= 0) sx = 1.0f;
  if (sy <= 0) sy = 1.0f;

  auto xf = decomposeXform(minX, minY, boundsW, boundsH, m);
  int64_t pw = std::max(int64_t(1), PxToEMU(boundsW * sx));
  int64_t ph = std::max(int64_t(1), PxToEMU(boundsH * sy));
  float scaledOfsX = minX * sx;
  float scaledOfsY = minY * sy;

  if (_bridgeContours && allContours.size() > 1) {
    auto groups = PrepareContourGroups(allContours, FillRule::EvenOdd);
    for (const auto& group : groups) {
      beginShape(out, "Glyph", xf.offX, xf.offY, xf.extCX, xf.extCY, xf.rotation);
      EmitGroupCustGeom(out, allContours, group, pw, ph, sx, sy, scaledOfsX, scaledOfsY);
      writeGlyphShape(out, fs.fill, alpha);
    }
  } else {
    beginShape(out, "Glyph", xf.offX, xf.offY, xf.extCX, xf.extCY, xf.rotation);
    WriteContourGeom(out, allContours, pw, ph, sx, sy, scaledOfsX, scaledOfsY, FillRule::EvenOdd);
    writeGlyphShape(out, fs.fill, alpha);
  }
}

void PPTWriter::writeNativeText(XMLBuilder& out, const Text* text, const FillStrokeInfo& fs,
                                const Matrix& m, float alpha,
                                const std::vector<LayerFilter*>& filters,
                                const std::vector<LayerStyle*>& styles,
                                const TextLayoutResult* precomputed) {
  if (text->text.empty()) {
    return;
  }

  auto* mutableText = const_cast<Text*>(text);
  float boxWidth = fs.textBox ? EffectiveTextBoxWidth(fs.textBox) : NAN;
  float boxHeight = fs.textBox ? EffectiveTextBoxHeight(fs.textBox) : NAN;
  bool hasTextBox = fs.textBox && !std::isnan(boxWidth) && boxWidth > 0;

  TextLayoutResult localResult;
  if (!precomputed) {
    auto params = hasTextBox ? MakeTextBoxParams(fs.textBox) : MakeStandaloneParams(text);
    localResult = TextLayout::Layout({mutableText}, params, _layoutContext);
    precomputed = &localResult;
  }
  auto* lines = precomputed->getTextLines(mutableText);

  float posX = 0;
  float posY = 0;
  float estWidth = 0;
  float estHeight = 0;

  if (hasTextBox) {
    posX = fs.textBox->position.x;
    posY = fs.textBox->position.y;
    estWidth = boxWidth;
    estHeight = (!std::isnan(boxHeight) && boxHeight > 0) ? boxHeight : text->fontSize * 1.4f;
  } else {
    auto textBounds = precomputed->getTextBounds(mutableText);
    if (textBounds.width > 0 && textBounds.height > 0) {
      posX = text->position.x + textBounds.x;
      posY = text->position.y + textBounds.y;
      estWidth = textBounds.width;
      estHeight = textBounds.height;
    } else {
      estWidth = static_cast<float>(CountUTF8Characters(text->text)) * text->fontSize * 0.6f;
      estHeight = text->fontSize * 1.4f;
      posX = text->position.x;
      posY = text->position.y - text->fontSize * 0.85f;
      if (text->textAnchor == TextAnchor::Center) {
        posX -= estWidth / 2.0f;
      } else if (text->textAnchor == TextAnchor::End) {
        posX -= estWidth;
      }
    }
  }

  auto xf = decomposeXform(posX, posY, estWidth, estHeight, m);

  int id = _ctx->nextShapeId();
  out.openElement("p:sp").closeElementStart();
  out.openElement("p:nvSpPr").closeElementStart();
  out.openElement("p:cNvPr")
      .addRequiredAttribute("id", id)
      .addRequiredAttribute("name", "TextBox")
      .closeElementSelfClosing();
  out.openElement("p:cNvSpPr").addRequiredAttribute("txBox", "1").closeElementSelfClosing();
  out.openElement("p:nvPr").closeElementSelfClosing();
  out.closeElement();  // p:nvSpPr

  out.openElement("p:spPr").closeElementStart();
  WriteXfrm(out, xf);
  out.openElement("a:prstGeom").addRequiredAttribute("prst", "rect").closeElementStart();
  out.openElement("a:avLst").closeElementSelfClosing();
  out.closeElement();
  out.openElement("a:noFill").closeElementSelfClosing();
  out.openElement("a:ln").closeElementStart();
  out.openElement("a:noFill").closeElementSelfClosing();
  out.closeElement();
  // Shadow effects on the surrounding p:spPr would apply to the text-box rectangle
  // rather than the text itself; emit them inside a:rPr below so the shadow follows
  // the actual glyph outlines.
  out.closeElement();  // p:spPr

  // When PAGX layout produced explicit per-line byte ranges we emit them as
  // soft <a:br/> breaks within a single paragraph ourselves, so PowerPoint
  // shouldn't perform additional line-wrapping on top (its font metrics differ
  // slightly from PAGX's, which would shift the break points). Vertical layout
  // has no line info; fall back to PowerPoint-driven wrapping in that case.
  bool useLineLayout = (lines != nullptr) && !lines->empty();
  // Justify alignment requires PowerPoint to know a target line width; with
  // wrap="none" the text is unbounded so PPT silently falls back to start
  // alignment. Use wrap="square" in that case so PPT can justify within the
  // shape's text area (our PAGX-determined visual lines should fit, so PPT
  // shouldn't introduce additional wraps).
  bool justifyAlign = fs.textBox && fs.textBox->textAlign == TextAlign::Justify;

  out.openElement("p:txBody").closeElementStart();
  out.openElement("a:bodyPr")
      .addRequiredAttribute("wrap", useLineLayout ? (justifyAlign ? "square" : "none")
                                                  : (hasTextBox ? "square" : "none"))
      .addRequiredAttribute("lIns", "0")
      .addRequiredAttribute("tIns", "0")
      .addRequiredAttribute("rIns", "0")
      .addRequiredAttribute("bIns", "0");
  AddBodyPrAttrsForTextBox(out, fs.textBox);
  out.closeElementSelfClosing();
  out.openElement("a:lstStyle").closeElementSelfClosing();

  PPTRunStyle style = BuildRunStyle(text, fs.fill, alpha);
  if (text->textAnchor == TextAnchor::Center) {
    style.algn = "ctr";
  } else if (text->textAnchor == TextAnchor::End) {
    style.algn = "r";
  }
  if (fs.textBox) {
    if (fs.textBox->textAlign == TextAlign::Center) {
      style.algn = "ctr";
    } else if (fs.textBox->textAlign == TextAlign::End) {
      style.algn = "r";
    } else if (fs.textBox->textAlign == TextAlign::Justify) {
      style.algn = "just";
    }
  }

  int64_t lnSpcPts = fs.textBox ? LineHeightToSpcPts(fs.textBox->lineHeight) : 0;

  if (useLineLayout) {
    // Emit ALL visual lines inside a single <a:p> separated by soft <a:br/>
    // breaks. OOXML's algn="just" never justifies the last line of a
    // paragraph, so isolating each line in its own paragraph would disable
    // justify entirely; soft breaks within one paragraph make all lines
    // except the semantically last participate in justify.
    bool wroteAny = false;
    out.openElement("a:p").closeElementStart();
    WriteParagraphProperties(out, style.algn, lnSpcPts);
    for (const auto& lineInfo : *lines) {
      if (lineInfo.byteStart >= lineInfo.byteEnd) {
        continue;
      }
      std::string line =
          text->text.substr(lineInfo.byteStart, lineInfo.byteEnd - lineInfo.byteStart);
      if (line.empty()) {
        continue;
      }
      if (wroteAny) {
        writeLineBreak(out, style);
      }
      writeParagraphRun(out, line, style, filters, styles);
      wroteAny = true;
    }
    out.closeElement();  // a:p
  } else {
    const std::string& remaining = text->text;
    size_t pos = 0;
    while (pos <= remaining.size()) {
      size_t nl = remaining.find('\n', pos);
      std::string line =
          (nl == std::string::npos) ? remaining.substr(pos) : remaining.substr(pos, nl - pos);
      writeParagraph(out, line, style, filters, styles, lnSpcPts);
      if (nl == std::string::npos) {
        break;
      }
      pos = nl + 1;
    }
  }

  out.closeElement();  // p:txBody
  out.closeElement();  // p:sp
}

void PPTWriter::writeParagraphRun(XMLBuilder& out, const std::string& runText,
                                  const PPTRunStyle& style,
                                  const std::vector<LayerFilter*>& filters,
                                  const std::vector<LayerStyle*>& styles) {
  out.openElement("a:r").closeElementStart();
  out.openElement("a:rPr")
      .addRequiredAttribute("lang", "en-US")
      .addRequiredAttribute("sz", style.fontSize);
  if (style.hasBold) {
    out.addRequiredAttribute("b", "1");
  }
  if (style.hasItalic) {
    out.addRequiredAttribute("i", "1");
  }
  if (style.hasLetterSpacing) {
    out.addRequiredAttribute("spc", style.letterSpc);
  }
  out.closeElementStart();
  if (style.hasFillColor) {
    // For gradient fills, shape bounds are unknown at the run level; pass an empty
    // rect so writeColorSource falls back to the gradient's own coordinate space.
    writeColorSource(out, style.fillColor, style.fillAlpha);
  }
  writeEffects(out, filters, styles);
  if (!style.typeface.empty()) {
    out.openElement("a:latin")
        .addRequiredAttribute("typeface", style.typeface)
        .closeElementSelfClosing();
    out.openElement("a:ea")
        .addRequiredAttribute("typeface", style.typeface)
        .closeElementSelfClosing();
  }
  out.closeElement();  // a:rPr
  out.openElement("a:t").closeElementStart();
  out.addTextContent(runText);
  out.closeElement();  // a:t
  out.closeElement();  // a:r
}

void PPTWriter::writeLineBreak(XMLBuilder& out, const PPTRunStyle& style) {
  // <a:br><a:rPr sz="..."/></a:br> — the rPr controls the line-height of the
  // break so it matches the surrounding text size (PowerPoint otherwise uses
  // its default body font size, producing visibly mismatched line spacing).
  out.openElement("a:br").closeElementStart();
  out.openElement("a:rPr")
      .addRequiredAttribute("lang", "en-US")
      .addRequiredAttribute("sz", style.fontSize);
  if (style.hasBold) {
    out.addRequiredAttribute("b", "1");
  }
  if (style.hasItalic) {
    out.addRequiredAttribute("i", "1");
  }
  if (!style.typeface.empty()) {
    out.closeElementStart();
    out.openElement("a:latin")
        .addRequiredAttribute("typeface", style.typeface)
        .closeElementSelfClosing();
    out.openElement("a:ea")
        .addRequiredAttribute("typeface", style.typeface)
        .closeElementSelfClosing();
    out.closeElement();  // a:rPr
  } else {
    out.closeElementSelfClosing();
  }
  out.closeElement();  // a:br
}

void PPTWriter::writeParagraph(XMLBuilder& out, const std::string& lineText,
                               const PPTRunStyle& style, const std::vector<LayerFilter*>& filters,
                               const std::vector<LayerStyle*>& styles, int64_t lnSpcPts) {
  out.openElement("a:p").closeElementStart();
  WriteParagraphProperties(out, style.algn, lnSpcPts);
  writeParagraphRun(out, lineText, style, filters, styles);
  out.closeElement();  // a:p
}

// ── TextBox group ──────────────────────────────────────────────────────────

namespace {

// One "run" inside a rich TextBox: a Text element together with the Fill that
// applies to it (either the Group's own Fill or, when the Text is a direct
// child of the TextBox, the TextBox's top-level Fill).
struct RichTextRun {
  const Text* text = nullptr;
  const Fill* fill = nullptr;
};

// Walks the TextBox children in source order, pairing every Text with its
// nearest enclosing Fill. Direct Text children inherit `parentFill`; Texts
// nested inside a Group use that Group's locally-collected Fill (falling back
// to `parentFill` when the Group has none).
void CollectRichTextRuns(const std::vector<Element*>& elements, const Fill* parentFill,
                         std::vector<RichTextRun>& outRuns) {
  for (auto* element : elements) {
    auto type = element->nodeType();
    if (type == NodeType::Text) {
      auto* t = static_cast<const Text*>(element);
      if (!t->text.empty()) {
        outRuns.push_back({t, parentFill});
      }
    } else if (type == NodeType::Group) {
      auto* g = static_cast<const Group*>(element);
      auto groupFs = CollectFillStroke(g->elements);
      const Fill* effectiveFill = groupFs.fill ? groupFs.fill : parentFill;
      CollectRichTextRuns(g->elements, effectiveFill, outRuns);
    }
  }
}

}  // namespace

void PPTWriter::writeTextBoxGroup(XMLBuilder& out, const Group* textBox,
                                  const std::vector<Element*>& elements, const Matrix& transform,
                                  float alpha, const std::vector<LayerFilter*>& filters,
                                  const std::vector<LayerStyle*>& styles) {
  auto* box = static_cast<const TextBox*>(textBox);
  auto topLevelFs = CollectFillStroke(elements);

  std::vector<RichTextRun> runs;
  CollectRichTextRuns(elements, topLevelFs.fill, runs);
  if (runs.empty()) {
    return;
  }

  // Run a layout pass to obtain the textbox's resolved bounds AND its
  // per-Text line-break decisions. We use those line breaks as paragraph
  // boundaries below so PowerPoint reproduces the same wrapping that PAGX
  // computed (PPT's font metrics differ slightly, so leaving wrapping to
  // PowerPoint produces visibly different break points).
  std::vector<Text*> mutableTexts;
  mutableTexts.reserve(runs.size());
  for (const auto& run : runs) {
    mutableTexts.push_back(const_cast<Text*>(run.text));
  }
  auto params = MakeTextBoxParams(box);
  auto layoutResult = TextLayout::Layout(mutableTexts, params, _layoutContext);

  float boxWidth = EffectiveTextBoxWidth(box);
  float boxHeight = EffectiveTextBoxHeight(box);
  bool hasBoxWidth = !std::isnan(boxWidth) && boxWidth > 0;

  // `transform` already incorporates BuildGroupMatrix(box) (applied by the
  // caller in writeElements), so the local origin here is (0, 0). Adding
  // box->position again would double-offset the shape, pushing the text-box
  // away from the centered position the layout assigned to it.
  float estWidth = hasBoxWidth ? boxWidth : layoutResult.bounds.width;
  if (estWidth <= 0) {
    estWidth = static_cast<float>(CountUTF8Characters(runs.front().text->text)) *
               runs.front().text->fontSize * 0.6f;
  }
  float estHeight =
      (!std::isnan(boxHeight) && boxHeight > 0) ? boxHeight : layoutResult.bounds.height;
  if (estHeight <= 0) {
    estHeight = runs.front().text->fontSize * 1.4f;
  }

  auto xf = decomposeXform(0.0f, 0.0f, estWidth, estHeight, transform);

  int id = _ctx->nextShapeId();
  out.openElement("p:sp").closeElementStart();
  out.openElement("p:nvSpPr").closeElementStart();
  out.openElement("p:cNvPr")
      .addRequiredAttribute("id", id)
      .addRequiredAttribute("name", "TextBox")
      .closeElementSelfClosing();
  out.openElement("p:cNvSpPr").addRequiredAttribute("txBox", "1").closeElementSelfClosing();
  out.openElement("p:nvPr").closeElementSelfClosing();
  out.closeElement();  // p:nvSpPr

  out.openElement("p:spPr").closeElementStart();
  WriteXfrm(out, xf);
  out.openElement("a:prstGeom").addRequiredAttribute("prst", "rect").closeElementStart();
  out.openElement("a:avLst").closeElementSelfClosing();
  out.closeElement();
  out.openElement("a:noFill").closeElementSelfClosing();
  out.openElement("a:ln").closeElementStart();
  out.openElement("a:noFill").closeElementSelfClosing();
  out.closeElement();
  out.closeElement();  // p:spPr

  // Per-Text line metadata produced by PAGX's own layout. Use these line
  // breaks as authoritative paragraph boundaries (rather than letting
  // PowerPoint re-wrap with its own font metrics, which doesn't match the
  // PAGX renderer) so the PPT output matches the PAGX rendering exactly.
  // `getTextLines` returns nullptr for layouts where line info isn't tracked
  // (notably vertical writing mode); those fall back to legacy '\n'-splitting
  // and PowerPoint-driven wrapping.
  struct LineEntry {
    size_t runIndex = 0;
    float baselineY = 0;
    uint32_t byteStart = 0;
    uint32_t byteEnd = 0;
  };
  std::vector<LineEntry> lineEntries;
  bool useLineLayout = !runs.empty() && box->writingMode != WritingMode::Vertical;
  if (useLineLayout) {
    for (size_t i = 0; i < runs.size(); ++i) {
      auto* mt = const_cast<Text*>(runs[i].text);
      auto* lines = layoutResult.getTextLines(mt);
      if (lines == nullptr) {
        useLineLayout = false;
        lineEntries.clear();
        break;
      }
      for (const auto& li : *lines) {
        if (li.byteStart >= li.byteEnd) {
          continue;
        }
        lineEntries.push_back({i, li.baselineY, li.byteStart, li.byteEnd});
      }
    }
    if (useLineLayout && lineEntries.empty()) {
      useLineLayout = false;
    }
  }

  const char* algn = nullptr;
  if (box->textAlign == TextAlign::Center) {
    algn = "ctr";
  } else if (box->textAlign == TextAlign::End) {
    algn = "r";
  } else if (box->textAlign == TextAlign::Justify) {
    algn = "just";
  }
  // Justify alignment requires PowerPoint to know a target line width; with
  // wrap="none" the text is unbounded so PPT silently falls back to start
  // alignment. Use wrap="square" in that case so PPT can justify within the
  // shape's text area. Our PAGX-determined visual lines should already fit
  // within the shape, so PPT shouldn't introduce additional wraps.
  bool justifyAlign = (algn != nullptr && std::string(algn) == "just");

  out.openElement("p:txBody").closeElementStart();
  out.openElement("a:bodyPr")
      .addRequiredAttribute("wrap", useLineLayout ? (justifyAlign ? "square" : "none")
                                                  : (hasBoxWidth ? "square" : "none"))
      .addRequiredAttribute("lIns", "0")
      .addRequiredAttribute("tIns", "0")
      .addRequiredAttribute("rIns", "0")
      .addRequiredAttribute("bIns", "0");
  AddBodyPrAttrsForTextBox(out, box);
  out.closeElementSelfClosing();
  out.openElement("a:lstStyle").closeElementSelfClosing();
  int64_t lnSpcPts = LineHeightToSpcPts(box->lineHeight);

  auto writePPr = [&]() { WriteParagraphProperties(out, algn, lnSpcPts); };

  // Build per-run styles up-front (font/size/bold/italic/color/typeface).
  // Alignment lives on a:pPr (not a:rPr) so we leave style.algn at nullptr here.
  std::vector<PPTRunStyle> runStyles;
  runStyles.reserve(runs.size());
  for (const auto& run : runs) {
    runStyles.push_back(BuildRunStyle(run.text, run.fill, alpha));
  }

  bool paragraphOpen = false;
  auto openParagraph = [&]() {
    out.openElement("a:p").closeElementStart();
    writePPr();
    paragraphOpen = true;
  };
  auto closeParagraph = [&]() {
    if (paragraphOpen) {
      out.closeElement();  // a:p
      paragraphOpen = false;
    }
  };
  auto emitRun = [&](const std::string& fragment, const PPTRunStyle& rs) {
    if (fragment.empty()) {
      return;
    }
    if (!paragraphOpen) {
      openParagraph();
    }
    writeParagraphRun(out, fragment, rs, filters, styles);
  };

  if (useLineLayout) {
    // Stable-sort entries by baselineY so that runs from different Text
    // elements sharing the same visual line stay in source order (rich text
    // flows left-to-right within a baseline). Then group consecutive entries
    // with matching baselineY into one visual line.
    std::stable_sort(lineEntries.begin(), lineEntries.end(),
                     [](const LineEntry& a, const LineEntry& b) {
                       return a.baselineY < b.baselineY;
                     });
    constexpr float kBaselineEpsilon = 0.5f;
    // Emit ALL visual lines inside a single <a:p> separated by soft <a:br/>
    // breaks (rather than one <a:p> per line). This is required for justify
    // alignment to behave: OOXML's algn="just" never justifies the last line
    // of a paragraph, so isolating each visual line in its own paragraph
    // turns every line into a "last line" and disables justify entirely.
    // Soft breaks within a single paragraph make all lines except the
    // semantically last one participate in justify.
    openParagraph();
    bool firstLine = true;
    size_t i = 0;
    while (i < lineEntries.size()) {
      float baseline = lineEntries[i].baselineY;
      if (!firstLine) {
        writeLineBreak(out, runStyles[lineEntries[i].runIndex]);
      }
      firstLine = false;
      while (i < lineEntries.size() &&
             std::fabs(lineEntries[i].baselineY - baseline) < kBaselineEpsilon) {
        const auto& entry = lineEntries[i];
        const std::string& src = runs[entry.runIndex].text->text;
        emitRun(src.substr(entry.byteStart, entry.byteEnd - entry.byteStart),
                runStyles[entry.runIndex]);
        ++i;
      }
    }
    closeParagraph();
  } else {
    // Fallback path: stream runs into paragraphs, splitting only on '\n'. A
    // single Text element may carry multiple newlines internally, in which
    // case it contributes to several consecutive paragraphs. PowerPoint
    // performs its own wrapping inside the shape if a paragraph exceeds the
    // box width.
    for (size_t i = 0; i < runs.size(); ++i) {
      const std::string& s = runs[i].text->text;
      const auto& rs = runStyles[i];
      size_t pos = 0;
      while (pos <= s.size()) {
        size_t nl = s.find('\n', pos);
        if (nl == std::string::npos) {
          emitRun(s.substr(pos), rs);
          break;
        }
        emitRun(s.substr(pos, nl - pos), rs);
        // Newline terminates the current paragraph; if the paragraph is still
        // empty (e.g. consecutive '\n's), open and immediately close it so the
        // blank line is preserved.
        if (!paragraphOpen) {
          openParagraph();
        }
        closeParagraph();
        pos = nl + 1;
      }
    }
    closeParagraph();
  }

  out.closeElement();  // p:txBody
  out.closeElement();  // p:sp
}

// ── Element / layer traversal ──────────────────────────────────────────────
//
// PAGX's vector-element model is "accumulate-render": each Layer maintains an
// implicit geometry list that grows as Rectangle / Ellipse / Path / Text
// elements are encountered in document order. A Painter (Fill or Stroke)
// renders every geometry currently in scope; geometry stays in the list, so
// subsequent painters can render it again (multi-fill / multi-stroke).
//
// Group is a *one-way* scope:
//   * Painters declared inside a Group only see geometry added inside that
//     same Group (the scope_start index in `processVectorScope` enforces this).
//   * Geometry added inside a Group propagates upward when the Group exits, so
//     painters at the outer scope can still render that geometry — this is how
//     `<Group><Rect/></Group><Fill/>` ends up filling the rect (the
//     "Propagation" case in samples/group.pagx).
//
// PowerPoint shapes only carry a single fill + single stroke, so each painter
// emits its own copy of every visible geometry (`emitGeometryWithFs`); shapes
// rendered later overlap earlier ones, matching PAGX's "later painter renders
// on top" rule. This is also what makes the multi-fill / multi-stroke samples
// render correctly.
//
// Layer is the accumulation boundary — geometry never crosses a Layer boundary,
// so the accumulator is created fresh per `writeElements` call.

namespace {

// Look for the first non-container <TextBox> in `elements` so it can be
// associated with sibling Text geometry (matches the legacy
// CollectFillStroke().textBox behaviour). Container TextBoxes (those with
// children) are handled separately as scopes by processVectorScope.
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

}  // namespace

// Dispatch a single accumulated geometry through the appropriate per-shape
// writer with the given Painter applied. Each painter that renders a geometry
// goes through this function so that multi-fill / multi-stroke produces one
// PowerPoint shape per painter (overlapping in document order).
void PPTWriter::emitGeometryWithFs(XMLBuilder& out, const AccumulatedGeometry& entry,
                                   const FillStrokeInfo& fs,
                                   const std::vector<LayerFilter*>& filters,
                                   const std::vector<LayerStyle*>& styles) {
  FillStrokeInfo localFs = fs;
  if (localFs.textBox == nullptr) {
    localFs.textBox = entry.textBox;
  }
  switch (entry.element->nodeType()) {
    case NodeType::Rectangle:
      writeRectangle(out, static_cast<const Rectangle*>(entry.element), localFs, entry.transform,
                     entry.alpha, filters, styles);
      break;
    case NodeType::Ellipse:
      writeEllipse(out, static_cast<const Ellipse*>(entry.element), localFs, entry.transform,
                   entry.alpha, filters, styles);
      break;
    case NodeType::Path:
      writePath(out, static_cast<const Path*>(entry.element), localFs, entry.transform, entry.alpha,
                filters, styles);
      break;
    case NodeType::Text: {
      auto* text = static_cast<const Text*>(entry.element);
      // GlyphRun-only Text (no readable text content) carries pre-shaped glyph
      // outlines from a custom font; PowerPoint's native a:r runs can't express
      // arbitrary glyph IDs + per-glyph transforms, so the only way to render
      // these is via path geometry — regardless of the convertTextToPath flag.
      bool glyphRunOnly = text->text.empty() && !text->glyphRuns.empty();
      if ((_convertTextToPath || glyphRunOnly) && !text->glyphRuns.empty()) {
        writeTextAsPath(out, text, localFs, entry.transform, entry.alpha, filters, styles);
      } else {
        writeNativeText(out, text, localFs, entry.transform, entry.alpha, filters, styles);
      }
      break;
    }
    default:
      break;
  }
}

// Walk a single scope's element list, growing `accumulator` with new geometry
// and emitting a copy of every visible geometry whenever a Painter is hit.
// `scopeStart` is the index where the current Group's scope begins — Painters
// inside this scope can only render entries from [scopeStart, end), enforcing
// the "Painters within the group only render geometry accumulated within the
// group" rule from the spec while allowing geometry to propagate upward when
// the scope unwinds.
void PPTWriter::processVectorScope(XMLBuilder& out, const std::vector<Element*>& elements,
                                   const Matrix& transform, float alpha,
                                   const std::vector<LayerFilter*>& filters,
                                   const std::vector<LayerStyle*>& styles,
                                   const TextBox* parentTextBox,
                                   std::vector<AccumulatedGeometry>& accumulator,
                                   size_t scopeStart) {
  // The TextBox modifier-association rule is "first <TextBox> in this element
  // list applies to all sibling Text geometry"; we pre-scan once to mirror the
  // legacy CollectFillStroke().textBox behaviour, then fall back to the
  // parent's TextBox so Text inside a Group still inherits an outer one.
  const TextBox* localTextBox = FindModifierTextBox(elements);
  if (localTextBox == nullptr) {
    localTextBox = parentTextBox;
  }

  for (auto* element : elements) {
    auto type = element->nodeType();
    switch (type) {
      case NodeType::Fill:
      case NodeType::Stroke: {
        FillStrokeInfo painterFs = {};
        if (type == NodeType::Fill) {
          painterFs.fill = static_cast<const Fill*>(element);
        } else {
          painterFs.stroke = static_cast<const Stroke*>(element);
        }
        for (size_t i = scopeStart; i < accumulator.size(); ++i) {
          emitGeometryWithFs(out, accumulator[i], painterFs, filters, styles);
        }
        break;
      }
      case NodeType::Rectangle:
      case NodeType::Ellipse:
      case NodeType::Path:
      case NodeType::Text: {
        AccumulatedGeometry entry;
        entry.element = element;
        entry.transform = transform;
        entry.alpha = alpha;
        entry.textBox = localTextBox;
        accumulator.push_back(entry);
        break;
      }
      case NodeType::TextBox: {
        auto* tb = static_cast<const TextBox*>(element);
        if (tb->elements.empty()) {
          // Modifier-only TextBox — already captured by FindModifierTextBox above.
          break;
        }
        Matrix tbMatrix = transform * BuildGroupMatrix(tb);
        float tbAlpha = alpha * tb->alpha;
        if (_convertTextToPath) {
          // Container TextBox under glyph-path mode: process as its own scope so
          // child Text is added to the accumulator with the box's transform/alpha
          // and box-level params surface as their textBox. Geometry still
          // propagates upward like Group, and an outer Painter can render it.
          // Path-modifier resolution is scoped per Group/TextBox so that a
          // TrimPath / RoundCorner / MergePath / Repeater nested inside the
          // box only sees its sibling shapes (matches the renderer behaviour).
          const std::vector<Element*>& innerWalked =
              _resolveModifiers ? _resolver.resolve(tb->elements) : tb->elements;
          processVectorScope(out, innerWalked, tbMatrix, tbAlpha, filters, styles, tb, accumulator,
                             accumulator.size());
        } else {
          // Native PowerPoint text rendering still goes through the dedicated
          // multi-run text-box writer: PPTX represents multi-style text with
          // its own a:p/a:r runs and we don't accumulate Text geometry into
          // the surrounding scope in that mode.
          writeTextBoxGroup(out, tb, tb->elements, tbMatrix, tbAlpha, filters, styles);
        }
        break;
      }
      case NodeType::Group: {
        auto* group = static_cast<const Group*>(element);
        Matrix groupMatrix = transform * BuildGroupMatrix(group);
        float groupAlpha = alpha * group->alpha;
        // New scope start: Painters inside the Group can only see geometry
        // added from this point forward. After the recursive call returns the
        // accumulator still contains the Group's geometry, so outer Painters
        // can render it (geometry propagates upward across Group boundaries).
        // Re-run the path-modifier resolver on the inner element list so that
        // TrimPath / RoundCorner / MergePath / Repeater nested inside this
        // Group are baked into editable geometry. Group is a scope boundary —
        // these modifiers only operate on sibling shapes inside the same
        // Group, matching the tgfx renderer's per-Group VectorContext.
        const std::vector<Element*>& innerWalked =
            _resolveModifiers ? _resolver.resolve(group->elements) : group->elements;
        processVectorScope(out, innerWalked, groupMatrix, groupAlpha, filters, styles,
                           localTextBox, accumulator, accumulator.size());
        break;
      }
      case NodeType::Repeater:
        // Reaching this case means the resolver was disabled or the input
        // contained a Repeater inside an empty scope (output cleared by a
        // copies==0 case, etc.); silently skip so the remaining content still
        // renders (matches the behaviour for other unresolved modifiers like
        // TrimPath / RoundCorner / MergePath).
        break;
      default:
        // TextPath, TextModifier, RangeSelector, Polystar (when the resolver
        // is disabled), and any unrecognized element type fall through
        // silently. The layer-level rasterization probe runs in writeLayer to
        // escalate cases where dropping these elements would change the
        // visual result.
        break;
    }
  }
}

void PPTWriter::writeElements(XMLBuilder& out, const std::vector<Element*>& elements,
                              const Matrix& transform, float alpha,
                              const std::vector<LayerFilter*>& filters,
                              const std::vector<LayerStyle*>& styles,
                              const TextBox* parentTextBox) {
  // Bake every path-modifier (Polystar -> Path, Repeater -> grouped copies,
  // TrimPath / RoundCorner / MergePath -> editable Path via tgfx). Painters
  // (Fill / Stroke), Group, and text-related elements pass through unchanged
  // so the accumulator walk below behaves exactly as in the unresolved case.
  const std::vector<Element*>& walked = _resolveModifiers ? _resolver.resolve(elements) : elements;

  std::vector<AccumulatedGeometry> accumulator;
  accumulator.reserve(walked.size());
  processVectorScope(out, walked, transform, alpha, filters, styles, parentTextBox, accumulator,
                     /*scopeStart=*/0);
}

// Rasterize the entire layer (including its sub-tree) to a single embedded PNG
// and emit it as a positioned p:pic. Returns true if a picture was emitted.
bool PPTWriter::rasterizeLayerAsPicture(XMLBuilder& out, const Layer* layer) {
  auto& buildResult = ensureBuildResult();
  auto it = buildResult.layerMap.find(layer);
  if (it == buildResult.layerMap.end() || !buildResult.root) {
    return false;
  }
  auto tgfxLayer = it->second;
  if (!tgfxLayer) {
    return false;
  }
  auto pngData = RenderMaskedLayer(&_gpu, buildResult.root, tgfxLayer);
  if (!pngData) {
    return false;
  }
  auto bounds = tgfxLayer->getBounds(buildResult.root.get(), true);
  auto offX = PxToEMU(bounds.left);
  auto offY = PxToEMU(bounds.top);
  auto extCX = std::max(int64_t(1), PxToEMU(bounds.width()));
  auto extCY = std::max(int64_t(1), PxToEMU(bounds.height()));
  auto relId = _ctx->addRawImage(std::move(pngData));
  writePicture(out, relId, offX, offY, extCX, extCY);
  return true;
}

void PPTWriter::writeLayer(XMLBuilder& out, const Layer* layer, const Matrix& parentMatrix,
                           float parentAlpha) {
  if (!layer->visible && layer->mask == nullptr) {
    return;
  }

  Matrix layerMatrix = parentMatrix * BuildLayerMatrix(layer);
  float layerAlpha = parentAlpha * layer->alpha;

  if (layer->mask != nullptr && _bakeMask) {
    if (rasterizeLayerAsPicture(out, layer)) {
      return;
    }
    // Bake fell through (zero bounds, no GPU, etc.) - fall through to writing
    // the layer as a regular layer so its content is at least visible without
    // the mask effect.
  }

  // OOXML has no native clipping primitive for arbitrary shape children, so a
  // layer that carries a scrollRect (set explicitly or generated by clipToBounds
  // during layout) can only be honoured by rasterizing the whole layer subtree
  // through the renderer, which already applies setScrollRect via tgfx. When
  // baking is disabled the clip is silently dropped and child shapes are emitted
  // unclipped (matches the behaviour of unsupported features elsewhere).
  if (layer->hasScrollRect && _bakeScrollRect) {
    if (rasterizeLayerAsPicture(out, layer)) {
      return;
    }
    // Bake fell through (zero bounds, no GPU, etc.) - fall through and emit the
    // layer's content unclipped so it remains at least partially visible.
  }

  // Probe the layer for features that OOXML cannot represent natively. When
  // anything trips the probe AND the corresponding rasterize-* option is on,
  // bake the whole layer to a PNG so the visual result is preserved. The
  // alternative (silently dropping unsupported elements) was the V1 behaviour
  // and produced obviously wrong slides for documents with TextPath, complex
  // blends, ColorMatrix filters, or wide-gamut colors.
  auto features = ProbeLayerFeatures(layer);
  if (features.needsRasterization(_rasterizeUnsupportedBlend, _rasterizeWideGamut)) {
    if (rasterizeLayerAsPicture(out, layer)) {
      return;
    }
  }

  writeElements(out, layer->contents, layerMatrix, layerAlpha, layer->filters, layer->styles);

  if (layer->composition != nullptr) {
    for (const auto* compLayer : layer->composition->layers) {
      writeLayer(out, compLayer, layerMatrix, layerAlpha);
    }
  }

  for (const auto* child : layer->children) {
    writeLayer(out, child, layerMatrix, layerAlpha);
  }
}

//==============================================================================
// ZIP helpers
//==============================================================================

static bool AddZipEntry(zipFile zf, const char* name, const void* data, unsigned size) {
  zip_fileinfo zi = {};
  if (zipOpenNewFileInZip(zf, name, &zi, nullptr, 0, nullptr, 0, nullptr, Z_DEFLATED,
                          Z_DEFAULT_COMPRESSION) != ZIP_OK) {
    return false;
  }
  if (zipWriteInFileInZip(zf, data, size) != ZIP_OK) {
    zipCloseFileInZip(zf);
    return false;
  }
  zipCloseFileInZip(zf);
  return true;
}

static bool AddZipString(zipFile zf, const char* name, const std::string& content) {
  return AddZipEntry(zf, name, content.data(), static_cast<unsigned>(content.size()));
}

//==============================================================================
// PPTExporter::ToFile
//==============================================================================

bool PPTExporter::ToFile(PAGXDocument& doc, const std::string& filePath, const Options& options) {
  if (!doc.isLayoutApplied()) {
    doc.applyLayout();
  }

  auto layoutContext = std::make_unique<LayoutContext>(options.fontConfig);

  PPTWriterContext context;
  PPTWriter writer(&context, &doc, options, layoutContext.get());

  // Build slide body content
  XMLBuilder body(false, 2, 0, 16384);
  for (const auto* layer : doc.layers) {
    writer.writeLayer(body, layer);
  }
  std::string bodyContent = body.release();

  // Assemble slide XML
  std::string slide;
  slide.reserve(2048 + bodyContent.size());
  slide += "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>";
  slide +=
      "<p:sld xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
      "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" "
      "xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\">";
  slide += "<p:cSld><p:spTree>";
  slide += "<p:nvGrpSpPr><p:cNvPr id=\"1\" name=\"\"/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr>";
  slide +=
      "<p:grpSpPr><a:xfrm><a:off x=\"0\" y=\"0\"/><a:ext cx=\"0\" cy=\"0\"/>"
      "<a:chOff x=\"0\" y=\"0\"/><a:chExt cx=\"0\" cy=\"0\"/></a:xfrm></p:grpSpPr>";
  slide += bodyContent;
  slide += "</p:spTree></p:cSld>";
  slide += "<p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr>";
  slide += "</p:sld>";

  // Write ZIP via minizip
  zipFile zf = zipOpen(filePath.c_str(), APPEND_STATUS_CREATE);
  if (!zf) {
    return false;
  }

  bool ok = true;
  ok = ok && AddZipString(zf, "[Content_Types].xml", GenerateContentTypes(context));
  ok = ok && AddZipString(zf, "_rels/.rels", GenerateRootRels());
  ok = ok && AddZipString(zf, "ppt/presentation.xml", GeneratePresentation(doc.width, doc.height));
  ok = ok && AddZipString(zf, "ppt/_rels/presentation.xml.rels", GeneratePresentationRels());
  ok = ok && AddZipString(zf, "ppt/slides/slide1.xml", slide);
  ok = ok && AddZipString(zf, "ppt/slides/_rels/slide1.xml.rels", GenerateSlideRels(context));
  ok = ok && AddZipString(zf, "ppt/slideMasters/slideMaster1.xml", GenerateSlideMaster());
  ok = ok &&
       AddZipString(zf, "ppt/slideMasters/_rels/slideMaster1.xml.rels", GenerateSlideMasterRels());
  ok = ok && AddZipString(zf, "ppt/slideLayouts/slideLayout1.xml", GenerateSlideLayout());
  ok = ok &&
       AddZipString(zf, "ppt/slideLayouts/_rels/slideLayout1.xml.rels", GenerateSlideLayoutRels());
  ok = ok && AddZipString(zf, "ppt/theme/theme1.xml", GenerateTheme());
  ok = ok && AddZipString(zf, "ppt/presProps.xml", GeneratePresProps());
  ok = ok && AddZipString(zf, "ppt/viewProps.xml", GenerateViewProps());
  ok = ok && AddZipString(zf, "ppt/tableStyles.xml", GenerateTableStyles());
  ok = ok && AddZipString(zf, "docProps/core.xml", GenerateCoreProps());
  ok = ok && AddZipString(zf, "docProps/app.xml", GenerateAppProps());

  for (const auto& img : context.images()) {
    if (!ok) {
      break;
    }
    if (img.cachedData && img.cachedData->size() > 0) {
      ok = AddZipEntry(zf, img.mediaPath.c_str(), img.cachedData->bytes(),
                       static_cast<unsigned>(img.cachedData->size()));
    }
  }

  if (!ok) {
    zipClose(zf, nullptr);
    return false;
  }
  return zipClose(zf, nullptr) == ZIP_OK;
}

}  // namespace pagx
