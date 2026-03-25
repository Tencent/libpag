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

#include "pagx/PDFExporter.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/ColorStop.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
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
#include "tgfx/core/Data.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/ImageInfo.h"
#include "tgfx/core/Pixmap.h"

namespace pagx {

// 4*(sqrt(2)-1)/3, optimal cubic Bezier approximation for quarter-circle arcs.
static constexpr float kKappa = 0.5522847498f;

// PDF does not support scientific notation (e.g., "1e-06") in numeric values.
// The spec requires decimal digits with optional sign and decimal point only.
static std::string PDFFloat(float value) {
  if (std::abs(value) < 0.00001f) {
    return "0";
  }
  char buf[64];
  snprintf(buf, sizeof(buf), "%.6f", value);
  auto len = strlen(buf);
  if (memchr(buf, '.', len)) {
    while (len > 0 && buf[len - 1] == '0') {
      buf[--len] = '\0';
    }
    if (len > 0 && buf[len - 1] == '.') {
      buf[--len] = '\0';
    }
  }
  return std::string(buf);
}

static const char* BlendModeToPDFName(BlendMode mode) {
  switch (mode) {
    case BlendMode::Normal:
      return "Normal";
    case BlendMode::Multiply:
      return "Multiply";
    case BlendMode::Screen:
      return "Screen";
    case BlendMode::Overlay:
      return "Overlay";
    case BlendMode::Darken:
      return "Darken";
    case BlendMode::Lighten:
      return "Lighten";
    case BlendMode::ColorDodge:
      return "ColorDodge";
    case BlendMode::ColorBurn:
      return "ColorBurn";
    case BlendMode::HardLight:
      return "HardLight";
    case BlendMode::SoftLight:
      return "SoftLight";
    case BlendMode::Difference:
      return "Difference";
    case BlendMode::Exclusion:
      return "Exclusion";
    case BlendMode::Hue:
      return "Hue";
    case BlendMode::Saturation:
      return "Saturation";
    case BlendMode::Color:
      return "Color";
    case BlendMode::Luminosity:
      return "Luminosity";
    default:
      return nullptr;
  }
}

//==============================================================================
// PDFStream – builds PDF content stream operators
//==============================================================================

class PDFStream {
 public:
  void append(const std::string& s) {
    _buf += s;
  }
  void append(const char* s) {
    _buf += s;
  }
  void append(float v) {
    _buf += PDFFloat(v);
  }
  void space() {
    _buf += ' ';
  }
  void newline() {
    _buf += '\n';
  }

  // Graphics state
  void save() {
    _buf += "q\n";
  }
  void restore() {
    _buf += "Q\n";
  }

  void concatMatrix(const Matrix& m) {
    append(m.a);
    space();
    append(m.b);
    space();
    append(m.c);
    space();
    append(m.d);
    space();
    append(m.tx);
    space();
    append(m.ty);
    _buf += " cm\n";
  }

  // Path construction
  void moveTo(float x, float y) {
    append(x);
    space();
    append(y);
    _buf += " m\n";
  }
  void lineTo(float x, float y) {
    append(x);
    space();
    append(y);
    _buf += " l\n";
  }
  void curveTo(float c1x, float c1y, float c2x, float c2y, float x, float y) {
    append(c1x);
    space();
    append(c1y);
    space();
    append(c2x);
    space();
    append(c2y);
    space();
    append(x);
    space();
    append(y);
    _buf += " c\n";
  }
  void closePath() {
    _buf += "h\n";
  }
  void rect(float x, float y, float w, float h) {
    append(x);
    space();
    append(y);
    space();
    append(w);
    space();
    append(h);
    _buf += " re\n";
  }

  // Path painting
  void fill() {
    _buf += "f\n";
  }
  void fillEvenOdd() {
    _buf += "f*\n";
  }
  void stroke() {
    _buf += "S\n";
  }
  void fillAndStroke() {
    _buf += "B\n";
  }
  void fillEvenOddAndStroke() {
    _buf += "B*\n";
  }
  void endPath() {
    _buf += "n\n";
  }

  // Clipping
  void clip() {
    _buf += "W\n";
  }
  void clipEvenOdd() {
    _buf += "W*\n";
  }

  // Color
  void setFillRGB(float r, float g, float b) {
    append(r);
    space();
    append(g);
    space();
    append(b);
    _buf += " rg\n";
  }
  void setStrokeRGB(float r, float g, float b) {
    append(r);
    space();
    append(g);
    space();
    append(b);
    _buf += " RG\n";
  }

  // Pattern color
  void setFillPattern(const std::string& name) {
    _buf += "/Pattern cs /" + name + " scn\n";
  }
  void setStrokePattern(const std::string& name) {
    _buf += "/Pattern CS /" + name + " SCN\n";
  }

  // Shading (direct paint)
  void paintShading(const std::string& name) {
    _buf += "/" + name + " sh\n";
  }

  // ExtGState
  void setExtGState(const std::string& name) {
    _buf += "/" + name + " gs\n";
  }

  // Stroke attributes
  void setLineWidth(float w) {
    append(w);
    _buf += " w\n";
  }
  void setLineCap(int cap) {
    _buf += std::to_string(cap) + " J\n";
  }
  void setLineJoin(int join) {
    _buf += std::to_string(join) + " j\n";
  }
  void setMiterLimit(float limit) {
    append(limit);
    _buf += " M\n";
  }
  void setDash(const std::vector<float>& dashes, float offset) {
    _buf += '[';
    for (size_t i = 0; i < dashes.size(); i++) {
      if (i > 0) {
        space();
      }
      append(dashes[i]);
    }
    _buf += "] ";
    append(offset);
    _buf += " d\n";
  }

  // Text
  void beginText() {
    _buf += "BT\n";
  }
  void endText() {
    _buf += "ET\n";
  }
  void setFont(const std::string& name, float size) {
    _buf += "/" + name + " ";
    append(size);
    _buf += " Tf\n";
  }
  void setTextMatrix(float a, float b, float c, float d, float e, float f) {
    append(a);
    space();
    append(b);
    space();
    append(c);
    space();
    append(d);
    space();
    append(e);
    space();
    append(f);
    _buf += " Tm\n";
  }
  void showText(const std::string& escaped) {
    _buf += "(" + escaped + ") Tj\n";
  }

  const std::string& str() const {
    return _buf;
  }
  std::string release() {
    return std::move(_buf);
  }
  bool empty() const {
    return _buf.empty();
  }

 private:
  std::string _buf = {};
};

//==============================================================================
// PDFObjectStore – manages numbered indirect objects
//==============================================================================

class PDFObjectStore {
 public:
  int reserve() {
    return _nextId++;
  }

