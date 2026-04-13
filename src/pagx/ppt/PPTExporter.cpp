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
#include <string>
#include <vector>
#include "base/utils/MathUtil.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/BlendFilter.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/ColorMatrixFilter.h"
#include "pagx/nodes/ColorStop.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/InnerShadowFilter.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/ppt/PPTBoilerplate.h"
#include "pagx/ppt/PPTContourUtils.h"
#include "pagx/ppt/PPTGeomEmitter.h"
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

class PPTWriter {
 public:
  PPTWriter(PPTWriterContext* ctx, PAGXDocument* doc, bool convertTextToPath, bool bakeMask,
            bool bridgeContours)
      : _ctx(ctx), _doc(doc), _convertTextToPath(convertTextToPath), _bakeMask(bakeMask),
        _bridgeContours(bridgeContours) {
  }

  void writeLayer(XMLBuilder& out, const Layer* layer, const Matrix& parentMatrix = {},
                  float parentAlpha = 1.0f);

 private:
  PPTWriterContext* _ctx = nullptr;
  PAGXDocument* _doc = nullptr;
  bool _convertTextToPath = true;
  bool _bakeMask = true;
  bool _bridgeContours = true;
  LayerBuildResult _buildResult = {};
  bool _buildResultReady = false;

  const LayerBuildResult& ensureBuildResult();

  void writeElements(XMLBuilder& out, const std::vector<Element*>& elements,
                     const Matrix& transform, float alpha,
                     const std::vector<LayerFilter*>& filters);

  void writeRectangle(XMLBuilder& out, const Rectangle* rect, const FillStrokeInfo& fs,
                      const Matrix& m, float alpha, const std::vector<LayerFilter*>& filters);
  void writeEllipse(XMLBuilder& out, const Ellipse* ellipse, const FillStrokeInfo& fs,
                    const Matrix& m, float alpha, const std::vector<LayerFilter*>& filters);
  void writePath(XMLBuilder& out, const Path* path, const FillStrokeInfo& fs, const Matrix& m,
                 float alpha, const std::vector<LayerFilter*>& filters);
  void writeTextAsPath(XMLBuilder& out, const Text* text, const FillStrokeInfo& fs, const Matrix& m,
                       float alpha, const std::vector<LayerFilter*>& filters);
  void writeNativeText(XMLBuilder& out, const Text* text, const FillStrokeInfo& fs, const Matrix& m,
                       float alpha, const std::vector<LayerFilter*>& filters);

  // Shape envelope helpers
  void beginShape(XMLBuilder& out, const char* name, int64_t offX, int64_t offY, int64_t extCX,
                  int64_t extCY, int rot = 0);
  void endShape(XMLBuilder& out);

  // Rasterized image as p:pic element
  void writePicture(XMLBuilder& out, const std::string& relId, int64_t offX, int64_t offY,
                    int64_t extCX, int64_t extCY);

  // Write non-tiling ImagePattern fill as a separate p:pic element.
  // Returns true if the image was written; caller should use a:noFill for the shape.
  bool writeImagePatternAsPicture(XMLBuilder& out, const Fill* fill, const Rect& shapeBounds,
                                  const Matrix& m, float alpha);

  // Fill / stroke / effects
  void writeFill(XMLBuilder& out, const Fill* fill, float alpha, const Rect& shapeBounds = {});
  void writeColorSource(XMLBuilder& out, const ColorSource* source, float alpha,
                        const Rect& shapeBounds = {});
  void writeGradientStops(XMLBuilder& out, const std::vector<ColorStop*>& stops);
  void writeStroke(XMLBuilder& out, const Stroke* stroke, float alpha);
  void writeEffects(XMLBuilder& out, const std::vector<LayerFilter*>& filters);
  void writeShadowElement(XMLBuilder& out, const char* tag, float blurX, float blurY, float offsetX,
                          float offsetY, const Color& color, bool includeAlign);

  static void WriteBlip(XMLBuilder& out, const std::string& relId, float alpha);
  static void WriteDefaultStretch(XMLBuilder& out);

  void writeShapeTail(XMLBuilder& out, const FillStrokeInfo& fs, float alpha,
                      const Rect& shapeBounds, bool imageWritten,
                      const std::vector<LayerFilter*>& filters);

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
};

// ── Transform decomposition ────────────────────────────────────────────────

