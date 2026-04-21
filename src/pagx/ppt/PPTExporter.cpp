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

  void writeElements(XMLBuilder& out, const std::vector<Element*>& elements,
                     const Matrix& transform, float alpha,
                     const std::vector<LayerFilter*>& filters,
                     const std::vector<LayerStyle*>& styles,
                     const TextBox* parentTextBox = nullptr);

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
                      const std::vector<LayerStyle*>& styles);
  void writeParagraphRun(XMLBuilder& out, const std::string& runText, const PPTRunStyle& style,
                         const std::vector<LayerFilter*>& filters,
                         const std::vector<LayerStyle*>& styles);
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

  // Custom geometry from PathData
  void writeCustomGeom(XMLBuilder& out, const PathData* data, float ofsX, float ofsY, float boundsW,
                       float boundsH, FillRule fillRule = FillRule::Winding,
                       float coordScaleX = 1.0f, float coordScaleY = 1.0f);

  // Shared contour-to-custGeom emitter used by writeCustomGeom and writeTextAsPath
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
  if (pattern->tileModeX == TileMode::Repeat || pattern->tileModeX == TileMode::Mirror ||
      pattern->tileModeY == TileMode::Repeat || pattern->tileModeY == TileMode::Mirror) {
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
    auto& src = out.openElement("a:srcRect");
    if (ipr.srcL != 0) {
      src.addRequiredAttribute("l", ipr.srcL);
    }
    if (ipr.srcT != 0) {
      src.addRequiredAttribute("t", ipr.srcT);
    }
    if (ipr.srcR != 0) {
      src.addRequiredAttribute("r", ipr.srcR);
    }
    if (ipr.srcB != 0) {
      src.addRequiredAttribute("b", ipr.srcB);
    }
    src.closeElementSelfClosing();
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
    out.openElement("a:srgbClr").addRequiredAttribute("val", ColorToHex6(stop->color));
    if (stop->color.alpha < 1.0f) {
      out.closeElementStart();
      out.openElement("a:alpha")
          .addRequiredAttribute("val", AlphaToPct(stop->color.alpha))
          .closeElementSelfClosing();
      out.closeElement();  // a:srgbClr
    } else {
      out.closeElementSelfClosing();
    }
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
      auto* grad = static_cast<const RadialGradient*>(source);
      out.openElement("a:gradFill").closeElementStart();
      writeGradientStops(out, grad->colorStops);
      out.openElement("a:path").addRequiredAttribute("path", "circle").closeElementStart();
      // Map the gradient center through its matrix to document space and convert to
      // fillToRect insets (1/1000 percent) relative to the shape bounds.  OOXML
      // radial gradients always span the whole bounding box, so radius cannot be
      // honoured exactly; only the focus position is preserved.  When shapeBounds
      // is empty (e.g. text fills) fall back to the shape center.
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
      out.openElement("a:gradFill")
          .addRequiredAttribute("rotWithShape", "1")
          .closeElementStart();
      writeGradientStops(out, grad->colorStops);
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
      out.openElement("a:path").addRequiredAttribute("path", "circle").closeElementStart();
      out.openElement("a:fillToRect")
          .addRequiredAttribute("l", l)
          .addRequiredAttribute("t", t)
          .addRequiredAttribute("r", r)
          .addRequiredAttribute("b", b)
          .closeElementSelfClosing();
      out.closeElement();  // a:path
      out.closeElement();  // a:gradFill
      break;
    }
    case NodeType::DiamondGradient: {
      // OOXML has no diamond gradient primitive. The closest preset is a
      // rectangular path gradient, which produces an axis-aligned diamond-like
      // pattern when the focus rect is collapsed to the center point.
      auto* grad = static_cast<const DiamondGradient*>(source);
      out.openElement("a:gradFill")
          .addRequiredAttribute("rotWithShape", "1")
          .closeElementStart();
      writeGradientStops(out, grad->colorStops);
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
      out.openElement("a:path").addRequiredAttribute("path", "rect").closeElementStart();
      out.openElement("a:fillToRect")
          .addRequiredAttribute("l", l)
          .addRequiredAttribute("t", t)
          .addRequiredAttribute("r", r)
          .addRequiredAttribute("b", b)
          .closeElementSelfClosing();
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

  bool needsTiling =
      (pattern->tileModeX == TileMode::Repeat || pattern->tileModeX == TileMode::Mirror ||
       pattern->tileModeY == TileMode::Repeat || pattern->tileModeY == TileMode::Mirror);

  if (needsTiling && _bakeTiledPattern && !shapeBounds.isEmpty()) {
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

  if (needsTiling && hasDimensions && !shapeBounds.isEmpty()) {
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
      auto& src = out.openElement("a:srcRect");
      if (ipr.srcL != 0) {
        src.addRequiredAttribute("l", ipr.srcL);
      }
      if (ipr.srcT != 0) {
        src.addRequiredAttribute("t", ipr.srcT);
      }
      if (ipr.srcR != 0) {
        src.addRequiredAttribute("r", ipr.srcR);
      }
      if (ipr.srcB != 0) {
        src.addRequiredAttribute("b", ipr.srcB);
      }
      src.closeElementSelfClosing();
      int fillL =
          static_cast<int>(std::round((ipr.visL - shapeBounds.x) / shapeBounds.width * 100000.0f));
      int fillT =
          static_cast<int>(std::round((ipr.visT - shapeBounds.y) / shapeBounds.height * 100000.0f));
      int fillR = static_cast<int>(std::round((shapeBounds.x + shapeBounds.width - ipr.visR) /
                                              shapeBounds.width * 100000.0f));
      int fillB = static_cast<int>(std::round((shapeBounds.y + shapeBounds.height - ipr.visB) /
                                              shapeBounds.height * 100000.0f));
      out.openElement("a:stretch").closeElementStart();
      auto& fr = out.openElement("a:fillRect");
      if (fillL != 0) {
        fr.addRequiredAttribute("l", fillL);
      }
      if (fillT != 0) {
        fr.addRequiredAttribute("t", fillT);
      }
      if (fillR != 0) {
        fr.addRequiredAttribute("r", fillR);
      }
      if (fillB != 0) {
        fr.addRequiredAttribute("b", fillB);
      }
      fr.closeElementSelfClosing();
      out.closeElement();  // a:stretch
    } else {
      WriteDefaultStretch(out);
    }
  }

  out.closeElement();  // a:blipFill
}