  void set(int id, const std::string& content) {
    if (id >= static_cast<int>(_objects.size())) {
      _objects.resize(static_cast<size_t>(id) + 1);
    }
    _objects[static_cast<size_t>(id)] = content;
  }

  int add(const std::string& content) {
    int id = reserve();
    set(id, content);
    return id;
  }

  std::string serialize(int rootId) const {
    std::string result;
    result.reserve(64 * 1024);

    result += "%PDF-1.4\n";
    // Binary comment to signal this is a binary file
    result += "%\xE2\xE3\xCF\xD3\n";

    std::vector<size_t> offsets(static_cast<size_t>(_nextId), 0);
    for (int i = 1; i < _nextId; i++) {
      auto idx = static_cast<size_t>(i);
      offsets[idx] = result.size();
      result += std::to_string(i) + " 0 obj\n";
      if (idx < _objects.size()) {
        result += _objects[idx];
      }
      result += "\nendobj\n\n";
    }

    size_t xrefOffset = result.size();
    result += "xref\n";
    result += "0 " + std::to_string(_nextId) + "\n";
    // Object 0: head of free list
    result += "0000000000 65535 f \n";
    for (int i = 1; i < _nextId; i++) {
      char buf[22];
      snprintf(buf, sizeof(buf), "%010lu 00000 n \n",
               static_cast<unsigned long>(offsets[static_cast<size_t>(i)]));
      result += buf;
    }

    result += "trailer\n<< /Size " + std::to_string(_nextId) + " /Root " + std::to_string(rootId) +
              " 0 R >>\nstartxref\n" + std::to_string(xrefOffset) + "\n%%EOF\n";

    return result;
  }

 private:
  int _nextId = 1;
  std::vector<std::string> _objects = {};
};

//==============================================================================
// Image data helpers
//==============================================================================

struct PDFImageData {
  std::string rgbBytes;
  std::string alphaBytes;
  int width = 0;
  int height = 0;
};

static PDFImageData GetPDFImageData(const std::shared_ptr<tgfx::Data>& imageData) {
  PDFImageData result;
  auto codec = tgfx::ImageCodec::MakeFrom(imageData);
  if (codec == nullptr) {
    return {};
  }
  result.width = codec->width();
  result.height = codec->height();
  if (IsJPEG(imageData->bytes(), imageData->size())) {
    result.rgbBytes = {reinterpret_cast<const char*>(imageData->bytes()), imageData->size()};
    return result;
  }
  auto info = tgfx::ImageInfo::Make(result.width, result.height, tgfx::ColorType::RGBA_8888,
                                    tgfx::AlphaType::Unpremultiplied);
  std::vector<uint8_t> pixels(info.byteSize());
  if (!codec->readPixels(info, pixels.data())) {
    return {};
  }

  auto pixelCount = static_cast<size_t>(result.width) * static_cast<size_t>(result.height);
  bool hasTransparency = false;
  for (size_t i = 0; i < pixelCount; i++) {
    if (pixels[i * 4 + 3] != 255) {
      hasTransparency = true;
      break;
    }
  }

  tgfx::Pixmap pixmap(info, pixels.data());
  auto jpegData = tgfx::ImageCodec::Encode(pixmap, tgfx::EncodedFormat::JPEG, 95);
  if (jpegData == nullptr) {
    return {};
  }
  result.rgbBytes = {reinterpret_cast<const char*>(jpegData->bytes()), jpegData->size()};

  if (hasTransparency) {
    result.alphaBytes.resize(pixelCount);
    for (size_t i = 0; i < pixelCount; i++) {
      result.alphaBytes[i] = static_cast<char>(pixels[i * 4 + 3]);
    }
  }

  return result;
}

//==============================================================================
// PDFResourceManager – tracks ExtGState, Shading, Pattern, Font resources
//==============================================================================

class PDFResourceManager {
 public:
  explicit PDFResourceManager(PDFObjectStore* store) : _store(store) {
  }

  std::string getExtGState(float fillAlpha, float strokeAlpha) {
    // Quantize to avoid creating duplicate objects for near-identical alphas.
    auto fa = static_cast<uint16_t>(std::round(fillAlpha * 10000.0f));
    auto sa = static_cast<uint16_t>(std::round(strokeAlpha * 10000.0f));
    uint32_t key = (static_cast<uint32_t>(fa) << 16) | sa;
    auto it = _gsCache.find(key);
    if (it != _gsCache.end()) {
      return it->second;
    }

    std::string name = "GS" + std::to_string(_gsCount++);
    int objId = _store->add("<< /Type /ExtGState /ca " + PDFFloat(fillAlpha) + " /CA " +
                            PDFFloat(strokeAlpha) + " >>");
    _extGStates[name] = objId;
    _gsCache[key] = name;
    return name;
  }

  std::string getBlendModeExtGState(BlendMode mode) {
    auto it = _bmCache.find(mode);
    if (it != _bmCache.end()) {
      return it->second;
    }
    auto pdfName = BlendModeToPDFName(mode);
    if (!pdfName) {
      return {};
    }
    std::string name = "GS" + std::to_string(_gsCount++);
    int objId = _store->add("<< /Type /ExtGState /BM /" + std::string(pdfName) + " >>");
    _extGStates[name] = objId;
    _bmCache[mode] = name;
    return name;
  }

  // Creates a Type 2 (exponential interpolation) or Type 3 (stitching) gradient function.
  int createGradientFunction(const std::vector<ColorStop*>& stops) {
    if (stops.size() < 2) {
      return -1;
    }

    if (stops.size() == 2) {
      return _store->add("<< /FunctionType 2 /Domain [0 1] /C0 [" + PDFFloat(stops[0]->color.red) +
                         " " + PDFFloat(stops[0]->color.green) + " " +
                         PDFFloat(stops[0]->color.blue) + "] /C1 [" +
                         PDFFloat(stops[1]->color.red) + " " + PDFFloat(stops[1]->color.green) +
                         " " + PDFFloat(stops[1]->color.blue) + "] /N 1 >>");
    }

    std::vector<int> segFuncs;
    segFuncs.reserve(stops.size() - 1);
    for (size_t i = 0; i + 1 < stops.size(); i++) {
      segFuncs.push_back(_store->add(
          "<< /FunctionType 2 /Domain [0 1] /C0 [" + PDFFloat(stops[i]->color.red) + " " +
          PDFFloat(stops[i]->color.green) + " " + PDFFloat(stops[i]->color.blue) + "] /C1 [" +
          PDFFloat(stops[i + 1]->color.red) + " " + PDFFloat(stops[i + 1]->color.green) + " " +
          PDFFloat(stops[i + 1]->color.blue) + "] /N 1 >>"));
    }

    std::string funcsArr = "[";
    for (size_t i = 0; i < segFuncs.size(); i++) {
      if (i > 0) {
        funcsArr += " ";
      }
      funcsArr += std::to_string(segFuncs[i]) + " 0 R";
    }
    funcsArr += "]";

    std::string bounds = "[";
    for (size_t i = 1; i + 1 < stops.size(); i++) {
      if (i > 1) {
        bounds += " ";
      }
      bounds += PDFFloat(stops[i]->offset);
    }
    bounds += "]";

    std::string encode = "[";
    for (size_t i = 0; i < segFuncs.size(); i++) {
      if (i > 0) {
        encode += " ";
      }
      encode += "0 1";
    }
    encode += "]";

    return _store->add("<< /FunctionType 3 /Domain [0 1] /Functions " + funcsArr + " /Bounds " +
                       bounds + " /Encode " + encode + " >>");
  }