PPTWriter::Xform PPTWriter::decomposeXform(float localX, float localY, float localW, float localH,
                                           const Matrix& m) {
  float cx = localX + localW / 2.0f;
  float cy = localY + localH / 2.0f;
  float tcx = m.a * cx + m.c * cy + m.tx;
  float tcy = m.b * cx + m.d * cy + m.ty;

  float sx = std::sqrt(m.a * m.a + m.b * m.b);
  float det = m.a * m.d - m.b * m.c;
  float sy = (sx > 0) ? std::abs(det) / sx : 0;

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
  out.openElement("a:off").addRequiredAttribute("x", xf.offX).addRequiredAttribute("y", xf.offY).closeElementSelfClosing();
  out.openElement("a:ext").addRequiredAttribute("cx", xf.extCX).addRequiredAttribute("cy", xf.extCY).closeElementSelfClosing();
  out.closeElement();  // a:xfrm
}

// ── Shape envelope ─────────────────────────────────────────────────────────

void PPTWriter::beginShape(XMLBuilder& out, const char* name, int64_t offX, int64_t offY,
                           int64_t extCX, int64_t extCY, int rot) {
  int id = _ctx->nextShapeId();
  out.openElement("p:sp").closeElementStart();

  out.openElement("p:nvSpPr").closeElementStart();
  out.openElement("p:cNvPr").addRequiredAttribute("id", id).addRequiredAttribute("name", name).closeElementSelfClosing();
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
  out.openElement("a:endParaRPr").addRequiredAttribute("lang", "zh-CN").addRequiredAttribute("altLang", "en-US").closeElementSelfClosing();
  out.closeElement();  // a:p
  out.closeElement();  // p:txBody
  out.closeElement();  // p:sp
}