// ── Fill / stroke ──────────────────────────────────────────────────────────

void PPTWriter::writeSolidColorFill(XMLBuilder& out, const Color& color, float alpha) {
  float effectiveAlpha = color.alpha * alpha;
  out.openElement("a:solidFill").closeElementStart();
  out.openElement("a:srgbClr").addRequiredAttribute("val", ColorToHex6(color));
  if (effectiveAlpha < 1.0f) {
    out.closeElementStart();
    out.openElement("a:alpha")
        .addRequiredAttribute("val", AlphaToPct(effectiveAlpha))
        .closeElementSelfClosing();
    out.closeElement();
  } else {
    out.closeElementSelfClosing();
  }
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
        out.openElement("a:srgbClr")
            .addRequiredAttribute("val", ColorToHex6(blend->color));
        if (blend->color.alpha < 1.0f) {
          out.closeElementStart();
          out.openElement("a:alpha")
              .addRequiredAttribute("val", AlphaToPct(blend->color.alpha))
              .closeElementSelfClosing();
          out.closeElement();  // a:srgbClr
        } else {
          out.closeElementSelfClosing();
        }
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
  EmitCustGeomHeader(out);
  out.openElement("a:pathLst").closeElementStart();

  if (_bridgeContours && contours.size() > 1) {
    auto depths = ComputeContainmentDepths(contours);
    if (fillRule == FillRule::EvenOdd) {
      AdjustWindingForEvenOdd(contours, depths);
    }
    auto groups = GroupContoursByOutermost(contours, depths);
    for (const auto& group : groups) {
      out.openElement("a:path")
          .addRequiredAttribute("w", pathWidth)
          .addRequiredAttribute("h", pathHeight)
          .closeElementStart();
      if (group.size() > 1) {
        EmitBridgedGroup(out, contours, group, scaleX, scaleY, scaledOfsX, scaledOfsY);
      } else {
        EmitContour(out, contours[group[0]], scaleX, scaleY, scaledOfsX, scaledOfsY);
      }
      out.closeElement();  // a:path
    }
  } else {
    out.openElement("a:path")
        .addRequiredAttribute("w", pathWidth)
        .addRequiredAttribute("h", pathHeight)
        .closeElementStart();
    for (auto& c : contours) {
      EmitContour(out, c, scaleX, scaleY, scaledOfsX, scaledOfsY);
    }
    out.closeElement();  // a:path
  }

  out.closeElement();  // a:pathLst
  out.closeElement();  // a:custGeom
}