  static std::string matrixString(const Matrix& m) {
    return "[" + PDFFloat(m.a) + " " + PDFFloat(m.b) + " " + PDFFloat(m.c) + " " + PDFFloat(m.d) +
           " " + PDFFloat(m.tx) + " " + PDFFloat(m.ty) + "]";
  }

  // Creates a Type 2 (axial) shading wrapped in a Pattern, returns the pattern resource name.
  // The ctm parameter is the accumulated CTM from page-level transforms (Y-flip, layer matrices,
  // etc.) that must be baked into the pattern matrix, because PDF shading patterns are positioned
  // relative to the initial user space and are not affected by the current graphics state CTM.
  std::string addLinearGradient(const LinearGradient* grad, const Matrix& ctm) {
    int funcId = createGradientFunction(grad->colorStops);
    if (funcId < 0) {
      return {};
    }

    int shadingId =
        _store->add("<< /ShadingType 2 /ColorSpace /DeviceRGB /Coords [" +
                    PDFFloat(grad->startPoint.x) + " " + PDFFloat(grad->startPoint.y) + " " +
                    PDFFloat(grad->endPoint.x) + " " + PDFFloat(grad->endPoint.y) + "] /Function " +
                    std::to_string(funcId) + " 0 R /Extend [true true] >>");

    std::string matStr = matrixString(ctm * grad->matrix);
    int patId = _store->add("<< /Type /Pattern /PatternType 2 /Shading " +
                            std::to_string(shadingId) + " 0 R /Matrix " + matStr + " >>");
    std::string patName = "P" + std::to_string(_patCount++);
    _patterns[patName] = patId;
    return patName;
  }

  // Creates a Type 3 (radial) shading wrapped in a Pattern, returns the pattern resource name.
  std::string addRadialGradient(const RadialGradient* grad, const Matrix& ctm) {
    int funcId = createGradientFunction(grad->colorStops);
    if (funcId < 0) {
      return {};
    }

    // Coords: [x0 y0 r0 x1 y1 r1] – start circle at center with r=0, end at full radius.
    int shadingId = _store->add("<< /ShadingType 3 /ColorSpace /DeviceRGB /Coords [" +
                                PDFFloat(grad->center.x) + " " + PDFFloat(grad->center.y) + " 0 " +
                                PDFFloat(grad->center.x) + " " + PDFFloat(grad->center.y) + " " +
                                PDFFloat(grad->radius) + "] /Function " + std::to_string(funcId) +
                                " 0 R /Extend [true true] >>");

    std::string matStr = matrixString(ctm * grad->matrix);
    int patId = _store->add("<< /Type /Pattern /PatternType 2 /Shading " +
                            std::to_string(shadingId) + " 0 R /Matrix " + matStr + " >>");
    std::string patName = "P" + std::to_string(_patCount++);
    _patterns[patName] = patId;
    return patName;
  }

  // Creates a Tiling Pattern (PatternType 1) that draws an image, returns the pattern resource name.
  std::string addImagePattern(const ImagePattern* pattern, const Matrix& ctm) {
    if (!pattern->image) {
      return {};
    }

    auto cacheIt = _imageCache.find(pattern->image);
    std::string xobjName = {};
    int xobjId = 0;
    int imageWidth = 0;
    int imageHeight = 0;

    if (cacheIt != _imageCache.end()) {
      xobjName = cacheIt->second.xObjectName;
      xobjId = cacheIt->second.objectId;
      imageWidth = cacheIt->second.width;
      imageHeight = cacheIt->second.height;
    } else {
      auto imageData = GetImageData(pattern->image);
      if (!imageData) {
        return {};
      }

      auto pdfImage = GetPDFImageData(imageData);
      if (pdfImage.rgbBytes.empty()) {
        return {};
      }
      imageWidth = pdfImage.width;
      imageHeight = pdfImage.height;

      std::string smaskRef;
      if (!pdfImage.alphaBytes.empty()) {
        int smaskId = _store->add(
            "<< /Type /XObject /Subtype /Image /Width " + std::to_string(imageWidth) + " /Height " +
            std::to_string(imageHeight) + " /ColorSpace /DeviceGray /BitsPerComponent 8 /Length " +
            std::to_string(pdfImage.alphaBytes.size()) + " >>\nstream\n" + pdfImage.alphaBytes +
            "\nendstream");
        smaskRef = " /SMask " + std::to_string(smaskId) + " 0 R";
      }

      xobjName = "Im" + std::to_string(_imgCount++);
      std::string xobjDict = "<< /Type /XObject /Subtype /Image /Width " +
                             std::to_string(imageWidth) + " /Height " +
                             std::to_string(imageHeight) +
                             " /ColorSpace /DeviceRGB /BitsPerComponent 8"
                             " /Filter /DCTDecode" +
                             smaskRef + " /Length " + std::to_string(pdfImage.rgbBytes.size()) +
                             " >>\nstream\n" + pdfImage.rgbBytes + "\nendstream";
      xobjId = _store->add(xobjDict);
      _imageCache[pattern->image] = {xobjName, xobjId, imageWidth, imageHeight};
    }

    auto width = static_cast<float>(imageWidth);
    auto height = static_cast<float>(imageHeight);

    bool clampX = (pattern->tileModeX == TileMode::Clamp || pattern->tileModeX == TileMode::Decal);
    bool clampY = (pattern->tileModeY == TileMode::Clamp || pattern->tileModeY == TileMode::Decal);
    float xStep = clampX ? 16384.0f : width;
    float yStep = clampY ? 16384.0f : height;

    // Content stream: draw the image inside the tile cell with Y-flip for PDF image coordinates.
    std::string tileStream = "q " + PDFFloat(width) + " 0 0 " + PDFFloat(-height) + " 0 " +
                             PDFFloat(height) + " cm /" + xobjName + " Do Q";

    std::string matStr = matrixString(ctm * pattern->matrix);
    std::string patDict =
        "<< /Type /Pattern /PatternType 1 /PaintType 1 /TilingType 1"
        " /BBox [0 0 " +
        PDFFloat(width) + " " + PDFFloat(height) + "] /XStep " + PDFFloat(xStep) + " /YStep " +
        PDFFloat(yStep) + " /Matrix " + matStr + " /Resources << /XObject << /" + xobjName + " " +
        std::to_string(xobjId) + " 0 R >> >> /Length " + std::to_string(tileStream.size()) +
        " >>\nstream\n" + tileStream + "\nendstream";
    int patId = _store->add(patDict);
    std::string patName = "P" + std::to_string(_patCount++);
    _patterns[patName] = patId;
    return patName;
  }