void PPTWriter::writeShapeTail(XMLBuilder& out, const FillStrokeInfo& fs, float alpha,
                               const Rect& shapeBounds, bool imageWritten,
                               const std::vector<LayerFilter*>& filters) {
  if (imageWritten) {
    out.openElement("a:noFill").closeElementSelfClosing();
  } else {
    writeFill(out, fs.fill, alpha, shapeBounds);
  }
  writeStroke(out, fs.stroke, alpha);
  writeEffects(out, filters);
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
    out.openElement("a:alphaModFix").addRequiredAttribute("amt", AlphaToPct(alpha)).closeElementSelfClosing();
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

// ── Rasterized picture ──────────────────────────────────────────────────────

void PPTWriter::writePicture(XMLBuilder& out, const std::string& relId, int64_t offX, int64_t offY,
                             int64_t extCX, int64_t extCY) {
  int id = _ctx->nextShapeId();
  out.openElement("p:pic").closeElementStart();

  out.openElement("p:nvPicPr").closeElementStart();
  out.openElement("p:cNvPr").addRequiredAttribute("id", id).addRequiredAttribute("name", "MaskedImage").closeElementSelfClosing();
  out.openElement("p:cNvPicPr").closeElementStart();
  out.openElement("a:picLocks").addRequiredAttribute("noChangeAspect", "1").closeElementSelfClosing();
  out.closeElement();  // p:cNvPicPr
  out.openElement("p:nvPr").closeElementSelfClosing();
  out.closeElement();  // p:nvPicPr

  out.openElement("p:blipFill").closeElementStart();
  WriteBlip(out, relId, 1.0f);
  WriteDefaultStretch(out);
  out.closeElement();  // p:blipFill

  out.openElement("p:spPr").closeElementStart();
  WriteXfrm(out, {offX, offY, extCX, extCY, 0});
  out.openElement("a:prstGeom").addRequiredAttribute("prst", "rect").closeElementStart();
  out.openElement("a:avLst").closeElementSelfClosing();
  out.closeElement();  // a:prstGeom
  out.closeElement();  // p:spPr

  out.closeElement();  // p:pic
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

  int id = _ctx->nextShapeId();
  out.openElement("p:pic").closeElementStart();

  out.openElement("p:nvPicPr").closeElementStart();
  out.openElement("p:cNvPr").addRequiredAttribute("id", id).addRequiredAttribute("name", "Image").closeElementSelfClosing();
  out.openElement("p:cNvPicPr").closeElementStart();
  out.openElement("a:picLocks").addRequiredAttribute("noChangeAspect", "1").closeElementSelfClosing();
  out.closeElement();  // p:cNvPicPr
  out.openElement("p:nvPr").closeElementSelfClosing();
  out.closeElement();  // p:nvPicPr

  out.openElement("p:blipFill").closeElementStart();
  WriteBlip(out, relId, effectiveAlpha);
  if (hasSrcRect) {
    auto& src = out.openElement("a:srcRect");
    if (ipr.srcL != 0) src.addRequiredAttribute("l", ipr.srcL);
    if (ipr.srcT != 0) src.addRequiredAttribute("t", ipr.srcT);
    if (ipr.srcR != 0) src.addRequiredAttribute("r", ipr.srcR);
    if (ipr.srcB != 0) src.addRequiredAttribute("b", ipr.srcB);
    src.closeElementSelfClosing();
  }
  WriteDefaultStretch(out);
  out.closeElement();  // p:blipFill

  out.openElement("p:spPr").closeElementStart();
  WriteXfrm(out, xf);
  out.openElement("a:prstGeom").addRequiredAttribute("prst", "rect").closeElementStart();
  out.openElement("a:avLst").closeElementSelfClosing();
  out.closeElement();  // a:prstGeom
  out.closeElement();  // p:spPr

  out.closeElement();  // p:pic
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
      out.openElement("a:alpha").addRequiredAttribute("val", AlphaToPct(stop->color.alpha)).closeElementSelfClosing();
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
      float effectiveAlpha = solid->color.alpha * alpha;
      out.openElement("a:solidFill").closeElementStart();
      out.openElement("a:srgbClr").addRequiredAttribute("val", ColorToHex6(solid->color));
      if (effectiveAlpha < 1.0f) {
        out.closeElementStart();
        out.openElement("a:alpha").addRequiredAttribute("val", AlphaToPct(effectiveAlpha)).closeElementSelfClosing();
        out.closeElement();
      } else {
        out.closeElementSelfClosing();
      }
      out.closeElement();  // a:solidFill
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
      out.openElement("a:lin").addRequiredAttribute("ang", ang).addRequiredAttribute("scaled", "1").closeElementSelfClosing();
      out.closeElement();  // a:gradFill
      break;
    }
    case NodeType::RadialGradient: {
      auto* grad = static_cast<const RadialGradient*>(source);
      out.openElement("a:gradFill").closeElementStart();
      writeGradientStops(out, grad->colorStops);
      out.openElement("a:path").addRequiredAttribute("path", "circle").closeElementStart();
      out.openElement("a:fillToRect")
          .addRequiredAttribute("l", 50000)
          .addRequiredAttribute("t", 50000)
          .addRequiredAttribute("r", 50000)
          .addRequiredAttribute("b", 50000)
          .closeElementSelfClosing();
      out.closeElement();  // a:path
      out.closeElement();  // a:gradFill
      break;
    }
    case NodeType::ImagePattern: {
      auto* pattern = static_cast<const ImagePattern*>(source);
      if (pattern->image) {
        std::string relId = _ctx->addImage(pattern->image);
        if (!relId.empty()) {
          out.openElement("a:blipFill").closeElementStart();
          WriteBlip(out, relId, alpha);
          int imgW = 0;
          int imgH = 0;
          bool hasDimensions = GetImageDimensions(pattern->image, &imgW, &imgH);
          bool needsTiling =
              (pattern->tileModeX == TileMode::Repeat || pattern->tileModeX == TileMode::Mirror ||
               pattern->tileModeY == TileMode::Repeat || pattern->tileModeY == TileMode::Mirror);
          if (needsTiling && hasDimensions && !shapeBounds.isEmpty()) {
            const auto& M = pattern->matrix;
            float imgDpiX = 72.0f;
            float imgDpiY = 72.0f;
            GetImageDPI(pattern->image, &imgDpiX, &imgDpiY);
            double dpiCorrX = static_cast<double>(imgDpiX) / 96.0;
            double dpiCorrY = static_cast<double>(imgDpiY) / 96.0;
            auto sx = static_cast<int>(std::round(M.a * dpiCorrX * 100000.0));
            auto sy = static_cast<int>(std::round(M.d * dpiCorrY * 100000.0));
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
            bool hasTransform =
                hasDimensions && !shapeBounds.isEmpty() && !pattern->matrix.isIdentity();
            ImagePatternRect ipr = {};
            if (hasTransform && ComputeImagePatternRect(pattern, imgW, imgH, shapeBounds, &ipr)) {
              auto& src = out.openElement("a:srcRect");
              if (ipr.srcL != 0) src.addRequiredAttribute("l", ipr.srcL);
              if (ipr.srcT != 0) src.addRequiredAttribute("t", ipr.srcT);
              if (ipr.srcR != 0) src.addRequiredAttribute("r", ipr.srcR);
              if (ipr.srcB != 0) src.addRequiredAttribute("b", ipr.srcB);
              src.closeElementSelfClosing();
              int fillL = static_cast<int>(
                  std::round((ipr.visL - shapeBounds.x) / shapeBounds.width * 100000.0f));
              int fillT = static_cast<int>(
                  std::round((ipr.visT - shapeBounds.y) / shapeBounds.height * 100000.0f));
              int fillR = static_cast<int>(std::round(
                  (shapeBounds.x + shapeBounds.width - ipr.visR) / shapeBounds.width * 100000.0f));
              int fillB =
                  static_cast<int>(std::round((shapeBounds.y + shapeBounds.height - ipr.visB) /
                                              shapeBounds.height * 100000.0f));
              out.openElement("a:stretch").closeElementStart();
              auto& fr = out.openElement("a:fillRect");
              if (fillL != 0) fr.addRequiredAttribute("l", fillL);
              if (fillT != 0) fr.addRequiredAttribute("t", fillT);
              if (fillR != 0) fr.addRequiredAttribute("r", fillR);
              if (fillB != 0) fr.addRequiredAttribute("b", fillB);
              fr.closeElementSelfClosing();
              out.closeElement();  // a:stretch
            } else {
              WriteDefaultStretch(out);
            }
          }
          out.closeElement();  // a:blipFill
          break;
        }
      }
      out.openElement("a:noFill").closeElementSelfClosing();
      break;
    }
    default:
      out.openElement("a:noFill").closeElementSelfClosing();
      break;
  }
}

