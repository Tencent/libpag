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
#include <unordered_map>
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
#include "pagx/types/Rect.h"
#include "pagx/utils/ExporterUtils.h"
#include "pagx/utils/StringParser.h"
#include "pagx/xml/XMLBuilder.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/core/Data.h"
#include "tgfx/layers/DisplayList.h"
#include "zip.h"

namespace pagx {

using pag::FloatNearlyZero;
using pag::RadiansToDegrees;

//==============================================================================
// Coordinate / color / angle helpers
//==============================================================================

static constexpr double kEMUPerPx = 9525.0;

static int64_t PxToEMU(float px) {
  return static_cast<int64_t>(std::round(static_cast<double>(px) * kEMUPerPx));
}

static std::string I64(int64_t v) {
  return std::to_string(v);
}

static std::string ColorToHex6(const Color& c) {
  char buf[7];
  snprintf(buf, sizeof(buf), "%02X%02X%02X",
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

// XMLBuilder is provided by pagx/xml/XMLBuilder.h

//==============================================================================
// PPTWriterContext – image tracking and ID generation
//==============================================================================

struct ImageEntry {
  const Image* image = nullptr;
  std::shared_ptr<tgfx::Data> cachedData = nullptr;
  std::string relId;
  std::string mediaPath;
  bool isJPEG = false;
};

class PPTWriterContext {
 public:
  int nextShapeId() {
    return _shapeId++;
  }

  std::string addImage(const Image* image) {
    auto it = _imageMap.find(image);
    if (it != _imageMap.end()) {
      return it->second;
    }
    auto data = GetImageData(image);
    if (!data || data->size() == 0) {
      return {};
    }
    int idx = static_cast<int>(_images.size()) + 1;
    std::string relId = "rId" + std::to_string(_nextRelId++);
    bool jpeg = IsJPEG(data->bytes(), data->size());
    std::string ext = jpeg ? "jpeg" : "png";
    std::string mediaPath = "ppt/media/image" + std::to_string(idx) + "." + ext;
    _images.push_back({image, std::move(data), relId, mediaPath, jpeg});
    _imageMap[image] = relId;
    if (jpeg) {
      _hasJPEG = true;
    } else {
      _hasPNG = true;
    }
    return relId;
  }

  std::string addRawImage(std::shared_ptr<tgfx::Data> pngData) {
    int idx = static_cast<int>(_images.size()) + 1;
    std::string relId = "rId" + std::to_string(_nextRelId++);
    std::string mediaPath = "ppt/media/image" + std::to_string(idx) + ".png";
    _images.push_back({nullptr, std::move(pngData), relId, mediaPath, false});
    _hasPNG = true;
    return relId;
  }

  const std::vector<ImageEntry>& images() const {
    return _images;
  }

  bool hasJPEG() const {
    return _hasJPEG;
  }

  bool hasPNG() const {
    return _hasPNG;
  }

 private:
  int _shapeId = 2;
  int _nextRelId = 2;
  bool _hasJPEG = false;
  bool _hasPNG = false;
  std::vector<ImageEntry> _images;
  std::unordered_map<const Image*, std::string> _imageMap;
};

//==============================================================================
// Dash pattern → OOXML preset dash mapping
//==============================================================================

static const char* MatchPresetDash(const std::vector<float>& dashes, float strokeWidth) {
  if (dashes.empty() || strokeWidth <= 0) {
    return nullptr;
  }
  auto ratio = [&](size_t i) -> float { return (i < dashes.size()) ? dashes[i] / strokeWidth : 0; };
  size_t n = dashes.size();
  if (n == 2) {
    float dr = ratio(0);
    float sr = ratio(1);
    if (dr <= 1.5f) {
      return (sr <= 2.0f) ? "sysDot" : "dot";
    }
    if (dr <= 4.5f) {
      return (sr <= 2.0f) ? "sysDash" : "dash";
    }
    return "lgDash";
  }
  if (n == 4) {
    float dr = ratio(0);
    return (dr > 4.5f) ? "lgDashDot" : "dashDot";
  }
  if (n == 6) {
    float dr = ratio(0);
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
  float imageDocW = static_cast<float>(imgW) * M.a;
  float imageDocH = static_cast<float>(imgH) * M.d;
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

struct PathContour;

class PPTWriter {
 public:
  PPTWriter(PPTWriterContext* ctx, const PAGXDocument* doc, bool convertTextToPath, bool bakeMask)
      : _ctx(ctx), _doc(doc), _convertTextToPath(convertTextToPath), _bakeMask(bakeMask) {
  }

  void writeLayer(XMLBuilder& out, const Layer* layer, const Matrix& parentMatrix = {},
                  float parentAlpha = 1.0f);

 private:
  PPTWriterContext* _ctx;
  const PAGXDocument* _doc;
  bool _convertTextToPath;
  bool _bakeMask;
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
  static void WriteContourGeom(XMLBuilder& out, std::vector<PathContour>& contours,
                               int64_t pathWidth, int64_t pathHeight, float scaleX, float scaleY,
                               float scaledOfsX, float scaledOfsY, FillRule fillRule);

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
    out.open("a:xfrm").a("rot", xf.rotation).gt();
  } else {
    out.open("a:xfrm").gt();
  }
  out.open("a:off").a("x", xf.offX).a("y", xf.offY).sc();
  out.open("a:ext").a("cx", xf.extCX).a("cy", xf.extCY).sc();
  out.end();  // a:xfrm
}

// ── Shape envelope ─────────────────────────────────────────────────────────

void PPTWriter::beginShape(XMLBuilder& out, const char* name, int64_t offX, int64_t offY,
                           int64_t extCX, int64_t extCY, int rot) {
  int id = _ctx->nextShapeId();
  out.open("p:sp").gt();

  out.open("p:nvSpPr").gt();
  out.open("p:cNvPr").a("id", id).a("name", name).sc();
  out.open("p:cNvSpPr").sc();
  out.open("p:nvPr").sc();
  out.end();  // p:nvSpPr

  out.open("p:spPr").gt();
  WriteXfrm(out, {offX, offY, extCX, extCY, rot});
}

void PPTWriter::endShape(XMLBuilder& out) {
  out.end();  // p:spPr
  out.open("p:txBody").gt();
  out.open("a:bodyPr").sc();
  out.open("a:lstStyle").sc();
  out.open("a:p").gt();
  out.open("a:endParaRPr").a("lang", "zh-CN").a("altLang", "en-US").sc();
  out.end();  // a:p
  out.end();  // p:txBody
  out.end();  // p:sp
}

void PPTWriter::writeShapeTail(XMLBuilder& out, const FillStrokeInfo& fs, float alpha,
                               const Rect& shapeBounds, bool imageWritten,
                               const std::vector<LayerFilter*>& filters) {
  if (imageWritten) {
    out.open("a:noFill").sc();
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
    _buildResult = LayerBuilder::BuildWithMap(const_cast<PAGXDocument*>(_doc));
    _buildResultReady = true;
  }
  return _buildResult;
}

// ── Shared XML helpers ──────────────────────────────────────────────────────

void PPTWriter::WriteBlip(XMLBuilder& out, const std::string& relId, float alpha) {
  out.open("a:blip").a("r:embed", relId);
  if (alpha < 1.0f) {
    out.gt();
    out.open("a:alphaModFix").a("amt", AlphaToPct(alpha)).sc();
    out.end();  // a:blip
  } else {
    out.sc();
  }
}

void PPTWriter::WriteDefaultStretch(XMLBuilder& out) {
  out.open("a:stretch").gt();
  out.open("a:fillRect").sc();
  out.end();  // a:stretch
}

// ── Rasterized picture ──────────────────────────────────────────────────────

void PPTWriter::writePicture(XMLBuilder& out, const std::string& relId, int64_t offX, int64_t offY,
                             int64_t extCX, int64_t extCY) {
  int id = _ctx->nextShapeId();
  out.open("p:pic").gt();

  out.open("p:nvPicPr").gt();
  out.open("p:cNvPr").a("id", id).a("name", "MaskedImage").sc();
  out.open("p:cNvPicPr").gt();
  out.open("a:picLocks").a("noChangeAspect", "1").sc();
  out.end();  // p:cNvPicPr
  out.open("p:nvPr").sc();
  out.end();  // p:nvPicPr

  out.open("p:blipFill").gt();
  WriteBlip(out, relId, 1.0f);
  WriteDefaultStretch(out);
  out.end();  // p:blipFill

  out.open("p:spPr").gt();
  WriteXfrm(out, {offX, offY, extCX, extCY, 0});
  out.open("a:prstGeom").a("prst", "rect").gt();
  out.open("a:avLst").sc();
  out.end();  // a:prstGeom
  out.end();  // p:spPr

  out.end();  // p:pic
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
  out.open("p:pic").gt();

  out.open("p:nvPicPr").gt();
  out.open("p:cNvPr").a("id", id).a("name", "Image").sc();
  out.open("p:cNvPicPr").gt();
  out.open("a:picLocks").a("noChangeAspect", "1").sc();
  out.end();  // p:cNvPicPr
  out.open("p:nvPr").sc();
  out.end();  // p:nvPicPr

  out.open("p:blipFill").gt();
  WriteBlip(out, relId, effectiveAlpha);
  if (hasSrcRect) {
    auto& src = out.open("a:srcRect");
    if (ipr.srcL != 0) src.a("l", ipr.srcL);
    if (ipr.srcT != 0) src.a("t", ipr.srcT);
    if (ipr.srcR != 0) src.a("r", ipr.srcR);
    if (ipr.srcB != 0) src.a("b", ipr.srcB);
    src.sc();
  }
  WriteDefaultStretch(out);
  out.end();  // p:blipFill

  out.open("p:spPr").gt();
  WriteXfrm(out, xf);
  out.open("a:prstGeom").a("prst", "rect").gt();
  out.open("a:avLst").sc();
  out.end();  // a:prstGeom
  out.end();  // p:spPr

  out.end();  // p:pic
  return true;
}

// ── Color source / gradient ────────────────────────────────────────────────

void PPTWriter::writeGradientStops(XMLBuilder& out, const std::vector<ColorStop*>& stops) {
  out.open("a:gsLst").gt();
  for (const auto* stop : stops) {
    int pos = std::clamp(static_cast<int>(std::round(stop->offset * 100000.0f)), 0, 100000);
    out.open("a:gs").a("pos", pos).gt();
    out.open("a:srgbClr").a("val", ColorToHex6(stop->color));
    if (stop->color.alpha < 1.0f) {
      out.gt();
      out.open("a:alpha").a("val", AlphaToPct(stop->color.alpha)).sc();
      out.end();  // a:srgbClr
    } else {
      out.sc();
    }
    out.end();  // a:gs
  }
  out.end();  // a:gsLst
}

void PPTWriter::writeColorSource(XMLBuilder& out, const ColorSource* source, float alpha,
                                 const Rect& shapeBounds) {
  if (!source) {
    out.open("a:noFill").sc();
    return;
  }

  switch (source->nodeType()) {
    case NodeType::SolidColor: {
      auto* solid = static_cast<const SolidColor*>(source);
      float effectiveAlpha = solid->color.alpha * alpha;
      out.open("a:solidFill").gt();
      out.open("a:srgbClr").a("val", ColorToHex6(solid->color));
      if (effectiveAlpha < 1.0f) {
        out.gt();
        out.open("a:alpha").a("val", AlphaToPct(effectiveAlpha)).sc();
        out.end();
      } else {
        out.sc();
      }
      out.end();  // a:solidFill
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

      out.open("a:gradFill").gt();
      writeGradientStops(out, grad->colorStops);
      out.open("a:lin").a("ang", ang).a("scaled", "1").sc();
      out.end();  // a:gradFill
      break;
    }
    case NodeType::RadialGradient: {
      auto* grad = static_cast<const RadialGradient*>(source);
      out.open("a:gradFill").gt();
      writeGradientStops(out, grad->colorStops);
      out.open("a:path").a("path", "circle").gt();
      out.open("a:fillToRect").a("l", 50000).a("t", 50000).a("r", 50000).a("b", 50000).sc();
      out.end();  // a:path
      out.end();  // a:gradFill
      break;
    }
    case NodeType::ImagePattern: {
      auto* pattern = static_cast<const ImagePattern*>(source);
      if (pattern->image) {
        std::string relId = _ctx->addImage(pattern->image);
        if (!relId.empty()) {
          out.open("a:blipFill").gt();
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
            out.open("a:tile")
                .a("tx", tx)
                .a("ty", ty)
                .a("sx", sx)
                .a("sy", sy)
                .a("flip", flip)
                .a("algn", "tl")
                .sc();
          } else {
            bool hasTransform =
                hasDimensions && !shapeBounds.isEmpty() && !pattern->matrix.isIdentity();
            ImagePatternRect ipr = {};
            if (hasTransform && ComputeImagePatternRect(pattern, imgW, imgH, shapeBounds, &ipr)) {
              auto& src = out.open("a:srcRect");
              if (ipr.srcL != 0) src.a("l", ipr.srcL);
              if (ipr.srcT != 0) src.a("t", ipr.srcT);
              if (ipr.srcR != 0) src.a("r", ipr.srcR);
              if (ipr.srcB != 0) src.a("b", ipr.srcB);
              src.sc();
              int fillL = static_cast<int>(
                  std::round((ipr.visL - shapeBounds.x) / shapeBounds.width * 100000.0f));
              int fillT = static_cast<int>(
                  std::round((ipr.visT - shapeBounds.y) / shapeBounds.height * 100000.0f));
              int fillR = static_cast<int>(std::round(
                  (shapeBounds.x + shapeBounds.width - ipr.visR) / shapeBounds.width * 100000.0f));
              int fillB =
                  static_cast<int>(std::round((shapeBounds.y + shapeBounds.height - ipr.visB) /
                                              shapeBounds.height * 100000.0f));
              out.open("a:stretch").gt();
              auto& fr = out.open("a:fillRect");
              if (fillL != 0) fr.a("l", fillL);
              if (fillT != 0) fr.a("t", fillT);
              if (fillR != 0) fr.a("r", fillR);
              if (fillB != 0) fr.a("b", fillB);
              fr.sc();
              out.end();  // a:stretch
            } else {
              WriteDefaultStretch(out);
            }
          }
          out.end();  // a:blipFill
          break;
        }
      }
      out.open("a:noFill").sc();
      break;
    }
    default:
      out.open("a:noFill").sc();
      break;
  }
}

// ── Fill / stroke ──────────────────────────────────────────────────────────

void PPTWriter::writeFill(XMLBuilder& out, const Fill* fill, float alpha, const Rect& shapeBounds) {
  if (!fill || !fill->color) {
    out.open("a:noFill").sc();
    return;
  }
  float effectiveAlpha = fill->alpha * alpha;
  writeColorSource(out, fill->color, effectiveAlpha, shapeBounds);
}

void PPTWriter::writeStroke(XMLBuilder& out, const Stroke* stroke, float alpha) {
  if (!stroke) {
    out.open("a:ln").gt();
    out.open("a:noFill").sc();
    out.end();
    return;
  }

  int64_t w = PxToEMU(stroke->width);
  out.open("a:ln").a("w", w);
  if (stroke->cap == LineCap::Round) {
    out.a("cap", "rnd");
  } else if (stroke->cap == LineCap::Square) {
    out.a("cap", "sq");
  }
  out.gt();

  float effectiveAlpha = stroke->alpha * alpha;
  if (stroke->color) {
    writeColorSource(out, stroke->color, effectiveAlpha);
  } else {
    out.open("a:noFill").sc();
  }

  if (!stroke->dashes.empty()) {
    float sw = (stroke->width > 0) ? stroke->width : 1.0f;
    const char* preset = MatchPresetDash(stroke->dashes, sw);
    if (preset) {
      out.open("a:prstDash").a("val", preset).sc();
    } else {
      out.open("a:custDash").gt();
      for (size_t i = 0; i + 1 < stroke->dashes.size(); i += 2) {
        int d = std::max(1, static_cast<int>(std::round(stroke->dashes[i] / sw * 100000.0)));
        int sp = std::max(1, static_cast<int>(std::round(stroke->dashes[i + 1] / sw * 100000.0)));
        out.open("a:ds").a("d", d).a("sp", sp).sc();
      }
      if (stroke->dashes.size() % 2 != 0) {
        int d = std::max(1, static_cast<int>(std::round(stroke->dashes.back() / sw * 100000.0)));
        out.open("a:ds").a("d", d).a("sp", d).sc();
      }
      out.end();  // a:custDash
    }
  }

  if (stroke->join == LineJoin::Round) {
    out.open("a:round").sc();
  } else if (stroke->join == LineJoin::Bevel) {
    out.open("a:bevel").sc();
  } else {
    int lim = static_cast<int>(std::round(stroke->miterLimit * 100000.0f));
    out.open("a:miter").a("lim", lim).sc();
  }

  out.end();  // a:ln
}

// ── Effects (shadow) ───────────────────────────────────────────────────────

void PPTWriter::writeShadowElement(XMLBuilder& out, const char* tag, float blurX, float blurY,
                                   float offsetX, float offsetY, const Color& color,
                                   bool includeAlign) {
  float blur = (blurX + blurY) / 2.0f;
  float dist = std::sqrt(offsetX * offsetX + offsetY * offsetY);
  float dir = RadiansToDegrees(std::atan2(offsetY, offsetX));
  auto& builder = out.open(tag)
                      .a("blurRad", PxToEMU(blur))
                      .a("dist", PxToEMU(dist))
                      .a("dir", AngleToPPT(dir + 90.0f));
  if (includeAlign) {
    builder.a("algn", "ctr").a("rotWithShape", "0");
  }
  builder.gt();
  out.open("a:srgbClr").a("val", ColorToHex6(color)).gt();
  out.open("a:alpha").a("val", AlphaToPct(color.alpha)).sc();
  out.end();  // a:srgbClr
  out.end();  // tag
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

  out.open("a:effectLst").gt();
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
  out.end();  // a:effectLst
}

// ── Even-odd contour processing ───────────────────────────────────────────

struct PathSeg {
  PathVerb verb = PathVerb::Move;
  Point pts[3] = {};
};

struct PathContour {
  Point start = {};
  std::vector<PathSeg> segs;
  bool closed = false;
};

static Point SegEndpoint(const PathSeg& seg) {
  if (seg.verb == PathVerb::Quad) {
    return seg.pts[1];
  }
  if (seg.verb == PathVerb::Cubic) {
    return seg.pts[2];
  }
  return seg.pts[0];
}

static float ComputeSignedArea(const PathContour& contour) {
  float area = 0;
  Point prev = contour.start;
  for (const auto& seg : contour.segs) {
    Point next = SegEndpoint(seg);
    area += (prev.x * next.y - next.x * prev.y);
    prev = next;
  }
  area += (prev.x * contour.start.y - contour.start.x * prev.y);
  return area;
}

static void ReverseContour(PathContour& c) {
  size_t n = c.segs.size();
  Point originalStart = c.start;
  c.start = SegEndpoint(c.segs[n - 1]);

  std::vector<PathSeg> rev;
  rev.reserve(n);
  for (int i = static_cast<int>(n) - 1; i >= 0; i--) {
    Point dest = (i > 0) ? SegEndpoint(c.segs[i - 1]) : originalStart;
    const auto& seg = c.segs[i];
    PathSeg reversed;
    if (seg.verb == PathVerb::Cubic) {
      reversed.verb = PathVerb::Cubic;
      reversed.pts[0] = seg.pts[1];
      reversed.pts[1] = seg.pts[0];
      reversed.pts[2] = dest;
    } else if (seg.verb == PathVerb::Quad) {
      reversed.verb = PathVerb::Quad;
      reversed.pts[0] = seg.pts[0];
      reversed.pts[1] = dest;
    } else {
      reversed.verb = PathVerb::Line;
      reversed.pts[0] = dest;
    }
    rev.push_back(reversed);
  }
  c.segs = std::move(rev);
}

// Ray-casting point-in-polygon test using contour endpoint polygon approximation.
static bool PointInsideContour(const Point& pt, const PathContour& contour) {
  std::vector<Point> poly;
  poly.push_back(contour.start);
  for (const auto& seg : contour.segs) {
    poly.push_back(SegEndpoint(seg));
  }
  bool inside = false;
  for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
    if (((poly[i].y > pt.y) != (poly[j].y > pt.y)) &&
        (pt.x <
         (poly[j].x - poly[i].x) * (pt.y - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x)) {
      inside = !inside;
    }
  }
  return inside;
}

static std::vector<PathContour> ParsePathContours(const PathData* data) {
  std::vector<PathContour> contours;
  data->forEach([&](PathVerb verb, const Point* pts) {
    if (verb == PathVerb::Move) {
      PathContour c;
      c.start = pts[0];
      contours.push_back(std::move(c));
    } else if (!contours.empty()) {
      if (verb == PathVerb::Close) {
        contours.back().closed = true;
      } else {
        PathSeg seg;
        seg.verb = verb;
        int ptCount = PathData::PointsPerVerb(verb);
        for (int i = 0; i < ptCount; i++) {
          seg.pts[i] = pts[i];
        }
        contours.back().segs.push_back(seg);
      }
    }
  });
  return contours;
}

static std::vector<int> ComputeContainmentDepths(const std::vector<PathContour>& contours) {
  std::vector<int> depths(contours.size(), 0);
  for (size_t i = 0; i < contours.size(); i++) {
    for (size_t j = 0; j < contours.size(); j++) {
      if (i == j || !contours[j].closed || contours[j].segs.empty()) {
        continue;
      }
      if (PointInsideContour(contours[i].start, contours[j])) {
        depths[i]++;
      }
    }
  }
  return depths;
}

static void AdjustWindingForEvenOdd(std::vector<PathContour>& contours,
                                    const std::vector<int>& depths) {
  int refSign = 0;
  for (size_t i = 0; i < contours.size(); i++) {
    if (depths[i] == 0 && contours[i].closed && !contours[i].segs.empty()) {
      refSign = (ComputeSignedArea(contours[i]) >= 0) ? 1 : -1;
      break;
    }
  }
  if (refSign == 0 && !contours.empty()) {
    refSign = (ComputeSignedArea(contours[0]) >= 0) ? 1 : -1;
  }
  for (size_t i = 0; i < contours.size(); i++) {
    if (!contours[i].closed || contours[i].segs.empty()) {
      continue;
    }
    float area = ComputeSignedArea(contours[i]);
    int currentSign = (area >= 0) ? 1 : -1;
    int targetSign = (depths[i] % 2 == 0) ? refSign : -refSign;
    if (currentSign != targetSign) {
      ReverseContour(contours[i]);
    }
  }
}

// Groups contours by their outermost (depth-0) ancestor so that each group
// can be emitted as a separate a:path element. This improves compatibility
// with PPT viewers that limit contour count per path or stop rendering after
// the first a:close.
static std::vector<std::vector<size_t>> GroupContoursByOutermost(
    const std::vector<PathContour>& contours, const std::vector<int>& depths) {
  std::vector<int> parent(contours.size(), -1);
  for (size_t i = 0; i < contours.size(); i++) {
    if (depths[i] == 0) {
      parent[i] = static_cast<int>(i);
    } else {
      for (size_t j = 0; j < contours.size(); j++) {
        if (depths[j] == 0 && j != i && contours[j].closed && !contours[j].segs.empty()) {
          if (PointInsideContour(contours[i].start, contours[j])) {
            parent[i] = static_cast<int>(j);
            break;
          }
        }
      }
      if (parent[i] < 0) {
        parent[i] = static_cast<int>(i);
      }
    }
  }

  std::vector<int> groupIndex(contours.size(), -1);
  std::vector<std::vector<size_t>> groups;
  for (size_t i = 0; i < contours.size(); i++) {
    int p = parent[i];
    if (groupIndex[p] < 0) {
      groupIndex[p] = static_cast<int>(groups.size());
      groups.push_back({i});
    } else {
      groups[groupIndex[p]].push_back(i);
    }
  }
  return groups;
}

// ── Custom geometry ────────────────────────────────────────────────────────

static void EmitPoint(XMLBuilder& out, const char* tag, float x, float y, float ofsX, float ofsY) {
  out.open(tag).gt();
  out.open("a:pt").a("x", PxToEMU(x - ofsX)).a("y", PxToEMU(y - ofsY)).sc();
  out.end();
}

static void EmitQuadBezTo(XMLBuilder& out, const char* tag, float x0, float y0, float x1, float y1,
                          float ofsX, float ofsY) {
  out.open(tag).gt();
  out.open("a:pt").a("x", PxToEMU(x0 - ofsX)).a("y", PxToEMU(y0 - ofsY)).sc();
  out.open("a:pt").a("x", PxToEMU(x1 - ofsX)).a("y", PxToEMU(y1 - ofsY)).sc();
  out.end();
}

static void EmitCubicBezTo(XMLBuilder& out, float x0, float y0, float x1, float y1, float x2,
                           float y2, float ofsX, float ofsY) {
  out.open("a:cubicBezTo").gt();
  out.open("a:pt").a("x", PxToEMU(x0 - ofsX)).a("y", PxToEMU(y0 - ofsY)).sc();
  out.open("a:pt").a("x", PxToEMU(x1 - ofsX)).a("y", PxToEMU(y1 - ofsY)).sc();
  out.open("a:pt").a("x", PxToEMU(x2 - ofsX)).a("y", PxToEMU(y2 - ofsY)).sc();
  out.end();
}

static void EmitContourSegments(XMLBuilder& out, const PathContour& c, float csX, float csY,
                                float sOfsX, float sOfsY) {
  for (const auto& s : c.segs) {
    switch (s.verb) {
      case PathVerb::Line:
        EmitPoint(out, "a:lnTo", s.pts[0].x * csX, s.pts[0].y * csY, sOfsX, sOfsY);
        break;
      case PathVerb::Quad:
        EmitQuadBezTo(out, "a:quadBezTo", s.pts[0].x * csX, s.pts[0].y * csY, s.pts[1].x * csX,
                      s.pts[1].y * csY, sOfsX, sOfsY);
        break;
      case PathVerb::Cubic:
        EmitCubicBezTo(out, s.pts[0].x * csX, s.pts[0].y * csY, s.pts[1].x * csX, s.pts[1].y * csY,
                       s.pts[2].x * csX, s.pts[2].y * csY, sOfsX, sOfsY);
        break;
      default:
        break;
    }
  }
}

static void EmitContour(XMLBuilder& out, const PathContour& c, float csX, float csY, float sOfsX,
                        float sOfsY) {
  EmitPoint(out, "a:moveTo", c.start.x * csX, c.start.y * csY, sOfsX, sOfsY);
  EmitContourSegments(out, c, csX, csY, sOfsX, sOfsY);
  if (c.closed) {
    out.open("a:close").sc();
  }
}

// Stitches an outer contour with its nested inner contours into a single contour
// using zero-width bridge lines. The bridge goes from the outer's start to each
// inner contour's start and returns along the same line, producing a self-overlapping
// edge pair that cancels in any scan-line rasterizer. The result is a single contour
// that correctly renders hollow regions without relying on multi-path XOR support.
static void EmitBridgedGroup(XMLBuilder& out, const std::vector<PathContour>& contours,
                             const std::vector<size_t>& group, float csX, float csY, float sOfsX,
                             float sOfsY) {
  const auto& outer = contours[group[0]];

  EmitPoint(out, "a:moveTo", outer.start.x * csX, outer.start.y * csY, sOfsX, sOfsY);
  EmitContourSegments(out, outer, csX, csY, sOfsX, sOfsY);
  if (outer.closed) {
    EmitPoint(out, "a:lnTo", outer.start.x * csX, outer.start.y * csY, sOfsX, sOfsY);
  }

  for (size_t i = 1; i < group.size(); i++) {
    const auto& inner = contours[group[i]];
    if (inner.segs.empty()) {
      continue;
    }
    // Bridge IN: outer.start → inner.start
    EmitPoint(out, "a:lnTo", inner.start.x * csX, inner.start.y * csY, sOfsX, sOfsY);
    EmitContourSegments(out, inner, csX, csY, sOfsX, sOfsY);
    if (inner.closed) {
      EmitPoint(out, "a:lnTo", inner.start.x * csX, inner.start.y * csY, sOfsX, sOfsY);
    }
    // Bridge OUT: inner.start → outer.start (same line reversed, cancels bridge IN)
    EmitPoint(out, "a:lnTo", outer.start.x * csX, outer.start.y * csY, sOfsX, sOfsY);
  }

  out.open("a:close").sc();
}

void PPTWriter::WriteContourGeom(XMLBuilder& out, std::vector<PathContour>& contours,
                                 int64_t pathWidth, int64_t pathHeight, float scaleX, float scaleY,
                                 float scaledOfsX, float scaledOfsY, FillRule fillRule) {
  out.open("a:custGeom").gt();
  out.open("a:avLst").sc();
  out.open("a:gdLst").sc();
  out.open("a:ahLst").sc();
  out.open("a:cxnLst").sc();
  out.open("a:rect").a("l", "0").a("t", "0").a("r", "r").a("b", "b").sc();

  out.open("a:pathLst").gt();

  if (contours.size() <= 1) {
    out.open("a:path").a("w", pathWidth).a("h", pathHeight).gt();
    for (auto& c : contours) {
      EmitContour(out, c, scaleX, scaleY, scaledOfsX, scaledOfsY);
    }
    out.end();  // a:path
  } else {
    auto depths = ComputeContainmentDepths(contours);
    if (fillRule == FillRule::EvenOdd) {
      AdjustWindingForEvenOdd(contours, depths);
    }
    auto groups = GroupContoursByOutermost(contours, depths);
    for (const auto& group : groups) {
      out.open("a:path").a("w", pathWidth).a("h", pathHeight).gt();
      if (group.size() > 1) {
        EmitBridgedGroup(out, contours, group, scaleX, scaleY, scaledOfsX, scaledOfsY);
      } else {
        EmitContour(out, contours[group[0]], scaleX, scaleY, scaledOfsX, scaledOfsY);
      }
      out.end();  // a:path
    }
  }

  out.end();  // a:pathLst
  out.end();  // a:custGeom
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
    out.open("a:prstGeom").a("prst", "roundRect").gt();
    out.open("a:avLst").gt();
    out.open("a:gd").a("name", "adj").a("fmla", "val " + std::to_string(adj)).sc();
    out.end();  // a:avLst
    out.end();  // a:prstGeom
  } else {
    out.open("a:prstGeom").a("prst", "rect").gt();
    out.open("a:avLst").sc();
    out.end();
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

  out.open("a:prstGeom").a("prst", "ellipse").gt();
  out.open("a:avLst").sc();
  out.end();

  writeShapeTail(out, fs, alpha, shapeBounds, imageWritten, filters);
}

void PPTWriter::writePath(XMLBuilder& out, const Path* path, const FillStrokeInfo& fs,
                          const Matrix& m, float alpha, const std::vector<LayerFilter*>& filters) {
  if (!path->data || path->data->isEmpty()) {
    return;
  }
  Rect bounds = const_cast<PathData*>(path->data)->getBounds();
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
  beginShape(out, "Path", xf.offX, xf.offY, xf.extCX, xf.extCY, xf.rotation);

  FillRule fillRule = (fs.fill) ? fs.fill->fillRule : FillRule::Winding;
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
    const auto& t = gp.transform;
    auto txPt = [&](const Point& p) -> Point {
      return {t.a * p.x + t.c * p.y + t.tx, t.b * p.x + t.d * p.y + t.ty};
    };
    gp.pathData->forEach([&](PathVerb verb, const Point* pts) {
      if (verb == PathVerb::Move) {
        PathContour c;
        c.start = txPt(pts[0]);
        allContours.push_back(std::move(c));
      } else if (!allContours.empty()) {
        if (verb == PathVerb::Close) {
          allContours.back().closed = true;
        } else {
          PathSeg seg;
          seg.verb = verb;
          int ptCount = PathData::PointsPerVerb(verb);
          for (int i = 0; i < ptCount; i++) {
            seg.pts[i] = txPt(pts[i]);
          }
          allContours.back().segs.push_back(seg);
        }
      }
    });
  }

  if (allContours.empty()) {
    return;
  }

  float minX = std::numeric_limits<float>::max();
  float minY = std::numeric_limits<float>::max();
  float maxX = std::numeric_limits<float>::lowest();
  float maxY = std::numeric_limits<float>::lowest();
  for (const auto& contour : allContours) {
    auto updateBounds = [&](const Point& p) {
      minX = std::min(minX, p.x);
      minY = std::min(minY, p.y);
      maxX = std::max(maxX, p.x);
      maxY = std::max(maxY, p.y);
    };
    updateBounds(contour.start);
    for (const auto& seg : contour.segs) {
      int n = PathData::PointsPerVerb(seg.verb);
      for (int i = 0; i < n; i++) {
        updateBounds(seg.pts[i]);
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
  beginShape(out, "Glyph", xf.offX, xf.offY, xf.extCX, xf.extCY, xf.rotation);

  int64_t pw = std::max(int64_t(1), PxToEMU(boundsW * sx));
  int64_t ph = std::max(int64_t(1), PxToEMU(boundsH * sy));
  WriteContourGeom(out, allContours, pw, ph, sx, sy, minX * sx, minY * sy, FillRule::EvenOdd);

  writeFill(out, fs.fill, alpha);
  out.open("a:ln").gt();
  out.open("a:noFill").sc();
  out.end();
  endShape(out);
}

void PPTWriter::writeNativeText(XMLBuilder& out, const Text* text, const FillStrokeInfo& fs,
                                const Matrix& m, float alpha,
                                const std::vector<LayerFilter*>& filters) {
  if (text->text.empty()) {
    return;
  }

  float estWidth = static_cast<float>(text->text.size()) * text->fontSize * 0.6f;
  float estHeight = text->fontSize * 1.4f;
  float posX = text->position.x;
  float posY = text->position.y - text->fontSize * 0.85f;
  bool hasTextBox = false;

  if (fs.textBox && fs.textBox->size.width > 0) {
    hasTextBox = true;
    posX = fs.textBox->position.x;
    posY = fs.textBox->position.y;
    estWidth = fs.textBox->size.width;
    estHeight = (fs.textBox->size.height > 0) ? fs.textBox->size.height : text->fontSize * 1.4f;
  } else {
    if (text->textAnchor == TextAnchor::Center) {
      posX -= estWidth / 2.0f;
    } else if (text->textAnchor == TextAnchor::End) {
      posX -= estWidth;
    }
  }

  auto xf = decomposeXform(posX, posY, estWidth, estHeight, m);

  int id = _ctx->nextShapeId();
  out.open("p:sp").gt();
  out.open("p:nvSpPr").gt();
  out.open("p:cNvPr").a("id", id).a("name", "TextBox").sc();
  out.open("p:cNvSpPr").a("txBox", "1").sc();
  out.open("p:nvPr").sc();
  out.end();  // p:nvSpPr

  out.open("p:spPr").gt();
  WriteXfrm(out, xf);
  out.open("a:prstGeom").a("prst", "rect").gt();
  out.open("a:avLst").sc();
  out.end();
  out.open("a:noFill").sc();
  out.open("a:ln").gt();
  out.open("a:noFill").sc();
  out.end();
  writeEffects(out, filters);
  out.end();  // p:spPr

  out.open("p:txBody").gt();
  auto& bodyPr = out.open("a:bodyPr")
                     .a("wrap", hasTextBox ? "square" : "none")
                     .a("lIns", "0")
                     .a("tIns", "0")
                     .a("rIns", "0")
                     .a("bIns", "0");
  if (fs.textBox) {
    if (fs.textBox->paragraphAlign == ParagraphAlign::Middle) {
      bodyPr.a("anchor", "ctr");
    } else if (fs.textBox->paragraphAlign == ParagraphAlign::Far) {
      bodyPr.a("anchor", "b");
    }
  }
  bodyPr.sc();
  out.open("a:lstStyle").sc();

  // Split text by newlines into paragraphs
  std::string remaining = text->text;
  size_t pos = 0;
  while (pos <= remaining.size()) {
    size_t nl = remaining.find('\n', pos);
    std::string line =
        (nl == std::string::npos) ? remaining.substr(pos) : remaining.substr(pos, nl - pos);
    out.open("a:p").gt();

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
      out.open("a:pPr").a("algn", algn).sc();
    }

    out.open("a:r").gt();
    out.open("a:rPr").a("lang", "en-US").a("sz", FontSizeToPPT(text->fontSize));
    bool hasBold = text->fontStyle.find("Bold") != std::string::npos;
    bool hasItalic = text->fontStyle.find("Italic") != std::string::npos;
    if (hasBold) {
      out.a("b", "1");
    }
    if (hasItalic) {
      out.a("i", "1");
    }
    if (text->letterSpacing != 0.0f) {
      out.a("spc", static_cast<int64_t>(std::round(text->letterSpacing * 75.0)));
    }
    out.gt();

    if (fs.fill && fs.fill->color && fs.fill->color->nodeType() == NodeType::SolidColor) {
      auto* solid = static_cast<const SolidColor*>(fs.fill->color);
      float ea = solid->color.alpha * fs.fill->alpha * alpha;
      out.open("a:solidFill").gt();
      out.open("a:srgbClr").a("val", ColorToHex6(solid->color));
      if (ea < 1.0f) {
        out.gt();
        out.open("a:alpha").a("val", AlphaToPct(ea)).sc();
        out.end();
      } else {
        out.sc();
      }
      out.end();  // a:solidFill
    }

    if (!text->fontFamily.empty()) {
      auto typeface = StripQuotes(text->fontFamily);
      out.open("a:latin").a("typeface", typeface).sc();
      out.open("a:ea").a("typeface", typeface).sc();
    }

    out.end();  // a:rPr
    out.open("a:t").gt();
    out.text(line);
    out.end();  // a:t
    out.end();  // a:r
    out.end();  // a:p

    if (nl == std::string::npos) {
      break;
    }
    pos = nl + 1;
  }

  out.end();  // p:txBody
  out.end();  // p:sp
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
// Boilerplate PPTX XML generators
//==============================================================================

static std::string GenerateContentTypes(const PPTWriterContext& ctx) {
  std::string s;
  s.reserve(2048);
  s += "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>";
  s += "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">";
  s += "<Default Extension=\"xml\" ContentType=\"application/xml\"/>";
  s += "<Default Extension=\"rels\" "
       "ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>";
  if (ctx.hasPNG()) {
    s += "<Default Extension=\"png\" ContentType=\"image/png\"/>";
  }
  if (ctx.hasJPEG()) {
    s += "<Default Extension=\"jpeg\" ContentType=\"image/jpeg\"/>";
  }
  s += "<Override PartName=\"/ppt/presentation.xml\" "
       "ContentType=\"application/vnd.openxmlformats-officedocument.presentationml.presentation."
       "main+xml\"/>";
  s += "<Override PartName=\"/ppt/slides/slide1.xml\" "
       "ContentType=\"application/vnd.openxmlformats-officedocument.presentationml.slide+xml\"/>";
  s += "<Override PartName=\"/ppt/slideMasters/slideMaster1.xml\" "
       "ContentType=\"application/vnd.openxmlformats-officedocument.presentationml.slideMaster+"
       "xml\"/>";
  s += "<Override PartName=\"/ppt/presProps.xml\" "
       "ContentType=\"application/vnd.openxmlformats-officedocument.presentationml.presProps+xml\"/"
       ">";
  s += "<Override PartName=\"/ppt/viewProps.xml\" "
       "ContentType=\"application/vnd.openxmlformats-officedocument.presentationml.viewProps+xml\"/"
       ">";
  s += "<Override PartName=\"/ppt/theme/theme1.xml\" "
       "ContentType=\"application/vnd.openxmlformats-officedocument.theme+xml\"/>";
  s += "<Override PartName=\"/ppt/tableStyles.xml\" "
       "ContentType=\"application/vnd.openxmlformats-officedocument.presentationml.tableStyles+"
       "xml\"/>";
  s += "<Override PartName=\"/ppt/slideLayouts/slideLayout1.xml\" "
       "ContentType=\"application/vnd.openxmlformats-officedocument.presentationml.slideLayout+"
       "xml\"/>";
  s += "<Override PartName=\"/docProps/core.xml\" "
       "ContentType=\"application/vnd.openxmlformats-package.core-properties+xml\"/>";
  s += "<Override PartName=\"/docProps/app.xml\" "
       "ContentType=\"application/vnd.openxmlformats-officedocument.extended-properties+xml\"/>";
  s += "</Types>";
  return s;
}

static std::string GenerateRootRels() {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
         "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
         "<Relationship Id=\"rId1\" "
         "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/"
         "officeDocument\" Target=\"ppt/presentation.xml\"/>"
         "<Relationship Id=\"rId2\" "
         "Type=\"http://schemas.openxmlformats.org/package/2006/relationships/metadata/"
         "core-properties\" Target=\"docProps/core.xml\"/>"
         "<Relationship Id=\"rId3\" "
         "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/"
         "extended-properties\" Target=\"docProps/app.xml\"/>"
         "</Relationships>";
}

// OOXML ST_SlideSizeCoordinate range: 914400 EMU (1") to 51206400 EMU (56").
static constexpr int64_t kMaxSlideSizeEMU = 51206400;
// Standard 16:9 slide dimensions.
static constexpr int64_t kStdWidth16x9 = 12192000;  // 10"
static constexpr int64_t kStdHeight16x9 = 6858000;  // 7.5"

static std::string GeneratePresentation(float w, float h) {
  int64_t rawCX = PxToEMU(w);
  int64_t rawCY = PxToEMU(h);
  bool outOfRange = rawCX > kMaxSlideSizeEMU || rawCY > kMaxSlideSizeEMU;
  int64_t cx = outOfRange ? kStdWidth16x9 : rawCX;
  int64_t cy = outOfRange ? kStdHeight16x9 : rawCY;
  std::string s;
  s.reserve(512);
  s += "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>";
  s += "<p:presentation xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
       "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" "
       "xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\">";
  s += "<p:sldMasterIdLst><p:sldMasterId id=\"2147483648\" r:id=\"rId1\"/></p:sldMasterIdLst>";
  s += "<p:sldIdLst><p:sldId id=\"256\" r:id=\"rId2\"/></p:sldIdLst>";
  if (outOfRange) {
    s += "<p:sldSz cx=\"" + I64(cx) + "\" cy=\"" + I64(cy) + "\"/>";
  } else {
    s += "<p:sldSz cx=\"" + I64(cx) + "\" cy=\"" + I64(cy) + "\" type=\"custom\"/>";
  }
  s += "<p:notesSz cx=\"" + I64(cx) + "\" cy=\"" + I64(cy) + "\"/>";
  s += "<p:defaultTextStyle>"
       "<a:defPPr><a:defRPr lang=\"zh-CN\"/></a:defPPr>";
  for (int lvl = 1; lvl <= 9; lvl++) {
    int marL = (lvl - 1) * 457200;
    s += "<a:lvl" + std::to_string(lvl) + "pPr marL=\"" + std::to_string(marL) +
         "\" algn=\"l\" defTabSz=\"914400\" rtl=\"0\" eaLnBrk=\"1\" "
         "latinLnBrk=\"0\" hangingPunct=\"1\">"
         "<a:defRPr sz=\"1800\" kern=\"1200\">"
         "<a:solidFill><a:schemeClr val=\"tx1\"/></a:solidFill>"
         "<a:latin typeface=\"+mn-lt\"/><a:ea typeface=\"+mn-ea\"/>"
         "<a:cs typeface=\"+mn-cs\"/>"
         "</a:defRPr></a:lvl" +
         std::to_string(lvl) + "pPr>";
  }
  s += "</p:defaultTextStyle>";
  s += "</p:presentation>";
  return s;
}

static std::string GeneratePresentationRels() {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
         "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
         "<Relationship Id=\"rId1\" "
         "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideMaster\" "
         "Target=\"slideMasters/slideMaster1.xml\"/>"
         "<Relationship Id=\"rId2\" "
         "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide\" "
         "Target=\"slides/slide1.xml\"/>"
         "<Relationship Id=\"rId3\" "
         "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/presProps\" "
         "Target=\"presProps.xml\"/>"
         "<Relationship Id=\"rId4\" "
         "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/viewProps\" "
         "Target=\"viewProps.xml\"/>"
         "<Relationship Id=\"rId5\" "
         "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/theme\" "
         "Target=\"theme/theme1.xml\"/>"
         "<Relationship Id=\"rId6\" "
         "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/tableStyles\" "
         "Target=\"tableStyles.xml\"/>"
         "</Relationships>";
}

static std::string GenerateSlideRels(const PPTWriterContext& ctx) {
  std::string s;
  s.reserve(512);
  s += "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>";
  s += "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">";
  s += "<Relationship Id=\"rId1\" "
       "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideLayout\" "
       "Target=\"../slideLayouts/slideLayout1.xml\"/>";
  for (const auto& img : ctx.images()) {
    s += "<Relationship Id=\"" + img.relId +
         "\" "
         "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/image\" "
         "Target=\"../" +
         img.mediaPath.substr(4) + "\"/>";
  }
  s += "</Relationships>";
  return s;
}

static std::string GenerateSlideMaster() {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
         "<p:sldMaster xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
         "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" "
         "xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\">"
         "<p:cSld><p:bg><p:bgPr>"
         "<a:solidFill><a:schemeClr val=\"bg1\"/></a:solidFill>"
         "<a:effectLst/></p:bgPr></p:bg>"
         "<p:spTree><p:nvGrpSpPr><p:cNvPr id=\"1\" name=\"\"/>"
         "<p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr>"
         "<p:grpSpPr><a:xfrm><a:off x=\"0\" y=\"0\"/><a:ext cx=\"0\" cy=\"0\"/>"
         "<a:chOff x=\"0\" y=\"0\"/><a:chExt cx=\"0\" cy=\"0\"/></a:xfrm></p:grpSpPr>"
         "</p:spTree>"
         "</p:cSld>"
         "<p:clrMap bg1=\"lt1\" tx1=\"dk1\" bg2=\"lt2\" tx2=\"dk2\" "
         "accent1=\"accent1\" accent2=\"accent2\" accent3=\"accent3\" "
         "accent4=\"accent4\" accent5=\"accent5\" accent6=\"accent6\" "
         "hlink=\"hlink\" folHlink=\"folHlink\"/>"
         "<p:sldLayoutIdLst>"
         "<p:sldLayoutId id=\"2147483649\" r:id=\"rId1\"/>"
         "</p:sldLayoutIdLst>"
         "<p:txStyles>"
         "<p:titleStyle>"
         "<a:lvl1pPr algn=\"l\" defTabSz=\"914400\" rtl=\"0\" eaLnBrk=\"1\" "
         "latinLnBrk=\"0\" hangingPunct=\"1\">"
         "<a:lnSpc><a:spcPct val=\"90000\"/></a:lnSpc>"
         "<a:spcBef><a:spcPct val=\"0\"/></a:spcBef>"
         "<a:buNone/>"
         "<a:defRPr sz=\"4400\" kern=\"1200\">"
         "<a:solidFill><a:schemeClr val=\"tx1\"/></a:solidFill>"
         "<a:latin typeface=\"+mj-lt\"/><a:ea typeface=\"+mj-ea\"/>"
         "<a:cs typeface=\"+mj-cs\"/>"
         "</a:defRPr></a:lvl1pPr>"
         "</p:titleStyle>"
         "<p:bodyStyle>"
         "<a:lvl1pPr marL=\"228600\" indent=\"-228600\" algn=\"l\" defTabSz=\"914400\" "
         "rtl=\"0\" eaLnBrk=\"1\" latinLnBrk=\"0\" hangingPunct=\"1\">"
         "<a:lnSpc><a:spcPct val=\"90000\"/></a:lnSpc>"
         "<a:spcBef><a:spcPts val=\"1000\"/></a:spcBef>"
         "<a:buFont typeface=\"Arial\" panose=\"020B0604020202020204\" pitchFamily=\"34\" "
         "charset=\"0\"/>"
         "<a:buChar char=\"&#x2022;\"/>"
         "<a:defRPr sz=\"2800\" kern=\"1200\">"
         "<a:solidFill><a:schemeClr val=\"tx1\"/></a:solidFill>"
         "<a:latin typeface=\"+mn-lt\"/><a:ea typeface=\"+mn-ea\"/>"
         "<a:cs typeface=\"+mn-cs\"/>"
         "</a:defRPr></a:lvl1pPr>"
         "</p:bodyStyle>"
         "<p:otherStyle>"
         "<a:defPPr>"
         "<a:defRPr lang=\"zh-CN\"/>"
         "</a:defPPr>"
         "</p:otherStyle>"
         "</p:txStyles>"
         "</p:sldMaster>";
}

static std::string GenerateSlideMasterRels() {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
         "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
         "<Relationship Id=\"rId1\" "
         "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideLayout\" "
         "Target=\"../slideLayouts/slideLayout1.xml\"/>"
         "<Relationship Id=\"rId2\" "
         "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/theme\" "
         "Target=\"../theme/theme1.xml\"/>"
         "</Relationships>";
}

static std::string GenerateSlideLayout() {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
         "<p:sldLayout xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
         "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" "
         "xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\" "
         "type=\"blank\" preserve=\"1\">"
         "<p:cSld name=\"Blank\"><p:spTree>"
         "<p:nvGrpSpPr><p:cNvPr id=\"1\" name=\"\"/>"
         "<p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr>"
         "<p:grpSpPr><a:xfrm><a:off x=\"0\" y=\"0\"/><a:ext cx=\"0\" cy=\"0\"/>"
         "<a:chOff x=\"0\" y=\"0\"/><a:chExt cx=\"0\" cy=\"0\"/></a:xfrm></p:grpSpPr>"
         "</p:spTree></p:cSld>"
         "<p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr>"
         "</p:sldLayout>";
}

static std::string GenerateSlideLayoutRels() {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
         "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
         "<Relationship Id=\"rId1\" "
         "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideMaster\" "
         "Target=\"../slideMasters/slideMaster1.xml\"/>"
         "</Relationships>";
}

static std::string GenerateTheme() {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
         "<a:theme xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" name=\"PAGX\">"
         "<a:themeElements>"
         "<a:clrScheme name=\"PAGX\">"
         "<a:dk1><a:srgbClr val=\"000000\"/></a:dk1>"
         "<a:lt1><a:srgbClr val=\"FFFFFF\"/></a:lt1>"
         "<a:dk2><a:srgbClr val=\"44546A\"/></a:dk2>"
         "<a:lt2><a:srgbClr val=\"E7E6E6\"/></a:lt2>"
         "<a:accent1><a:srgbClr val=\"4472C4\"/></a:accent1>"
         "<a:accent2><a:srgbClr val=\"ED7D31\"/></a:accent2>"
         "<a:accent3><a:srgbClr val=\"A5A5A5\"/></a:accent3>"
         "<a:accent4><a:srgbClr val=\"FFC000\"/></a:accent4>"
         "<a:accent5><a:srgbClr val=\"5B9BD5\"/></a:accent5>"
         "<a:accent6><a:srgbClr val=\"70AD47\"/></a:accent6>"
         "<a:hlink><a:srgbClr val=\"0563C1\"/></a:hlink>"
         "<a:folHlink><a:srgbClr val=\"954F72\"/></a:folHlink>"
         "</a:clrScheme>"
         "<a:fontScheme name=\"PAGX\">"
         "<a:majorFont><a:latin typeface=\"Calibri\"/><a:ea typeface=\"\"/>"
         "<a:cs typeface=\"\"/></a:majorFont>"
         "<a:minorFont><a:latin typeface=\"Calibri\"/><a:ea typeface=\"\"/>"
         "<a:cs typeface=\"\"/></a:minorFont>"
         "</a:fontScheme>"
         "<a:fmtScheme name=\"PAGX\">"
         "<a:fillStyleLst><a:solidFill><a:schemeClr val=\"phClr\"/></a:solidFill>"
         "<a:solidFill><a:schemeClr val=\"phClr\"/></a:solidFill>"
         "<a:solidFill><a:schemeClr val=\"phClr\"/></a:solidFill></a:fillStyleLst>"
         "<a:lnStyleLst><a:ln w=\"6350\"><a:solidFill><a:schemeClr val=\"phClr\"/>"
         "</a:solidFill></a:ln><a:ln w=\"12700\"><a:solidFill><a:schemeClr val=\"phClr\"/>"
         "</a:solidFill></a:ln><a:ln w=\"19050\"><a:solidFill><a:schemeClr val=\"phClr\"/>"
         "</a:solidFill></a:ln></a:lnStyleLst>"
         "<a:effectStyleLst><a:effectStyle><a:effectLst/></a:effectStyle>"
         "<a:effectStyle><a:effectLst/></a:effectStyle>"
         "<a:effectStyle><a:effectLst/></a:effectStyle></a:effectStyleLst>"
         "<a:bgFillStyleLst><a:solidFill><a:schemeClr val=\"phClr\"/></a:solidFill>"
         "<a:solidFill><a:schemeClr val=\"phClr\"/></a:solidFill>"
         "<a:solidFill><a:schemeClr val=\"phClr\"/></a:solidFill></a:bgFillStyleLst>"
         "</a:fmtScheme>"
         "</a:themeElements>"
         "<a:objectDefaults/>"
         "<a:extraClrSchemeLst/>"
         "</a:theme>";
}

static std::string GeneratePresProps() {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
         "<p:presentationPr "
         "xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
         "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" "
         "xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\"/>";
}

static std::string GenerateViewProps() {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
         "<p:viewPr "
         "xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
         "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" "
         "xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\">"
         "<p:normalViewPr><p:restoredLeft sz=\"15611\"/>"
         "<p:restoredTop sz=\"94658\"/></p:normalViewPr>"
         "<p:slideViewPr><p:cSldViewPr snapToGrid=\"0\">"
         "<p:cViewPr varScale=\"1\">"
         "<p:scale><a:sx n=\"100\" d=\"100\"/><a:sy n=\"100\" d=\"100\"/></p:scale>"
         "<p:origin x=\"0\" y=\"0\"/></p:cViewPr>"
         "<p:guideLst/></p:cSldViewPr></p:slideViewPr>"
         "<p:notesTextViewPr><p:cViewPr>"
         "<p:scale><a:sx n=\"1\" d=\"1\"/><a:sy n=\"1\" d=\"1\"/></p:scale>"
         "<p:origin x=\"0\" y=\"0\"/></p:cViewPr></p:notesTextViewPr>"
         "<p:gridSpacing cx=\"72008\" cy=\"72008\"/>"
         "</p:viewPr>";
}

static std::string GenerateTableStyles() {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
         "<a:tblStyleLst xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
         "def=\"{5C22544A-7EE6-4342-B048-85BDC9FD1C3A}\"/>";
}

static std::string GenerateCoreProps() {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
         "<cp:coreProperties "
         "xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\" "
         "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
         "xmlns:dcterms=\"http://purl.org/dc/terms/\" "
         "xmlns:dcmitype=\"http://purl.org/dc/dcmitype/\" "
         "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">"
         "<cp:revision>1</cp:revision>"
         "</cp:coreProperties>";
}

static std::string GenerateAppProps() {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
         "<Properties "
         "xmlns=\"http://schemas.openxmlformats.org/officeDocument/2006/extended-properties\" "
         "xmlns:vt=\"http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes\">"
         "<TotalTime>0</TotalTime>"
         "<Words>0</Words>"
         "<Application>PAGX</Application>"
         "<Paragraphs>0</Paragraphs>"
         "<Slides>1</Slides>"
         "<Notes>0</Notes>"
         "<HiddenSlides>0</HiddenSlides>"
         "<MMClips>0</MMClips>"
         "<ScaleCrop>false</ScaleCrop>"
         "<LinksUpToDate>false</LinksUpToDate>"
         "<SharedDoc>false</SharedDoc>"
         "<HyperlinksChanged>false</HyperlinksChanged>"
         "</Properties>";
}

static void AddZipEntry(zipFile zf, const char* name, const void* data, unsigned size) {
  zip_fileinfo zi = {};
  if (zipOpenNewFileInZip(zf, name, &zi, nullptr, 0, nullptr, 0, nullptr, Z_DEFLATED,
                          Z_DEFAULT_COMPRESSION) == ZIP_OK) {
    zipWriteInFileInZip(zf, data, size);
    zipCloseFileInZip(zf);
  }
}

static void AddZipString(zipFile zf, const char* name, const std::string& content) {
  AddZipEntry(zf, name, content.data(), static_cast<unsigned>(content.size()));
}

//==============================================================================
// PPTExporter::ToFile
//==============================================================================

bool PPTExporter::ToFile(const PAGXDocument& doc, const std::string& filePath,
                         const Options& options) {
  PPTWriterContext context;
  PPTWriter writer(&context, &doc, options.convertTextToPath, options.bakeMask);

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