  PDFObjectStore* store() const {
    return _store;
  }

  std::string addSoftMaskExtGState(MaskType maskType, const std::string& formContent,
                                   const std::string& formResourceDict, float bboxWidth,
                                   float bboxHeight) {
    std::string bbox = "[0 0 " + PDFFloat(bboxWidth) + " " + PDFFloat(bboxHeight) + "]";
    int formId = _store->add("<< /Type /XObject /Subtype /Form /BBox " + bbox +
                             " /Group << /Type /Group /S /Transparency /CS /DeviceRGB >>"
                             " /Resources " +
                             formResourceDict + " /Length " + std::to_string(formContent.size()) +
                             " >>\nstream\n" + formContent + "\nendstream");

    std::string smaskSubtype = (maskType == MaskType::Luminance) ? "/Luminosity" : "/Alpha";
    std::string gsName = "GS" + std::to_string(_gsCount++);
    int gsId = _store->add("<< /Type /ExtGState /SMask << /Type /Mask /S " + smaskSubtype + " /G " +
                           std::to_string(formId) + " 0 R >> >>");
    _extGStates[gsName] = gsId;
    return gsName;
  }

  void ensureDefaultFont() {
    if (_fontRegistered) {
      return;
    }
    _fontRegistered = true;
    int fontId = _store->add(
        "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>");
    _fonts["F0"] = fontId;
  }

  std::string buildResourceDict() const {
    std::string dict = "<< ";

    if (!_extGStates.empty()) {
      dict += "/ExtGState << ";
      for (const auto& [name, objId] : _extGStates) {
        dict += "/" + name + " " + std::to_string(objId) + " 0 R ";
      }
      dict += ">> ";
    }

    if (!_patterns.empty()) {
      dict += "/Pattern << ";
      for (const auto& [name, objId] : _patterns) {
        dict += "/" + name + " " + std::to_string(objId) + " 0 R ";
      }
      dict += ">> ";
    }

    if (!_fonts.empty()) {
      dict += "/Font << ";
      for (const auto& [name, objId] : _fonts) {
        dict += "/" + name + " " + std::to_string(objId) + " 0 R ";
      }
      dict += ">> ";
    }

    dict += ">>";
    return dict;
  }

 private:
  PDFObjectStore* _store;
  std::map<std::string, int> _extGStates = {};
  std::map<std::string, int> _patterns = {};
  std::map<std::string, int> _fonts = {};
  std::unordered_map<uint32_t, std::string> _gsCache = {};
  std::map<BlendMode, std::string> _bmCache = {};

  struct ImageCacheEntry {
    std::string xObjectName;
    int objectId = 0;
    int width = 0;
    int height = 0;
  };
  std::map<const Image*, ImageCacheEntry> _imageCache = {};
  int _gsCount = 0;
  int _patCount = 0;
  int _imgCount = 0;
  bool _fontRegistered = false;
};

//==============================================================================
// Utility types and static helpers
//==============================================================================

static std::string EscapePDFString(const std::string& str) {
  std::string result;
  result.reserve(str.size() + 10);
  for (unsigned char c : str) {
    switch (c) {
      case '(':
        result += "\\(";
        break;
      case ')':
        result += "\\)";
        break;
      case '\\':
        result += "\\\\";
        break;
      default:
        if (c < 32 || c > 126) {
          char buf[5];
          snprintf(buf, sizeof(buf), "\\%03o", c);
          result += buf;
        } else {
          result += static_cast<char>(c);
        }
        break;
    }
  }
  return result;
}

//==============================================================================
// PDFWriter – converts PAGX nodes to PDF content stream operators
//==============================================================================

class PDFWriter {
 public:
  PDFWriter(PDFStream* stream, PDFResourceManager* resources, bool convertTextToPath,
            const Matrix& pageMatrix, float docWidth, float docHeight)
      : _stream(stream), _resources(resources), _convertTextToPath(convertTextToPath),
        _currentMatrix(pageMatrix), _docWidth(docWidth), _docHeight(docHeight) {
  }

  void writeLayer(const Layer* layer);

 private:
  PDFStream* _stream;
  PDFResourceManager* _resources;
  bool _convertTextToPath;
  Matrix _currentMatrix;
  float _docWidth;
  float _docHeight;

  void writeLayerContents(const Layer* layer, const Matrix& transform = {});
  void writeElements(const std::vector<Element*>& elements, const Matrix& transform = {});

  void writeRectangle(const Rectangle* rect, const FillStrokeInfo& fs, const Matrix& transform);
  void writeEllipse(const Ellipse* ellipse, const FillStrokeInfo& fs, const Matrix& transform);
  void writePath(const Path* path, const FillStrokeInfo& fs, const Matrix& transform);
  void writeText(const Text* text, const FillStrokeInfo& fs, const Matrix& transform);
  void writeTextAsPath(const Text* text, const FillStrokeInfo& fs, const Matrix& transform);
  void writeTextAsPDFText(const Text* text, const FillStrokeInfo& fs, const Matrix& transform);

  void emitPathData(const PathData& data, const Matrix& transform = {});
  void emitEllipsePath(float cx, float cy, float rx, float ry, const Matrix& transform = {});
  void emitRoundedRectPath(float x, float y, float w, float h, float r,
                           const Matrix& transform = {});
  void emitRectPath(float x, float y, float w, float h, const Matrix& transform = {});

  enum class ColorRefType { Solid, Pattern };

  struct ColorRef {
    ColorRefType type = ColorRefType::Solid;
    float r = 0, g = 0, b = 0;
    float alpha = 1.0f;
    std::string patternName;
  };

  ColorRef resolveColorSource(const ColorSource* source);