// ── Fill / stroke ──────────────────────────────────────────────────────────

void PPTWriter::writeFill(XMLBuilder& out, const Fill* fill, float alpha, const Rect& shapeBounds) {
  if (!fill || !fill->color) {
    out.openElement("a:noFill").closeElementSelfClosing();
    return;
  }
  float effectiveAlpha = fill->alpha * alpha;
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
    out.openElement("a:noFill").closeElementSelfClosing();
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
        out.openElement("a:ds").addRequiredAttribute("d", d).addRequiredAttribute("sp", sp).closeElementSelfClosing();
      }
      if (stroke->dashes.size() % 2 != 0) {
        int d = std::max(1, static_cast<int>(std::round(stroke->dashes.back() / sw * 100000.0)));
        out.openElement("a:ds").addRequiredAttribute("d", d).addRequiredAttribute("sp", d).closeElementSelfClosing();
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
  out.openElement("a:alpha").addRequiredAttribute("val", AlphaToPct(color.alpha)).closeElementSelfClosing();
  out.closeElement();  // a:srgbClr
  out.closeElement();  // tag
}

void PPTWriter::writeEffects(XMLBuilder& out, const std::vector<LayerFilter*>& filters) {
  bool hasShadow = false;
  for (const auto* f : filters) {
    if (f->nodeType() == NodeType::DropShadowFilter ||
        f->nodeType() == NodeType::InnerShadowFilter) {
      hasShadow = true;
      break;
    }
  }
  if (!hasShadow) {
    return;
  }

  out.openElement("a:effectLst").closeElementStart();
  for (const auto* f : filters) {
    if (f->nodeType() == NodeType::DropShadowFilter) {
      auto* s = static_cast<const DropShadowFilter*>(f);
      writeShadowElement(out, "a:outerShdw", s->blurX, s->blurY, s->offsetX, s->offsetY, s->color,
                         true);
    } else if (f->nodeType() == NodeType::InnerShadowFilter) {
      auto* s = static_cast<const InnerShadowFilter*>(f);
      writeShadowElement(out, "a:innerShdw", s->blurX, s->blurY, s->offsetX, s->offsetY, s->color,
                         false);
    }
  }
  out.closeElement();  // a:effectLst
}

// ── Custom geometry ────────────────────────────────────────────────────────