void PPTWriter::writeCustomGeom(XMLBuilder& out, const PathData* data, float ofsX, float ofsY,
                                float boundsW, float boundsH, FillRule fillRule, float coordScaleX,
                                float coordScaleY) {
  int64_t pw = std::max(int64_t(1), PxToEMU(boundsW * coordScaleX));
  int64_t ph = std::max(int64_t(1), PxToEMU(boundsH * coordScaleY));
  auto contours = ParsePathContours(data);
  WriteContourGeom(out, contours, pw, ph, coordScaleX, coordScaleY, ofsX * coordScaleX,
                   ofsY * coordScaleY, fillRule);
}

// ── Shape writers ──────────────────────────────────────────────────────────

void PPTWriter::writeRectangle(XMLBuilder& out, const Rectangle* rect, const FillStrokeInfo& fs,
                               const Matrix& m, float alpha,
                               const std::vector<LayerFilter*>& filters,
                               const std::vector<LayerStyle*>& styles) {
  float x = rect->position.x - rect->size.width / 2.0f;
  float y = rect->position.y - rect->size.height / 2.0f;
  Rect shapeBounds = Rect::MakeXYWH(x, y, rect->size.width, rect->size.height);

  bool imageWritten = writeImagePatternAsPicture(out, fs.fill, shapeBounds, m, alpha);
  if (imageWritten && !fs.stroke && filters.empty() && styles.empty()) {
    return;
  }

  auto xf = decomposeXform(x, y, rect->size.width, rect->size.height, m);
  beginShape(out, "Rectangle", xf.offX, xf.offY, xf.extCX, xf.extCY, xf.rotation);

  if (rect->roundness > 0) {
    float minSide = std::min(rect->size.width, rect->size.height);
    int adj = (minSide > 0)
                  ? std::clamp(static_cast<int>(rect->roundness * 100000.0f / minSide), 0, 50000)
                  : 0;
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
                             const Matrix& m, float alpha,
                             const std::vector<LayerFilter*>& filters,
                             const std::vector<LayerStyle*>& styles) {
  float rx = ellipse->size.width / 2.0f;
  float ry = ellipse->size.height / 2.0f;
  float x = ellipse->position.x - rx;
  float y = ellipse->position.y - ry;
  Rect shapeBounds = Rect::MakeXYWH(x, y, ellipse->size.width, ellipse->size.height);

  bool imageWritten = writeImagePatternAsPicture(out, fs.fill, shapeBounds, m, alpha);
  if (imageWritten && !fs.stroke && filters.empty() && styles.empty()) {
    return;
  }

  auto xf = decomposeXform(x, y, ellipse->size.width, ellipse->size.height, m);
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

  if (_bridgeContours && contours.size() > 1) {
    auto groups = PrepareContourGroups(contours, fillRule);
    if (groups.size() > 1) {
      int64_t pw = std::max(int64_t(1), PxToEMU(adjustedW));
      int64_t ph = std::max(int64_t(1), PxToEMU(adjustedH));
      for (const auto& group : groups) {
        beginShape(out, "Path", xf.offX, xf.offY, xf.extCX, xf.extCY, xf.rotation);
        EmitGroupCustGeom(out, contours, group, pw, ph, 1.0f, 1.0f, adjustedX, adjustedY);
        writeShapeTail(out, fs, alpha, shapeBounds, imageWritten, filters, styles);
      }
      return;
    }
  }

  beginShape(out, "Path", xf.offX, xf.offY, xf.extCX, xf.extCY, xf.rotation);
  int64_t pw = std::max(int64_t(1), PxToEMU(adjustedW));
  int64_t ph = std::max(int64_t(1), PxToEMU(adjustedH));
  WriteContourGeom(out, contours, pw, ph, 1.0f, 1.0f, adjustedX, adjustedY, fillRule);
  writeShapeTail(out, fs, alpha, shapeBounds, imageWritten, filters, styles);
}

void PPTWriter::writeTextAsPath(XMLBuilder& out, const Text* text, const FillStrokeInfo& fs,
                                const Matrix& m, float alpha,
                                const std::vector<LayerFilter*>& /*filters*/,
                                const std::vector<LayerStyle*>& /*styles*/) {
  auto glyphPaths = ComputeGlyphPaths(*text, text->position.x, text->position.y);
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
    estHeight =
        (!std::isnan(boxHeight) && boxHeight > 0) ? boxHeight : text->fontSize * 1.4f;
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

  out.openElement("p:txBody").closeElementStart();
  auto& bodyPr = out.openElement("a:bodyPr")
                     .addRequiredAttribute("wrap", hasTextBox ? "square" : "none")
                     .addRequiredAttribute("lIns", "0")
                     .addRequiredAttribute("tIns", "0")
                     .addRequiredAttribute("rIns", "0")
                     .addRequiredAttribute("bIns", "0");
  if (fs.textBox) {
    // CJK vertical writing: characters stay upright, columns stack right-to-left.
    // "eaVert" matches the WritingMode::Vertical contract (East Asian vertical
    // text). Latin "vert"/"vert270" rotate glyphs sideways which is wrong here.
    if (fs.textBox->writingMode == WritingMode::Vertical) {
      bodyPr.addRequiredAttribute("vert", "eaVert");
    }
    // OOXML's "anchor" describes alignment along the block-flow axis, which
    // matches paragraphAlign in both writing modes:
    //   - Horizontal: block axis is top→bottom (Near=top, Far=bottom).
    //   - Vertical (eaVert): block axis is right→left (Near=right column,
    //     Far=left column). PowerPoint maps t/ctr/b to start/center/end of
    //     that axis, so the same enum→string mapping applies.
    if (fs.textBox->paragraphAlign == ParagraphAlign::Middle) {
      bodyPr.addRequiredAttribute("anchor", "ctr");
    } else if (fs.textBox->paragraphAlign == ParagraphAlign::Far) {
      bodyPr.addRequiredAttribute("anchor", "b");
    }
  }
  bodyPr.closeElementSelfClosing();
  out.openElement("a:lstStyle").closeElementSelfClosing();

  PPTRunStyle style = {};
  style.algn = nullptr;
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
  style.hasBold =
      text->fauxBold || text->fontStyle.find("Bold") != std::string::npos;
  style.hasItalic =
      text->fauxItalic || text->fontStyle.find("Italic") != std::string::npos;
  style.fontSize = FontSizeToPPT(text->fontSize);
  style.letterSpc = static_cast<int64_t>(std::round(text->letterSpacing * 75.0));
  style.hasLetterSpacing = text->letterSpacing != 0.0f;
  // a:rPr supports a:solidFill and a:gradFill but not a:blipFill, so ImagePattern
  // fills are skipped (text falls back to the renderer default).
  style.hasFillColor = false;
  if (fs.fill && fs.fill->color) {
    auto type = fs.fill->color->nodeType();
    style.hasFillColor =
        (type == NodeType::SolidColor || type == NodeType::LinearGradient ||
         type == NodeType::RadialGradient || type == NodeType::ConicGradient ||
         type == NodeType::DiamondGradient);
  }
  style.fillAlpha = style.hasFillColor ? fs.fill->alpha * alpha : 0;
  style.fillColor = style.hasFillColor ? fs.fill->color : nullptr;
  style.typeface = text->fontFamily.empty() ? std::string() : StripQuotes(text->fontFamily);

  if (lines && !lines->empty()) {
    for (const auto& lineInfo : *lines) {
      if (lineInfo.byteStart >= lineInfo.byteEnd) {
        continue;
      }
      std::string line =
          text->text.substr(lineInfo.byteStart, lineInfo.byteEnd - lineInfo.byteStart);
      if (line.empty()) {
        continue;
      }
      writeParagraph(out, line, style, filters, styles);
    }
  } else {
    const std::string& remaining = text->text;
    size_t pos = 0;
    while (pos <= remaining.size()) {
      size_t nl = remaining.find('\n', pos);
      std::string line =
          (nl == std::string::npos) ? remaining.substr(pos) : remaining.substr(pos, nl - pos);
      writeParagraph(out, line, style, filters, styles);
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

void PPTWriter::writeParagraph(XMLBuilder& out, const std::string& lineText,
                               const PPTRunStyle& style,
                               const std::vector<LayerFilter*>& filters,
                               const std::vector<LayerStyle*>& styles) {
  out.openElement("a:p").closeElementStart();
  if (style.algn) {
    out.openElement("a:pPr").addRequiredAttribute("algn", style.algn).closeElementSelfClosing();
  }
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

  // Run a layout pass purely to obtain the textbox's resolved bounds; we let
  // PowerPoint perform its own line wrapping inside the shape so that mixed
  // run-styles (font/size/color) don't have to be threaded through PAGX's
  // line-break offsets.
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
  float estHeight = (!std::isnan(boxHeight) && boxHeight > 0) ? boxHeight
                                                              : layoutResult.bounds.height;
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

  out.openElement("p:txBody").closeElementStart();
  auto& bodyPr = out.openElement("a:bodyPr")
                     .addRequiredAttribute("wrap", hasBoxWidth ? "square" : "none")
                     .addRequiredAttribute("lIns", "0")
                     .addRequiredAttribute("tIns", "0")
                     .addRequiredAttribute("rIns", "0")
                     .addRequiredAttribute("bIns", "0");
  if (box->writingMode == WritingMode::Vertical) {
    bodyPr.addRequiredAttribute("vert", "eaVert");
  }
  if (box->paragraphAlign == ParagraphAlign::Middle) {
    bodyPr.addRequiredAttribute("anchor", "ctr");
  } else if (box->paragraphAlign == ParagraphAlign::Far) {
    bodyPr.addRequiredAttribute("anchor", "b");
  }
  bodyPr.closeElementSelfClosing();
  out.openElement("a:lstStyle").closeElementSelfClosing();

  const char* algn = nullptr;
  if (box->textAlign == TextAlign::Center) {
    algn = "ctr";
  } else if (box->textAlign == TextAlign::End) {
    algn = "r";
  } else if (box->textAlign == TextAlign::Justify) {
    algn = "just";
  }

  // Build per-run styles up-front (font/size/bold/italic/color/typeface).
  struct ResolvedRunStyle {
    PPTRunStyle style = {};
  };
  std::vector<ResolvedRunStyle> runStyles;
  runStyles.reserve(runs.size());
  for (const auto& run : runs) {
    ResolvedRunStyle rs;
    rs.style.algn = nullptr;  // alignment lives on a:pPr, not a:rPr
    rs.style.hasBold =
        run.text->fauxBold || run.text->fontStyle.find("Bold") != std::string::npos;
    rs.style.hasItalic =
        run.text->fauxItalic || run.text->fontStyle.find("Italic") != std::string::npos;
    rs.style.fontSize = FontSizeToPPT(run.text->fontSize);
    rs.style.letterSpc = static_cast<int64_t>(std::round(run.text->letterSpacing * 75.0));
    rs.style.hasLetterSpacing = run.text->letterSpacing != 0.0f;
    rs.style.hasFillColor = false;
    if (run.fill && run.fill->color) {
      auto type = run.fill->color->nodeType();
      // a:rPr supports a:solidFill and a:gradFill but not a:blipFill, so ImagePattern
      // fills are skipped (the run falls back to the renderer default).
      rs.style.hasFillColor =
          (type == NodeType::SolidColor || type == NodeType::LinearGradient ||
           type == NodeType::RadialGradient || type == NodeType::ConicGradient ||
           type == NodeType::DiamondGradient);
    }
    rs.style.fillAlpha = rs.style.hasFillColor ? run.fill->alpha * alpha : 0;
    rs.style.fillColor = rs.style.hasFillColor ? run.fill->color : nullptr;
    rs.style.typeface =
        run.text->fontFamily.empty() ? std::string() : StripQuotes(run.text->fontFamily);
    runStyles.push_back(std::move(rs));
  }

  // Stream runs into paragraphs, splitting on '\n'. A single Text element may
  // carry multiple newlines internally, in which case it contributes to several
  // consecutive paragraphs.
  bool paragraphOpen = false;
  auto openParagraph = [&]() {
    out.openElement("a:p").closeElementStart();
    if (algn) {
      out.openElement("a:pPr").addRequiredAttribute("algn", algn).closeElementSelfClosing();
    }
    paragraphOpen = true;
  };
  auto closeParagraph = [&]() {
    if (paragraphOpen) {
      out.closeElement();  // a:p
      paragraphOpen = false;
    }
  };
  auto emitRun = [&](const std::string& fragment, const ResolvedRunStyle& rs) {
    if (fragment.empty()) {
      return;
    }
    if (!paragraphOpen) {
      openParagraph();
    }
    writeParagraphRun(out, fragment, rs.style, filters, styles);
  };

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

  out.closeElement();  // p:txBody
  out.closeElement();  // p:sp
}

// ── Element / layer traversal ──────────────────────────────────────────────

void PPTWriter::writeElements(XMLBuilder& out, const std::vector<Element*>& elements,
                              const Matrix& transform, float alpha,
                              const std::vector<LayerFilter*>& filters,
                              const std::vector<LayerStyle*>& styles,
                              const TextBox* parentTextBox) {
  // Bake every path-modifier (Polystar -> Path, Repeater -> grouped copies,
  // TrimPath / RoundCorner / MergePath -> editable Path via tgfx). Painters
  // (Fill / Stroke) and text-related elements pass through unchanged so
  // CollectFillStroke and the per-element switch below behave exactly as in
  // the unresolved case.
  const std::vector<Element*>& walked =
      _resolveModifiers ? _resolver.resolve(elements) : elements;

  auto fs = CollectFillStroke(walked);
  if (parentTextBox != nullptr && fs.textBox == nullptr) {
    fs.textBox = parentTextBox;
  }

  for (size_t i = 0; i < walked.size(); ++i) {
    const auto* element = walked[i];
    auto type = element->nodeType();
    if (type == NodeType::Fill || type == NodeType::Stroke) {
      continue;
    }
    if (type == NodeType::TextBox) {
      auto* tb = static_cast<const TextBox*>(element);
      if (!tb->elements.empty()) {
        Matrix tbMatrix = BuildGroupMatrix(tb);
        Matrix combined = transform * tbMatrix;
        float tbAlpha = alpha * tb->alpha;
        if (_convertTextToPath) {
          // When text is converted to path geometry, recurse so each child Text is
          // rendered via writeTextAsPath; the TextBox metadata is propagated as
          // parentTextBox so child glyph runs still pick up box-level alignment.
          writeElements(out, tb->elements, combined, tbAlpha, filters, styles, tb);
        } else {
          writeTextBoxGroup(out, tb, tb->elements, combined, tbAlpha, filters, styles);
        }
      }
      continue;
    }
    switch (type) {
      case NodeType::Rectangle:
        writeRectangle(out, static_cast<const Rectangle*>(element), fs, transform, alpha, filters,
                       styles);
        break;
      case NodeType::Ellipse:
        writeEllipse(out, static_cast<const Ellipse*>(element), fs, transform, alpha, filters,
                     styles);
        break;
      case NodeType::Path:
        writePath(out, static_cast<const Path*>(element), fs, transform, alpha, filters, styles);
        break;
      case NodeType::Text: {
        auto* text = static_cast<const Text*>(element);
        // GlyphRun-only Text (no readable text content) carries pre-shaped glyph
        // outlines from a custom font; PowerPoint's native a:r runs can't express
        // arbitrary glyph IDs + per-glyph transforms, so the only way to render
        // these is via path geometry — regardless of the convertTextToPath flag.
        bool glyphRunOnly = text->text.empty() && !text->glyphRuns.empty();
        if ((_convertTextToPath || glyphRunOnly) && !text->glyphRuns.empty()) {
          writeTextAsPath(out, text, fs, transform, alpha, filters, styles);
        } else {
          writeNativeText(out, text, fs, transform, alpha, filters, styles);
        }
        break;
      }
      case NodeType::Group: {
        auto* group = static_cast<const Group*>(element);
        Matrix groupMatrix = BuildGroupMatrix(group);
        Matrix combined = transform * groupMatrix;
        float groupAlpha = alpha * group->alpha;
        writeElements(out, group->elements, combined, groupAlpha, filters, styles, parentTextBox);
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
        // Fill, Stroke, TextPath, TextModifier, RangeSelector, Polystar (when
        // the resolver is disabled), and any unrecognized element type fall
        // through silently. The layer-level rasterization probe runs in
        // writeLayer to escalate cases where dropping these elements would
        // change the visual result.
        break;
    }
  }
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