  struct PaintState {
    bool hasFill = false;
    bool hasStroke = false;
    bool isEvenOdd = false;
  };

  PaintState applyFillStrokeColors(const FillStrokeInfo& fs);
  void paintShape(const FillStrokeInfo& fs, FillRule fillRule = FillRule::Winding);
  void applyStrokeAttrs(const Stroke* stroke);

  void writeClipPath(const Layer* maskLayer, const Matrix& parentMatrix = {});
  void writeSoftMask(const Layer* maskLayer, MaskType maskType);
  void renderMaskLayerContent(const Layer* maskLayer);
};

//==============================================================================
// PDFWriter – path emission helpers
//==============================================================================

void PDFWriter::emitPathData(const PathData& data, const Matrix& transform) {
  bool hasTransform = !transform.isIdentity();
  Point cur = {};
  Point subpathStart = {};
  data.forEach(
      [this, &cur, &subpathStart, hasTransform, &transform](PathVerb verb, const Point* pts) {
        switch (verb) {
          case PathVerb::Move: {
            Point p = hasTransform ? transform.mapPoint(pts[0]) : pts[0];
            _stream->moveTo(p.x, p.y);
            cur = p;
            subpathStart = p;
            break;
          }
          case PathVerb::Line: {
            Point p = hasTransform ? transform.mapPoint(pts[0]) : pts[0];
            _stream->lineTo(p.x, p.y);
            cur = p;
            break;
          }
          case PathVerb::Quad: {
            Point cp = hasTransform ? transform.mapPoint(pts[0]) : pts[0];
            Point end = hasTransform ? transform.mapPoint(pts[1]) : pts[1];
            float c1x = cur.x + 2.0f / 3.0f * (cp.x - cur.x);
            float c1y = cur.y + 2.0f / 3.0f * (cp.y - cur.y);
            float c2x = end.x + 2.0f / 3.0f * (cp.x - end.x);
            float c2y = end.y + 2.0f / 3.0f * (cp.y - end.y);
            _stream->curveTo(c1x, c1y, c2x, c2y, end.x, end.y);
            cur = end;
            break;
          }
          case PathVerb::Cubic: {
            Point cp1 = hasTransform ? transform.mapPoint(pts[0]) : pts[0];
            Point cp2 = hasTransform ? transform.mapPoint(pts[1]) : pts[1];
            Point end = hasTransform ? transform.mapPoint(pts[2]) : pts[2];
            _stream->curveTo(cp1.x, cp1.y, cp2.x, cp2.y, end.x, end.y);
            cur = end;
            break;
          }
          case PathVerb::Close:
            _stream->closePath();
            cur = subpathStart;
            break;
        }
      });
}

void PDFWriter::emitEllipsePath(float cx, float cy, float rx, float ry, const Matrix& transform) {
  bool hasTransform = !transform.isIdentity();
  float kx = rx * kKappa;
  float ky = ry * kKappa;
  Point points[] = {
      {cx + rx, cy},      {cx + rx, cy + ky}, {cx + kx, cy + ry}, {cx, cy + ry},
      {cx - kx, cy + ry}, {cx - rx, cy + ky}, {cx - rx, cy},      {cx - rx, cy - ky},
      {cx - kx, cy - ry}, {cx, cy - ry},      {cx + kx, cy - ry}, {cx + rx, cy - ky},
      {cx + rx, cy},
  };
  if (hasTransform) {
    for (auto& p : points) {
      p = transform.mapPoint(p);
    }
  }
  _stream->moveTo(points[0].x, points[0].y);
  _stream->curveTo(points[1].x, points[1].y, points[2].x, points[2].y, points[3].x, points[3].y);
  _stream->curveTo(points[4].x, points[4].y, points[5].x, points[5].y, points[6].x, points[6].y);
  _stream->curveTo(points[7].x, points[7].y, points[8].x, points[8].y, points[9].x, points[9].y);
  _stream->curveTo(points[10].x, points[10].y, points[11].x, points[11].y, points[12].x,
                   points[12].y);
  _stream->closePath();
}

void PDFWriter::emitRoundedRectPath(float x, float y, float w, float h, float r,
                                    const Matrix& transform) {
  bool hasTransform = !transform.isIdentity();
  float maxR = std::min(w, h) / 2.0f;
  r = std::min(r, maxR);
  float k = r * kKappa;
  Point points[] = {
      {x + r, y},         {x + w - r, y},     {x + w - r + k, y},     {x + w, y + r - k},
      {x + w, y + r},     {x + w, y + h - r}, {x + w, y + h - r + k}, {x + w - r + k, y + h},
      {x + w - r, y + h}, {x + r, y + h},     {x + r - k, y + h},     {x, y + h - r + k},
      {x, y + h - r},     {x, y + r},         {x, y + r - k},         {x + r - k, y},
      {x + r, y},
  };
  if (hasTransform) {
    for (auto& p : points) {
      p = transform.mapPoint(p);
    }
  }
  _stream->moveTo(points[0].x, points[0].y);
  _stream->lineTo(points[1].x, points[1].y);
  _stream->curveTo(points[2].x, points[2].y, points[3].x, points[3].y, points[4].x, points[4].y);
  _stream->lineTo(points[5].x, points[5].y);
  _stream->curveTo(points[6].x, points[6].y, points[7].x, points[7].y, points[8].x, points[8].y);
  _stream->lineTo(points[9].x, points[9].y);
  _stream->curveTo(points[10].x, points[10].y, points[11].x, points[11].y, points[12].x,
                   points[12].y);
  _stream->lineTo(points[13].x, points[13].y);
  _stream->curveTo(points[14].x, points[14].y, points[15].x, points[15].y, points[16].x,
                   points[16].y);
  _stream->closePath();
}

void PDFWriter::emitRectPath(float x, float y, float w, float h, const Matrix& transform) {
  if (transform.isIdentity()) {
    _stream->rect(x, y, w, h);
    return;
  }
  Point p0 = transform.mapPoint({x, y});
  Point p1 = transform.mapPoint({x + w, y});
  Point p2 = transform.mapPoint({x + w, y + h});
  Point p3 = transform.mapPoint({x, y + h});
  _stream->moveTo(p0.x, p0.y);
  _stream->lineTo(p1.x, p1.y);
  _stream->lineTo(p2.x, p2.y);
  _stream->lineTo(p3.x, p3.y);
  _stream->closePath();
}

//==============================================================================
// PDFWriter – color source resolution
//==============================================================================

PDFWriter::ColorRef PDFWriter::resolveColorSource(const ColorSource* source) {
  if (!source) {
    return {};
  }

  if (source->nodeType() == NodeType::SolidColor) {
    auto solid = static_cast<const SolidColor*>(source);
    return {ColorRefType::Solid, solid->color.red,   solid->color.green,
            solid->color.blue,   solid->color.alpha, {}};
  }

  if (source->nodeType() == NodeType::LinearGradient) {
    auto grad = static_cast<const LinearGradient*>(source);
    std::string patName = _resources->addLinearGradient(grad, _currentMatrix);
    if (!patName.empty()) {
      return {ColorRefType::Pattern, 0, 0, 0, 1.0f, patName};
    }
  }

  if (source->nodeType() == NodeType::RadialGradient) {
    auto grad = static_cast<const RadialGradient*>(source);
    std::string patName = _resources->addRadialGradient(grad, _currentMatrix);
    if (!patName.empty()) {
      return {ColorRefType::Pattern, 0, 0, 0, 1.0f, patName};
    }
  }

  if (source->nodeType() == NodeType::ImagePattern) {
    auto pattern = static_cast<const ImagePattern*>(source);
    std::string patName = _resources->addImagePattern(pattern, _currentMatrix);
    if (!patName.empty()) {
      return {ColorRefType::Pattern, 0, 0, 0, 1.0f, patName};
    }
  }

  return {};
}

//==============================================================================
// PDFWriter – stroke attribute helpers
//==============================================================================

void PDFWriter::applyStrokeAttrs(const Stroke* stroke) {
  if (stroke->width != 1.0f) {
    _stream->setLineWidth(stroke->width);
  }
  // PDF line cap: 0=butt, 1=round, 2=square
  if (stroke->cap == LineCap::Round) {
    _stream->setLineCap(1);
  } else if (stroke->cap == LineCap::Square) {
    _stream->setLineCap(2);
  }
  // PDF line join: 0=miter, 1=round, 2=bevel
  if (stroke->join == LineJoin::Round) {
    _stream->setLineJoin(1);
  } else if (stroke->join == LineJoin::Bevel) {
    _stream->setLineJoin(2);
  }
  if (stroke->join == LineJoin::Miter && stroke->miterLimit != 4.0f) {
    _stream->setMiterLimit(stroke->miterLimit);
  }
  if (!stroke->dashes.empty()) {
    _stream->setDash(stroke->dashes, stroke->dashOffset);
  }
}

//==============================================================================
// PDFWriter – shape painting (fill + stroke)
//==============================================================================

PDFWriter::PaintState PDFWriter::applyFillStrokeColors(const FillStrokeInfo& fs) {
  PaintState state = {};
  state.hasFill = fs.fill && fs.fill->color;
  state.hasStroke = fs.stroke && fs.stroke->color;
  state.isEvenOdd = fs.fill && (fs.fill->fillRule == FillRule::EvenOdd);

  ColorRef fillRef = {};
  ColorRef strokeRef = {};
  if (state.hasFill) {
    fillRef = resolveColorSource(fs.fill->color);
  }
  if (state.hasStroke) {
    strokeRef = resolveColorSource(fs.stroke->color);
  }

  float fillAlpha = state.hasFill ? (fillRef.alpha * fs.fill->alpha) : 1.0f;
  float strokeAlpha = state.hasStroke ? (strokeRef.alpha * fs.stroke->alpha) : 1.0f;
  if (fillAlpha < 1.0f || strokeAlpha < 1.0f) {
    _stream->setExtGState(_resources->getExtGState(fillAlpha, strokeAlpha));
  }

  if (state.hasStroke) {
    applyStrokeAttrs(fs.stroke);
    if (strokeRef.type == ColorRefType::Solid) {
      _stream->setStrokeRGB(strokeRef.r, strokeRef.g, strokeRef.b);
    } else {
      _stream->setStrokePattern(strokeRef.patternName);
    }
  }

  if (state.hasFill) {
    if (fillRef.type == ColorRefType::Solid) {
      _stream->setFillRGB(fillRef.r, fillRef.g, fillRef.b);
    } else {
      _stream->setFillPattern(fillRef.patternName);
    }
  }

  return state;
}

void PDFWriter::paintShape(const FillStrokeInfo& fs, FillRule fillRule) {
  bool hasFill = fs.fill && fs.fill->color;
  bool hasStroke = fs.stroke && fs.stroke->color;
  if (!hasFill && !hasStroke) {
    _stream->endPath();
    return;
  }

  auto state = applyFillStrokeColors(fs);
  bool isEvenOdd = (fillRule == FillRule::EvenOdd);
  if (state.hasFill && state.hasStroke) {
    isEvenOdd ? _stream->fillEvenOddAndStroke() : _stream->fillAndStroke();
  } else if (state.hasFill) {
    isEvenOdd ? _stream->fillEvenOdd() : _stream->fill();
  } else {
    _stream->stroke();
  }
}

//==============================================================================
// PDFWriter – shape elements
//==============================================================================

void PDFWriter::writeRectangle(const Rectangle* rect, const FillStrokeInfo& fs,
                               const Matrix& transform) {
  float x = rect->position.x - rect->size.width / 2;
  float y = rect->position.y - rect->size.height / 2;

  _stream->save();
  Matrix savedMatrix = _currentMatrix;
  if (!transform.isIdentity()) {
    _stream->concatMatrix(transform);
    _currentMatrix = _currentMatrix * transform;
  }

  if (rect->roundness > 0) {
    emitRoundedRectPath(x, y, rect->size.width, rect->size.height, rect->roundness);
  } else {
    _stream->rect(x, y, rect->size.width, rect->size.height);
  }

  FillRule rule = fs.fill ? fs.fill->fillRule : FillRule::Winding;
  paintShape(fs, rule);
  _currentMatrix = savedMatrix;
  _stream->restore();
}

void PDFWriter::writeEllipse(const Ellipse* ellipse, const FillStrokeInfo& fs,
                             const Matrix& transform) {
  float rx = ellipse->size.width / 2;
  float ry = ellipse->size.height / 2;

  _stream->save();
  Matrix savedMatrix = _currentMatrix;
  if (!transform.isIdentity()) {
    _stream->concatMatrix(transform);
    _currentMatrix = _currentMatrix * transform;
  }

  emitEllipsePath(ellipse->position.x, ellipse->position.y, rx, ry);

  FillRule rule = fs.fill ? fs.fill->fillRule : FillRule::Winding;
  paintShape(fs, rule);
  _currentMatrix = savedMatrix;
  _stream->restore();
}

void PDFWriter::writePath(const Path* path, const FillStrokeInfo& fs, const Matrix& transform) {
  if (!path->data || path->data->isEmpty()) {
    return;
  }

  _stream->save();
  Matrix savedMatrix = _currentMatrix;
  if (!transform.isIdentity()) {
    _stream->concatMatrix(transform);
    _currentMatrix = _currentMatrix * transform;
  }

  emitPathData(*path->data);

  FillRule rule = fs.fill ? fs.fill->fillRule : FillRule::Winding;
  paintShape(fs, rule);
  _currentMatrix = savedMatrix;
  _stream->restore();
}

//==============================================================================
// PDFWriter – text elements
//==============================================================================

void PDFWriter::writeTextAsPath(const Text* text, const FillStrokeInfo& fs,
                                const Matrix& transform) {
  float textPosX = text->position.x;
  float textPosY = text->position.y;

  auto glyphPaths = ComputeGlyphPaths(*text, textPosX, textPosY);
  if (glyphPaths.empty()) {
    return;
  }

  _stream->save();
  Matrix savedMatrix = _currentMatrix;
  if (!transform.isIdentity()) {
    _stream->concatMatrix(transform);
    _currentMatrix = _currentMatrix * transform;
  }

  auto state = applyFillStrokeColors(fs);

  for (const auto& gp : glyphPaths) {
    _stream->save();
    _stream->concatMatrix(gp.transform);
    emitPathData(*gp.pathData);
    if (state.hasFill && state.hasStroke) {
      state.isEvenOdd ? _stream->fillEvenOddAndStroke() : _stream->fillAndStroke();
    } else if (state.hasFill) {
      state.isEvenOdd ? _stream->fillEvenOdd() : _stream->fill();
    } else if (state.hasStroke) {
      _stream->stroke();
    }
    _stream->restore();
  }

  _currentMatrix = savedMatrix;
  _stream->restore();
}

void PDFWriter::writeTextAsPDFText(const Text* text, const FillStrokeInfo& fs,
                                   const Matrix& transform) {
  if (text->text.empty()) {
    return;
  }

  _resources->ensureDefaultFont();

  _stream->save();
  Matrix savedMatrix = _currentMatrix;
  if (!transform.isIdentity()) {
    _stream->concatMatrix(transform);
    _currentMatrix = _currentMatrix * transform;
  }

  bool hasFill = fs.fill && fs.fill->color;
  bool hasStroke = fs.stroke && fs.stroke->color;
  ColorRef fillRef = {};
  if (hasFill) {
    fillRef = resolveColorSource(fs.fill->color);
    float fillAlpha = fillRef.alpha * fs.fill->alpha;
    if (fillAlpha < 1.0f) {
      _stream->setExtGState(_resources->getExtGState(fillAlpha, 1.0f));
    }
    if (fillRef.type == ColorRefType::Solid) {
      _stream->setFillRGB(fillRef.r, fillRef.g, fillRef.b);
    }
  }
  if (hasStroke) {
    auto strokeRef = resolveColorSource(fs.stroke->color);
    if (strokeRef.type == ColorRefType::Solid) {
      _stream->setStrokeRGB(strokeRef.r, strokeRef.g, strokeRef.b);
    }
  }

  _stream->beginText();
  _stream->setFont("F0", text->fontSize);
  // Under the Y-flip page transform, text needs an inverted Y scale to render right-side up.
  _stream->setTextMatrix(1, 0, 0, -1, text->position.x, text->position.y);
  _stream->showText(EscapePDFString(text->text));
  _stream->endText();

  _currentMatrix = savedMatrix;
  _stream->restore();
}

void PDFWriter::writeText(const Text* text, const FillStrokeInfo& fs, const Matrix& transform) {
  if (text->text.empty()) {
    return;
  }

  if (_convertTextToPath && !text->glyphRuns.empty()) {
    writeTextAsPath(text, fs, transform);
    return;
  }

  writeTextAsPDFText(text, fs, transform);
}

//==============================================================================
// PDFWriter – clip path
//==============================================================================

void PDFWriter::writeClipPath(const Layer* maskLayer, const Matrix& parentMatrix) {
  Matrix layerMatrix = BuildLayerMatrix(maskLayer);
  Matrix combined = parentMatrix * layerMatrix;

  for (const auto* element : maskLayer->contents) {
    auto type = element->nodeType();
    switch (type) {
      case NodeType::Rectangle: {
        auto rect = static_cast<const Rectangle*>(element);
        float x = rect->position.x - rect->size.width / 2;
        float y = rect->position.y - rect->size.height / 2;
        if (rect->roundness > 0) {
          emitRoundedRectPath(x, y, rect->size.width, rect->size.height, rect->roundness, combined);
        } else {
          emitRectPath(x, y, rect->size.width, rect->size.height, combined);
        }
        break;
      }
      case NodeType::Ellipse: {
        auto ellipse = static_cast<const Ellipse*>(element);
        float rx = ellipse->size.width / 2;
        float ry = ellipse->size.height / 2;
        emitEllipsePath(ellipse->position.x, ellipse->position.y, rx, ry, combined);
        break;
      }
      case NodeType::Path: {
        auto path = static_cast<const Path*>(element);
        if (path->data && !path->data->isEmpty()) {
          emitPathData(*path->data, combined);
        }
        break;
      }
      default:
        break;
    }
  }

  for (const auto* child : maskLayer->children) {
    writeClipPath(child, combined);
  }
}

//==============================================================================
// PDFWriter – soft mask (Alpha / Luminance)
//==============================================================================

void PDFWriter::renderMaskLayerContent(const Layer* maskLayer) {
  Matrix layerMatrix = BuildLayerMatrix(maskLayer);
  bool needsGroup = !layerMatrix.isIdentity() || maskLayer->alpha < 1.0f;
  Matrix savedMatrix = _currentMatrix;

  if (needsGroup) {
    _stream->save();
    if (!layerMatrix.isIdentity()) {
      _stream->concatMatrix(layerMatrix);
      _currentMatrix = _currentMatrix * layerMatrix;
    }
    if (maskLayer->alpha < 1.0f) {
      _stream->setExtGState(_resources->getExtGState(maskLayer->alpha, maskLayer->alpha));
    }
  }

  writeLayerContents(maskLayer);
  for (const auto* child : maskLayer->children) {
    renderMaskLayerContent(child);
  }

  if (needsGroup) {
    _currentMatrix = savedMatrix;
    _stream->restore();
  }
}

void PDFWriter::writeSoftMask(const Layer* maskLayer, MaskType maskType) {
  PDFStream maskStream;
  PDFResourceManager maskResources(_resources->store());
  PDFWriter maskWriter(&maskStream, &maskResources, _convertTextToPath, _currentMatrix, _docWidth,
                       _docHeight);
  maskWriter.renderMaskLayerContent(maskLayer);

  std::string formContent = maskStream.release();
  std::string formResourceDict = maskResources.buildResourceDict();

  std::string gsName = _resources->addSoftMaskExtGState(maskType, formContent, formResourceDict,
                                                        _docWidth, _docHeight);
  _stream->setExtGState(gsName);
}

//==============================================================================
// PDFWriter – element list and layer writing
//==============================================================================

void PDFWriter::writeElements(const std::vector<Element*>& elements, const Matrix& transform) {
  auto fs = CollectFillStroke(elements);

  for (const auto* element : elements) {
    auto type = element->nodeType();
    if (type == NodeType::Fill || type == NodeType::Stroke || type == NodeType::TextBox) {
      continue;
    }
    switch (type) {
      case NodeType::Rectangle:
        writeRectangle(static_cast<const Rectangle*>(element), fs, transform);
        break;
      case NodeType::Ellipse:
        writeEllipse(static_cast<const Ellipse*>(element), fs, transform);
        break;
      case NodeType::Path:
        writePath(static_cast<const Path*>(element), fs, transform);
        break;
      case NodeType::Text:
        writeText(static_cast<const Text*>(element), fs, transform);
        break;
      case NodeType::Group: {
        auto* group = static_cast<const Group*>(element);
        Matrix groupMatrix = BuildGroupMatrix(group);
        Matrix combined = transform * groupMatrix;

        bool hasAlpha = group->alpha < 1.0f;
        if (hasAlpha || !combined.isIdentity()) {
          _stream->save();
          Matrix savedMatrix = _currentMatrix;
          if (!combined.isIdentity()) {
            _stream->concatMatrix(combined);
            _currentMatrix = _currentMatrix * combined;
          }
          if (hasAlpha) {
            _stream->setExtGState(_resources->getExtGState(group->alpha, group->alpha));
          }
          writeElements(group->elements, {});
          _currentMatrix = savedMatrix;
          _stream->restore();
        } else {
          writeElements(group->elements, combined);
        }
        break;
      }
      default:
        break;
    }
  }
}

void PDFWriter::writeLayerContents(const Layer* layer, const Matrix& transform) {
  writeElements(layer->contents, transform);
}

void PDFWriter::writeLayer(const Layer* layer) {
  if (!layer->visible && layer->mask == nullptr) {
    return;
  }

  bool needsGroup = !layer->matrix.isIdentity() || layer->alpha < 1.0f || layer->mask != nullptr ||
                    layer->x != 0.0f || layer->y != 0.0f ||
                    layer->blendMode != BlendMode::Normal;

  if (!needsGroup) {
    writeLayerContents(layer);
    for (const auto* child : layer->children) {
      writeLayer(child);
    }
    return;
  }

  _stream->save();
  Matrix savedMatrix = _currentMatrix;

  Matrix layerMatrix = BuildLayerMatrix(layer);
  if (!layerMatrix.isIdentity()) {
    _stream->concatMatrix(layerMatrix);
    _currentMatrix = _currentMatrix * layerMatrix;
  }

  if (layer->alpha < 1.0f) {
    _stream->setExtGState(_resources->getExtGState(layer->alpha, layer->alpha));
  }

  if (layer->blendMode != BlendMode::Normal) {
    auto gsName = _resources->getBlendModeExtGState(layer->blendMode);
    if (!gsName.empty()) {
      _stream->setExtGState(gsName);
    }
  }

  if (layer->mask != nullptr) {
    if (layer->maskType == MaskType::Contour) {
      FillRule clipRule = DetectMaskFillRule(layer->mask);
      writeClipPath(layer->mask);
      if (clipRule == FillRule::EvenOdd) {
        _stream->clipEvenOdd();
      } else {
        _stream->clip();
      }
      _stream->endPath();
    } else {
      writeSoftMask(layer->mask, layer->maskType);
    }
  }

  writeLayerContents(layer);

  for (const auto* child : layer->children) {
    writeLayer(child);
  }

  _currentMatrix = savedMatrix;
  _stream->restore();
}

//==============================================================================
// Main Export functions
//==============================================================================

std::string PDFExporter::ToPDF(const PAGXDocument& doc, const Options& options) {
  PDFObjectStore store;
  PDFResourceManager resources(&store);
  PDFStream stream;

  // Apply page-level Y-flip so PAGX top-left origin maps to PDF bottom-left origin.
  Matrix yFlip = {};
  yFlip.a = 1;
  yFlip.b = 0;
  yFlip.c = 0;
  yFlip.d = -1;
  yFlip.tx = 0;
  yFlip.ty = doc.height;

  PDFWriter writer(&stream, &resources, options.convertTextToPath, yFlip, doc.width, doc.height);

  stream.save();
  stream.concatMatrix(yFlip);

  for (const auto* layer : doc.layers) {
    writer.writeLayer(layer);
  }

  stream.restore();

  std::string contentData = stream.release();

  // Reserve object IDs for the fixed structure: Catalog(1), Pages(2), Page(3), ContentStream(4)
  int catalogId = store.reserve();
  int pagesId = store.reserve();
  int pageId = store.reserve();
  int contentId = store.reserve();

  // Content stream object
  store.set(contentId, "<< /Length " + std::to_string(contentData.size()) + " >>\nstream\n" +
                           contentData + "\nendstream");

  // Page object
  std::string resourceDict = resources.buildResourceDict();
  store.set(pageId,
            "<< /Type /Page /Parent " + std::to_string(pagesId) + " 0 R /MediaBox [0 0 " +
                PDFFloat(doc.width) + " " + PDFFloat(doc.height) +
                "] /Group << /Type /Group /S /Transparency /CS /DeviceRGB >>"
                " /Contents " +
                std::to_string(contentId) + " 0 R /Resources " + resourceDict + " >>");

  // Pages tree
  store.set(pagesId, "<< /Type /Pages /Kids [" + std::to_string(pageId) + " 0 R] /Count 1 >>");

  // Catalog
  store.set(catalogId, "<< /Type /Catalog /Pages " + std::to_string(pagesId) + " 0 R >>");

  return store.serialize(catalogId);
}

bool PDFExporter::ToFile(const PAGXDocument& document, const std::string& filePath,
                         const Options& options) {
  auto pdfContent = ToPDF(document, options);
  if (pdfContent.empty()) {
    return false;
  }
  std::ofstream file(filePath, std::ios::binary);
  if (!file) {
    return false;
  }
  file.write(pdfContent.data(), static_cast<std::streamsize>(pdfContent.size()));
  return file.good();
}

}  // namespace pagx