void PPTWriter::WriteContourGeom(XMLBuilder& out, std::vector<PathContour>& contours,
                                 int64_t pathWidth, int64_t pathHeight, float scaleX, float scaleY,
                                 float scaledOfsX, float scaledOfsY, FillRule fillRule) {
  out.openElement("a:custGeom").closeElementStart();
  out.openElement("a:avLst").closeElementSelfClosing();
  out.openElement("a:gdLst").closeElementSelfClosing();
  out.openElement("a:ahLst").closeElementSelfClosing();
  out.openElement("a:cxnLst").closeElementSelfClosing();
  out.openElement("a:rect").addRequiredAttribute("l", "0").addRequiredAttribute("t", "0").addRequiredAttribute("r", "r").addRequiredAttribute("b", "b").closeElementSelfClosing();

  out.openElement("a:pathLst").closeElementStart();

  if (_bridgeContours && contours.size() > 1) {
    auto depths = ComputeContainmentDepths(contours);
    if (fillRule == FillRule::EvenOdd) {
      AdjustWindingForEvenOdd(contours, depths);
    }
    auto groups = GroupContoursByOutermost(contours, depths);
    for (const auto& group : groups) {
      out.openElement("a:path").addRequiredAttribute("w", pathWidth).addRequiredAttribute("h", pathHeight).closeElementStart();
      if (group.size() > 1) {
        EmitBridgedGroup(out, contours, group, scaleX, scaleY, scaledOfsX, scaledOfsY);
      } else {
        EmitContour(out, contours[group[0]], scaleX, scaleY, scaledOfsX, scaledOfsY);
      }
      out.closeElement();  // a:path
    }
  } else {
    out.openElement("a:path").addRequiredAttribute("w", pathWidth).addRequiredAttribute("h", pathHeight).closeElementStart();
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
                               const std::vector<LayerFilter*>& filters) {
  float x = rect->position.x - rect->size.width / 2.0f;
  float y = rect->position.y - rect->size.height / 2.0f;
  Rect shapeBounds = Rect::MakeXYWH(x, y, rect->size.width, rect->size.height);

  bool imageWritten = writeImagePatternAsPicture(out, fs.fill, shapeBounds, m, alpha);
  if (imageWritten && !fs.stroke && filters.empty()) {
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
    out.openElement("a:gd").addRequiredAttribute("name", "adj").addRequiredAttribute("fmla", "val " + std::to_string(adj)).closeElementSelfClosing();
    out.closeElement();  // a:avLst
    out.closeElement();  // a:prstGeom
  } else {
    out.openElement("a:prstGeom").addRequiredAttribute("prst", "rect").closeElementStart();
    out.openElement("a:avLst").closeElementSelfClosing();
    out.closeElement();
  }

  writeShapeTail(out, fs, alpha, shapeBounds, imageWritten, filters);
}

void PPTWriter::writeEllipse(XMLBuilder& out, const Ellipse* ellipse, const FillStrokeInfo& fs,
                             const Matrix& m, float alpha,
                             const std::vector<LayerFilter*>& filters) {
  float rx = ellipse->size.width / 2.0f;
  float ry = ellipse->size.height / 2.0f;
  float x = ellipse->position.x - rx;
  float y = ellipse->position.y - ry;
  Rect shapeBounds = Rect::MakeXYWH(x, y, ellipse->size.width, ellipse->size.height);

  bool imageWritten = writeImagePatternAsPicture(out, fs.fill, shapeBounds, m, alpha);
  if (imageWritten && !fs.stroke && filters.empty()) {
    return;
  }

  auto xf = decomposeXform(x, y, ellipse->size.width, ellipse->size.height, m);
  beginShape(out, "Ellipse", xf.offX, xf.offY, xf.extCX, xf.extCY, xf.rotation);

  out.openElement("a:prstGeom").addRequiredAttribute("prst", "ellipse").closeElementStart();
  out.openElement("a:avLst").closeElementSelfClosing();
  out.closeElement();

  writeShapeTail(out, fs, alpha, shapeBounds, imageWritten, filters);
}

void PPTWriter::writePath(XMLBuilder& out, const Path* path, const FillStrokeInfo& fs,
                          const Matrix& m, float alpha, const std::vector<LayerFilter*>& filters) {
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
  if (imageWritten && !fs.stroke && filters.empty()) {
    return;
  }

  auto xf = decomposeXform(adjustedX, adjustedY, adjustedW, adjustedH, m);
  FillRule fillRule = (fs.fill) ? fs.fill->fillRule : FillRule::Winding;

  if (_bridgeContours) {
    auto contours = ParsePathContours(path->data);
    if (contours.size() > 1) {
      auto groups = PrepareContourGroups(contours, fillRule);
      if (groups.size() > 1) {
        int64_t pw = std::max(int64_t(1), PxToEMU(adjustedW));
        int64_t ph = std::max(int64_t(1), PxToEMU(adjustedH));
        for (const auto& group : groups) {
          beginShape(out, "Path", xf.offX, xf.offY, xf.extCX, xf.extCY, xf.rotation);
          EmitGroupCustGeom(out, contours, group, pw, ph, 1.0f, 1.0f, adjustedX, adjustedY);
          writeShapeTail(out, fs, alpha, shapeBounds, imageWritten, filters);
        }
        return;
      }
    }
  }

  beginShape(out, "Path", xf.offX, xf.offY, xf.extCX, xf.extCY, xf.rotation);
  writeCustomGeom(out, path->data, adjustedX, adjustedY, adjustedW, adjustedH, fillRule);
  writeShapeTail(out, fs, alpha, shapeBounds, imageWritten, filters);
}

void PPTWriter::writeTextAsPath(XMLBuilder& out, const Text* text, const FillStrokeInfo& fs,
                                const Matrix& m, float alpha,
                                const std::vector<LayerFilter*>& /*filters*/) {
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

  float sx = std::sqrt(m.a * m.a + m.b * m.b);
  float det = m.a * m.d - m.b * m.c;
  float sy = (sx > 0) ? std::abs(det) / sx : 0;
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
      writeFill(out, fs.fill, alpha);
      out.openElement("a:ln").closeElementStart();
      out.openElement("a:noFill").closeElementSelfClosing();
      out.closeElement();
      endShape(out);
    }
  } else {
    beginShape(out, "Glyph", xf.offX, xf.offY, xf.extCX, xf.extCY, xf.rotation);
    WriteContourGeom(out, allContours, pw, ph, sx, sy, scaledOfsX, scaledOfsY, FillRule::EvenOdd);
    writeFill(out, fs.fill, alpha);
    out.openElement("a:ln").closeElementStart();
    out.openElement("a:noFill").closeElementSelfClosing();
    out.closeElement();
    endShape(out);
  }
}

void PPTWriter::writeNativeText(XMLBuilder& out, const Text* text, const FillStrokeInfo& fs,
                                const Matrix& m, float alpha,
                                const std::vector<LayerFilter*>& filters) {
  if (text->text.empty()) {
    return;
  }

  float estWidth = static_cast<float>(CountUTF8Characters(text->text)) * text->fontSize * 0.6f;
  float estHeight = text->fontSize * 1.4f;
  float posX = text->position.x;
  float posY = text->position.y - text->fontSize * 0.85f;
  bool hasTextBox = false;

  if (fs.textBox && !std::isnan(fs.textBox->width) && fs.textBox->width > 0) {
    hasTextBox = true;
    posX = fs.textBox->position.x;
    posY = fs.textBox->position.y;
    estWidth = fs.textBox->width;
    estHeight = (!std::isnan(fs.textBox->height) && fs.textBox->height > 0) ? fs.textBox->height
                                                                            : text->fontSize * 1.4f;
  } else {
    if (text->textAnchor == TextAnchor::Center) {
      posX -= estWidth / 2.0f;
    } else if (text->textAnchor == TextAnchor::End) {
      posX -= estWidth;
    }
  }

  auto xf = decomposeXform(posX, posY, estWidth, estHeight, m);

  int id = _ctx->nextShapeId();
  out.openElement("p:sp").closeElementStart();
  out.openElement("p:nvSpPr").closeElementStart();
  out.openElement("p:cNvPr").addRequiredAttribute("id", id).addRequiredAttribute("name", "TextBox").closeElementSelfClosing();
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
  writeEffects(out, filters);
  out.closeElement();  // p:spPr

  out.openElement("p:txBody").closeElementStart();
  auto& bodyPr = out.openElement("a:bodyPr")
                     .addRequiredAttribute("wrap", hasTextBox ? "square" : "none")
                     .addRequiredAttribute("lIns", "0")
                     .addRequiredAttribute("tIns", "0")
                     .addRequiredAttribute("rIns", "0")
                     .addRequiredAttribute("bIns", "0");
  if (fs.textBox) {
    if (fs.textBox->paragraphAlign == ParagraphAlign::Middle) {
      bodyPr.addRequiredAttribute("anchor", "ctr");
    } else if (fs.textBox->paragraphAlign == ParagraphAlign::Far) {
      bodyPr.addRequiredAttribute("anchor", "b");
    }
  }
  bodyPr.closeElementSelfClosing();
  out.openElement("a:lstStyle").closeElementSelfClosing();

  // Split text by newlines into paragraphs
  std::string remaining = text->text;
  size_t pos = 0;
  while (pos <= remaining.size()) {
    size_t nl = remaining.find('\n', pos);
    std::string line =
        (nl == std::string::npos) ? remaining.substr(pos) : remaining.substr(pos, nl - pos);
    out.openElement("a:p").closeElementStart();

    const char* algn = nullptr;
    if (text->textAnchor == TextAnchor::Center) {
      algn = "ctr";
    } else if (text->textAnchor == TextAnchor::End) {
      algn = "r";
    }
    if (fs.textBox) {
      if (fs.textBox->textAlign == TextAlign::Center) {
        algn = "ctr";
      } else if (fs.textBox->textAlign == TextAlign::End) {
        algn = "r";
      }
    }
    if (algn) {
      out.openElement("a:pPr").addRequiredAttribute("algn", algn).closeElementSelfClosing();
    }

    out.openElement("a:r").closeElementStart();
    out.openElement("a:rPr").addRequiredAttribute("lang", "en-US").addRequiredAttribute("sz", FontSizeToPPT(text->fontSize));
    bool hasBold = text->fontStyle.find("Bold") != std::string::npos;
    bool hasItalic = text->fontStyle.find("Italic") != std::string::npos;
    if (hasBold) {
      out.addRequiredAttribute("b", "1");
    }
    if (hasItalic) {
      out.addRequiredAttribute("i", "1");
    }
    if (text->letterSpacing != 0.0f) {
      out.addRequiredAttribute("spc", static_cast<int64_t>(std::round(text->letterSpacing * 75.0)));
    }
    out.closeElementStart();

    if (fs.fill && fs.fill->color && fs.fill->color->nodeType() == NodeType::SolidColor) {
      auto* solid = static_cast<const SolidColor*>(fs.fill->color);
      float ea = solid->color.alpha * fs.fill->alpha * alpha;
      out.openElement("a:solidFill").closeElementStart();
      out.openElement("a:srgbClr").addRequiredAttribute("val", ColorToHex6(solid->color));
      if (ea < 1.0f) {
        out.closeElementStart();
        out.openElement("a:alpha").addRequiredAttribute("val", AlphaToPct(ea)).closeElementSelfClosing();
        out.closeElement();
      } else {
        out.closeElementSelfClosing();
      }
      out.closeElement();  // a:solidFill
    }

    if (!text->fontFamily.empty()) {
      auto typeface = StripQuotes(text->fontFamily);
      out.openElement("a:latin").addRequiredAttribute("typeface", typeface).closeElementSelfClosing();
      out.openElement("a:ea").addRequiredAttribute("typeface", typeface).closeElementSelfClosing();
    }

    out.closeElement();  // a:rPr
    out.openElement("a:t").closeElementStart();
    out.addTextContent(line);
    out.closeElement();  // a:t
    out.closeElement();  // a:r
    out.closeElement();  // a:p

    if (nl == std::string::npos) {
      break;
    }
    pos = nl + 1;
  }

  out.closeElement();  // p:txBody
  out.closeElement();  // p:sp
}

// ── Element / layer traversal ──────────────────────────────────────────────

void PPTWriter::writeElements(XMLBuilder& out, const std::vector<Element*>& elements,
                              const Matrix& transform, float alpha,
                              const std::vector<LayerFilter*>& filters) {
  auto fs = CollectFillStroke(elements);

  for (const auto* element : elements) {
    auto type = element->nodeType();
    if (type == NodeType::Fill || type == NodeType::Stroke || type == NodeType::TextBox) {
      continue;
    }
    switch (type) {
      case NodeType::Rectangle:
        writeRectangle(out, static_cast<const Rectangle*>(element), fs, transform, alpha, filters);
        break;
      case NodeType::Ellipse:
        writeEllipse(out, static_cast<const Ellipse*>(element), fs, transform, alpha, filters);
        break;
      case NodeType::Path:
        writePath(out, static_cast<const Path*>(element), fs, transform, alpha, filters);
        break;
      case NodeType::Text: {
        auto* text = static_cast<const Text*>(element);
        if (_convertTextToPath && !text->glyphRuns.empty()) {
          writeTextAsPath(out, text, fs, transform, alpha, filters);
        } else {
          writeNativeText(out, text, fs, transform, alpha, filters);
        }
        break;
      }
      case NodeType::Group: {
        auto* group = static_cast<const Group*>(element);
        Matrix groupMatrix = BuildGroupMatrix(group);
        Matrix combined = transform * groupMatrix;
        float groupAlpha = alpha * group->alpha;
        writeElements(out, group->elements, combined, groupAlpha, filters);
        break;
      }
      default:
        break;
    }
  }
}

void PPTWriter::writeLayer(XMLBuilder& out, const Layer* layer, const Matrix& parentMatrix,
                           float parentAlpha) {
  if (!layer->visible && layer->mask == nullptr) {
    return;
  }

  Matrix layerMatrix = parentMatrix * BuildLayerMatrix(layer);
  float layerAlpha = parentAlpha * layer->alpha;

  if (layer->mask != nullptr && _bakeMask) {
    auto& buildResult = ensureBuildResult();
    auto it = buildResult.layerMap.find(layer);
    if (it != buildResult.layerMap.end()) {
      auto tgfxLayer = it->second;
      auto pngData = RenderMaskedLayer(buildResult.root, tgfxLayer);
      if (pngData) {
        auto bounds = tgfxLayer->getBounds(buildResult.root.get(), true);
        auto offX = PxToEMU(bounds.left);
        auto offY = PxToEMU(bounds.top);
        auto extCX = std::max(int64_t(1), PxToEMU(bounds.width()));
        auto extCY = std::max(int64_t(1), PxToEMU(bounds.height()));
        auto relId = _ctx->addRawImage(std::move(pngData));
        writePicture(out, relId, offX, offY, extCX, extCY);
      }
    }
    return;
  }

  writeElements(out, layer->contents, layerMatrix, layerAlpha, layer->filters);

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

  PPTWriterContext context;
  PPTWriter writer(&context, &doc, options.convertTextToPath, options.bakeMask,
                   options.bridgeContours);

  // Build slide body content
  XMLBuilder body(false, 2, 0, 16384);
  for (const auto* layer : doc.layers) {
    writer.writeLayer(body, layer);
  }
  std::string bodyContent = body.release();

  // Assemble slide XML
  std::string slide;
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

  AddZipString(zf, "[Content_Types].xml", GenerateContentTypes(context));
  AddZipString(zf, "_rels/.rels", GenerateRootRels());
  AddZipString(zf, "ppt/presentation.xml", GeneratePresentation(doc.width, doc.height));
  AddZipString(zf, "ppt/_rels/presentation.xml.rels", GeneratePresentationRels());
  AddZipString(zf, "ppt/slides/slide1.xml", slide);
  AddZipString(zf, "ppt/slides/_rels/slide1.xml.rels", GenerateSlideRels(context));
  AddZipString(zf, "ppt/slideMasters/slideMaster1.xml", GenerateSlideMaster());
  AddZipString(zf, "ppt/slideMasters/_rels/slideMaster1.xml.rels", GenerateSlideMasterRels());
  AddZipString(zf, "ppt/slideLayouts/slideLayout1.xml", GenerateSlideLayout());
  AddZipString(zf, "ppt/slideLayouts/_rels/slideLayout1.xml.rels", GenerateSlideLayoutRels());
  AddZipString(zf, "ppt/theme/theme1.xml", GenerateTheme());
  AddZipString(zf, "ppt/presProps.xml", GeneratePresProps());
  AddZipString(zf, "ppt/viewProps.xml", GenerateViewProps());
  AddZipString(zf, "ppt/tableStyles.xml", GenerateTableStyles());
  AddZipString(zf, "docProps/core.xml", GenerateCoreProps());
  AddZipString(zf, "docProps/app.xml", GenerateAppProps());

  for (const auto& img : context.images()) {
    if (img.cachedData && img.cachedData->size() > 0) {
      AddZipEntry(zf, img.mediaPath.c_str(), img.cachedData->bytes(),
                  static_cast<unsigned>(img.cachedData->size()));
    }
  }

  return zipClose(zf, nullptr) == ZIP_OK;
}

}  // namespace pagx
