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
#include <fstream>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include "base/utils/MathUtil.h"
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
#include "pagx/utils/StringParser.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/ImageInfo.h"
#include "tgfx/core/Pixmap.h"

namespace pagx {

using pag::DegreesToRadians;
using pag::FloatNearlyZero;

// 4*(sqrt(2)-1)/3, optimal cubic Bezier approximation for quarter-circle arcs.
static constexpr float kKappa = 0.5522847498f;

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
    _buf += FloatToString(v);
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

static bool IsJPEG(const uint8_t* data, size_t size) {
  return size >= 2 && data[0] == 0xFF && data[1] == 0xD8;
}

static std::shared_ptr<tgfx::Data> GetImageData(const Image* image) {
  if (image->data) {
    return tgfx::Data::MakeWithoutCopy(image->data->bytes(), image->data->size());
  }
  if (!image->filePath.empty()) {
    return tgfx::Data::MakeFromFile(image->filePath);
  }
  return nullptr;
}

static std::string GetJPEGData(const std::shared_ptr<tgfx::Data>& imageData, int* outWidth,
                               int* outHeight) {
  auto codec = tgfx::ImageCodec::MakeFrom(imageData);
  if (codec == nullptr) {
    return {};
  }
  *outWidth = codec->width();
  *outHeight = codec->height();
  if (IsJPEG(imageData->bytes(), imageData->size())) {
    return {reinterpret_cast<const char*>(imageData->bytes()), imageData->size()};
  }
  auto info = tgfx::ImageInfo::Make(*outWidth, *outHeight, tgfx::ColorType::RGBA_8888);
  std::vector<uint8_t> pixels(info.byteSize());
  if (!codec->readPixels(info, pixels.data())) {
    return {};
  }
  tgfx::Pixmap pixmap(info, pixels.data());
  auto jpegData = tgfx::ImageCodec::Encode(pixmap, tgfx::EncodedFormat::JPEG, 95);
  if (jpegData == nullptr) {
    return {};
  }
  return {reinterpret_cast<const char*>(jpegData->bytes()), jpegData->size()};
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
    int objId = _store->add("<< /Type /ExtGState /ca " + FloatToString(fillAlpha) + " /CA " +
                            FloatToString(strokeAlpha) + " >>");
    _extGStates[name] = objId;
    _gsCache[key] = name;
    return name;
  }

  // Creates a Type 2 (exponential interpolation) or Type 3 (stitching) gradient function.
  int createGradientFunction(const std::vector<ColorStop*>& stops) {
    if (stops.size() < 2) {
      return -1;
    }

    if (stops.size() == 2) {
      return _store->add(
          "<< /FunctionType 2 /Domain [0 1] /C0 [" + FloatToString(stops[0]->color.red) + " " +
          FloatToString(stops[0]->color.green) + " " + FloatToString(stops[0]->color.blue) +
          "] /C1 [" + FloatToString(stops[1]->color.red) + " " +
          FloatToString(stops[1]->color.green) + " " + FloatToString(stops[1]->color.blue) +
          "] /N 1 >>");
    }

    std::vector<int> segFuncs;
    segFuncs.reserve(stops.size() - 1);
    for (size_t i = 0; i + 1 < stops.size(); i++) {
      segFuncs.push_back(_store->add(
          "<< /FunctionType 2 /Domain [0 1] /C0 [" + FloatToString(stops[i]->color.red) + " " +
          FloatToString(stops[i]->color.green) + " " + FloatToString(stops[i]->color.blue) +
          "] /C1 [" + FloatToString(stops[i + 1]->color.red) + " " +
          FloatToString(stops[i + 1]->color.green) + " " + FloatToString(stops[i + 1]->color.blue) +
          "] /N 1 >>"));
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
      bounds += FloatToString(stops[i]->offset);
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
    return "[" + FloatToString(m.a) + " " + FloatToString(m.b) + " " + FloatToString(m.c) + " " +
           FloatToString(m.d) + " " + FloatToString(m.tx) + " " + FloatToString(m.ty) + "]";
  }

  // Creates a Type 2 (axial) shading wrapped in a Pattern, returns the pattern resource name.
  std::string addLinearGradient(const LinearGradient* grad) {
    int funcId = createGradientFunction(grad->colorStops);
    if (funcId < 0) {
      return {};
    }

    int shadingId =
        _store->add("<< /ShadingType 2 /ColorSpace /DeviceRGB /Coords [" +
                    FloatToString(grad->startPoint.x) + " " + FloatToString(grad->startPoint.y) +
                    " " + FloatToString(grad->endPoint.x) + " " + FloatToString(grad->endPoint.y) +
                    "] /Function " + std::to_string(funcId) + " 0 R /Extend [true true] >>");

    std::string matStr = matrixString(grad->matrix);
    int patId = _store->add("<< /Type /Pattern /PatternType 2 /Shading " +
                            std::to_string(shadingId) + " 0 R /Matrix " + matStr + " >>");
    std::string patName = "P" + std::to_string(_patCount++);
    _patterns[patName] = patId;
    return patName;
  }

  // Creates a Type 3 (radial) shading wrapped in a Pattern, returns the pattern resource name.
  std::string addRadialGradient(const RadialGradient* grad) {
    int funcId = createGradientFunction(grad->colorStops);
    if (funcId < 0) {
      return {};
    }

    // Coords: [x0 y0 r0 x1 y1 r1] – start circle at center with r=0, end at full radius.
    int shadingId = _store->add(
        "<< /ShadingType 3 /ColorSpace /DeviceRGB /Coords [" + FloatToString(grad->center.x) + " " +
        FloatToString(grad->center.y) + " 0 " + FloatToString(grad->center.x) + " " +
        FloatToString(grad->center.y) + " " + FloatToString(grad->radius) + "] /Function " +
        std::to_string(funcId) + " 0 R /Extend [true true] >>");

    std::string matStr = matrixString(grad->matrix);
    int patId = _store->add("<< /Type /Pattern /PatternType 2 /Shading " +
                            std::to_string(shadingId) + " 0 R /Matrix " + matStr + " >>");
    std::string patName = "P" + std::to_string(_patCount++);
    _patterns[patName] = patId;
    return patName;
  }

  // Creates a Tiling Pattern (PatternType 1) that draws an image, returns the pattern resource name.
  std::string addImagePattern(const ImagePattern* pattern) {
    if (!pattern->image) {
      return {};
    }

    auto cacheIt = _imageXObjectCache.find(pattern->image);
    std::string xobjName = {};
    int xobjId = 0;
    int imageWidth = 0;
    int imageHeight = 0;

    if (cacheIt != _imageXObjectCache.end()) {
      xobjName = cacheIt->second;
      xobjId = _xObjects[xobjName];
      imageWidth = _imageWidthCache[pattern->image];
      imageHeight = _imageHeightCache[pattern->image];
    } else {
      auto imageData = GetImageData(pattern->image);
      if (!imageData) {
        return {};
      }

      std::string jpegBytes = GetJPEGData(imageData, &imageWidth, &imageHeight);
      if (jpegBytes.empty()) {
        return {};
      }

      xobjName = "Im" + std::to_string(_imgCount++);
      std::string xobjDict =
          "<< /Type /XObject /Subtype /Image /Width " + std::to_string(imageWidth) + " /Height " +
          std::to_string(imageHeight) +
          " /ColorSpace /DeviceRGB /BitsPerComponent 8"
          " /Filter /DCTDecode /Length " +
          std::to_string(jpegBytes.size()) + " >>\nstream\n" + jpegBytes + "\nendstream";
      xobjId = _store->add(xobjDict);
      _xObjects[xobjName] = xobjId;
      _imageXObjectCache[pattern->image] = xobjName;
      _imageWidthCache[pattern->image] = imageWidth;
      _imageHeightCache[pattern->image] = imageHeight;
    }

    auto width = static_cast<float>(imageWidth);
    auto height = static_cast<float>(imageHeight);

    bool clampX = (pattern->tileModeX == TileMode::Clamp || pattern->tileModeX == TileMode::Decal);
    bool clampY = (pattern->tileModeY == TileMode::Clamp || pattern->tileModeY == TileMode::Decal);
    float xStep = clampX ? 16384.0f : width;
    float yStep = clampY ? 16384.0f : height;

    // Content stream: draw the image inside the tile cell with Y-flip for PDF image coordinates.
    std::string tileStream = "q " + FloatToString(width) + " 0 0 " + FloatToString(-height) +
                             " 0 " + FloatToString(height) + " cm /" + xobjName + " Do Q";

    std::string matStr = matrixString(pattern->matrix);
    std::string patDict =
        "<< /Type /Pattern /PatternType 1 /PaintType 1 /TilingType 1"
        " /BBox [0 0 " +
        FloatToString(width) + " " + FloatToString(height) + "] /XStep " + FloatToString(xStep) +
        " /YStep " + FloatToString(yStep) + " /Matrix " + matStr + " /Resources << /XObject << /" +
        xobjName + " " + std::to_string(xobjId) + " 0 R >> >> /Length " +
        std::to_string(tileStream.size()) + " >>\nstream\n" + tileStream + "\nendstream";
    int patId = _store->add(patDict);
    std::string patName = "P" + std::to_string(_patCount++);
    _patterns[patName] = patId;
    return patName;
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
  std::map<std::string, int> _xObjects = {};
  std::unordered_map<uint32_t, std::string> _gsCache = {};
  std::map<const Image*, std::string> _imageXObjectCache = {};
  std::map<const Image*, int> _imageWidthCache = {};
  std::map<const Image*, int> _imageHeightCache = {};
  int _gsCount = 0;
  int _patCount = 0;
  int _imgCount = 0;
  bool _fontRegistered = false;
};

//==============================================================================
// Utility types and static helpers
//==============================================================================

struct PDFFillStrokeInfo {
  const Fill* fill = nullptr;
  const Stroke* stroke = nullptr;
  const TextBox* textBox = nullptr;
};

static PDFFillStrokeInfo CollectFillStrokePDF(const std::vector<Element*>& contents) {
  PDFFillStrokeInfo info = {};
  for (const auto* element : contents) {
    if (element->nodeType() == NodeType::Fill && !info.fill) {
      info.fill = static_cast<const Fill*>(element);
    } else if (element->nodeType() == NodeType::Stroke && !info.stroke) {
      info.stroke = static_cast<const Stroke*>(element);
    } else if (element->nodeType() == NodeType::TextBox && !info.textBox) {
      info.textBox = static_cast<const TextBox*>(element);
    }
    if (info.fill && info.stroke && info.textBox) {
      break;
    }
  }
  return info;
}

static Matrix BuildLayerMatrixPDF(const Layer* layer) {
  Matrix m = layer->matrix;
  if (layer->x != 0.0f || layer->y != 0.0f) {
    m = Matrix::Translate(layer->x, layer->y) * m;
  }
  return m;
}

static Matrix BuildGroupMatrixPDF(const Group* group) {
  bool hasAnchor = !FloatNearlyZero(group->anchor.x) || !FloatNearlyZero(group->anchor.y);
  bool hasPosition = !FloatNearlyZero(group->position.x) || !FloatNearlyZero(group->position.y);
  bool hasRotation = !FloatNearlyZero(group->rotation);
  bool hasScale =
      !FloatNearlyZero(group->scale.x - 1.0f) || !FloatNearlyZero(group->scale.y - 1.0f);
  bool hasSkew = !FloatNearlyZero(group->skew);

  if (!hasAnchor && !hasPosition && !hasRotation && !hasScale && !hasSkew) {
    return {};
  }

  Matrix m = {};
  if (hasAnchor) {
    m = Matrix::Translate(-group->anchor.x, -group->anchor.y);
  }
  if (hasScale) {
    m = Matrix::Scale(group->scale.x, group->scale.y) * m;
  }
  if (hasSkew) {
    m = Matrix::Rotate(group->skewAxis) * m;
    Matrix shear = {};
    shear.c = std::tan(DegreesToRadians(group->skew));
    m = shear * m;
    m = Matrix::Rotate(-group->skewAxis) * m;
  }
  if (hasRotation) {
    m = Matrix::Rotate(group->rotation) * m;
  }
  if (hasPosition) {
    m = Matrix::Translate(group->position.x, group->position.y) * m;
  }

  return m;
}

static FillRule DetectMaskFillRule(const Layer* maskLayer) {
  for (const auto* element : maskLayer->contents) {
    if (element->nodeType() == NodeType::Fill) {
      auto rule = static_cast<const Fill*>(element)->fillRule;
      if (rule == FillRule::EvenOdd) {
        return FillRule::EvenOdd;
      }
    }
  }
  for (const auto* child : maskLayer->children) {
    if (DetectMaskFillRule(child) == FillRule::EvenOdd) {
      return FillRule::EvenOdd;
    }
  }
  return FillRule::Winding;
}

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
// PDFGlyphPath – glyph path data for text-to-path conversion
//==============================================================================

struct PDFGlyphPath {
  Matrix transform;
  const PathData* pathData;
};

static std::vector<PDFGlyphPath> ComputeGlyphPathsPDF(const Text& text, float textPosX,
                                                      float textPosY) {
  std::vector<PDFGlyphPath> result;
  for (const auto* run : text.glyphRuns) {
    if (!run->font || run->glyphs.empty()) {
      continue;
    }
    float scale = run->fontSize / static_cast<float>(run->font->unitsPerEm);
    float currentX = textPosX + run->x;
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
      if (!glyph || !glyph->path || glyph->path->isEmpty()) {
        continue;
      }

      float posX = 0;
      float posY = 0;
      if (i < run->positions.size()) {
        posX = textPosX + run->x + run->positions[i].x;
        posY = textPosY + run->y + run->positions[i].y;
        if (i < run->xOffsets.size()) {
          posX += run->xOffsets[i];
        }
      } else if (i < run->xOffsets.size()) {
        posX = textPosX + run->x + run->xOffsets[i];
        posY = textPosY + run->y;
      } else {
        posX = currentX;
        posY = textPosY + run->y;
      }
      currentX += glyph->advance * scale;

      Matrix glyphMatrix = Matrix::Translate(posX, posY) * Matrix::Scale(scale, scale);

      bool hasRotation = i < run->rotations.size() && run->rotations[i] != 0;
      bool hasGlyphScale =
          i < run->scales.size() && (run->scales[i].x != 1 || run->scales[i].y != 1);
      bool hasGlyphSkew = i < run->skews.size() && run->skews[i] != 0;

      if (hasRotation || hasGlyphScale || hasGlyphSkew) {
        float anchorX = glyph->advance * 0.5f;
        float anchorY = 0;
        if (i < run->anchors.size()) {
          anchorX += run->anchors[i].x;
          anchorY += run->anchors[i].y;
        }

        Matrix perGlyph = Matrix::Translate(-anchorX, -anchorY);
        if (hasGlyphScale) {
          perGlyph = Matrix::Scale(run->scales[i].x, run->scales[i].y) * perGlyph;
        }
        if (hasGlyphSkew) {
          Matrix shear = {};
          shear.c = std::tan(DegreesToRadians(run->skews[i]));
          perGlyph = shear * perGlyph;
        }
        if (hasRotation) {
          perGlyph = Matrix::Rotate(run->rotations[i]) * perGlyph;
        }
        perGlyph = Matrix::Translate(anchorX, anchorY) * perGlyph;
        glyphMatrix = glyphMatrix * perGlyph;
      }

      result.push_back({glyphMatrix, glyph->path});
    }
  }
  return result;
}

//==============================================================================
// PDFWriter – converts PAGX nodes to PDF content stream operators
//==============================================================================

class PDFWriter {
 public:
  PDFWriter(PDFStream* stream, PDFResourceManager* resources, bool convertTextToPath)
      : _stream(stream), _resources(resources), _convertTextToPath(convertTextToPath) {
  }

  void writeLayer(const Layer* layer);

 private:
  PDFStream* _stream;
  PDFResourceManager* _resources;
  bool _convertTextToPath;

  void writeLayerContents(const Layer* layer, const Matrix& transform = {});
  void writeElements(const std::vector<Element*>& elements, const Matrix& transform = {});

  void writeRectangle(const Rectangle* rect, const PDFFillStrokeInfo& fs, const Matrix& transform);
  void writeEllipse(const Ellipse* ellipse, const PDFFillStrokeInfo& fs, const Matrix& transform);
  void writePath(const Path* path, const PDFFillStrokeInfo& fs, const Matrix& transform);
  void writeText(const Text* text, const PDFFillStrokeInfo& fs, const Matrix& transform);
  void writeTextAsPath(const Text* text, const PDFFillStrokeInfo& fs, const Matrix& transform);
  void writeTextAsPDFText(const Text* text, const PDFFillStrokeInfo& fs, const Matrix& transform);

  void emitPathData(const PathData& data);
  void emitEllipsePath(float cx, float cy, float rx, float ry);
  void emitRoundedRectPath(float x, float y, float w, float h, float r);

  enum class ColorRefType { Solid, Pattern };

  struct ColorRef {
    ColorRefType type = ColorRefType::Solid;
    float r = 0, g = 0, b = 0;
    float alpha = 1.0f;
    std::string patternName;
  };

  ColorRef resolveColorSource(const ColorSource* source);
  void paintShape(const PDFFillStrokeInfo& fs, FillRule fillRule = FillRule::Winding);
  void applyStrokeAttrs(const Stroke* stroke);

  void writeClipPath(const Layer* maskLayer, const Matrix& parentMatrix = {});
};

//==============================================================================
// PDFWriter – path emission helpers
//==============================================================================

void PDFWriter::emitPathData(const PathData& data) {
  Point cur = {};
  Point subpathStart = {};
  data.forEach([this, &cur, &subpathStart](PathVerb verb, const Point* pts) {
    switch (verb) {
      case PathVerb::Move:
        _stream->moveTo(pts[0].x, pts[0].y);
        cur = pts[0];
        subpathStart = pts[0];
        break;
      case PathVerb::Line:
        _stream->lineTo(pts[0].x, pts[0].y);
        cur = pts[0];
        break;
      case PathVerb::Quad: {
        // Promote Q(p0, cp, p1) to cubic: C(p0, p0+2/3*(cp-p0), p1+2/3*(cp-p1), p1)
        float c1x = cur.x + 2.0f / 3.0f * (pts[0].x - cur.x);
        float c1y = cur.y + 2.0f / 3.0f * (pts[0].y - cur.y);
        float c2x = pts[1].x + 2.0f / 3.0f * (pts[0].x - pts[1].x);
        float c2y = pts[1].y + 2.0f / 3.0f * (pts[0].y - pts[1].y);
        _stream->curveTo(c1x, c1y, c2x, c2y, pts[1].x, pts[1].y);
        cur = pts[1];
        break;
      }
      case PathVerb::Cubic:
        _stream->curveTo(pts[0].x, pts[0].y, pts[1].x, pts[1].y, pts[2].x, pts[2].y);
        cur = pts[2];
        break;
      case PathVerb::Close:
        _stream->closePath();
        cur = subpathStart;
        break;
    }
  });
}

void PDFWriter::emitEllipsePath(float cx, float cy, float rx, float ry) {
  float kx = rx * kKappa;
  float ky = ry * kKappa;
  _stream->moveTo(cx + rx, cy);
  _stream->curveTo(cx + rx, cy + ky, cx + kx, cy + ry, cx, cy + ry);
  _stream->curveTo(cx - kx, cy + ry, cx - rx, cy + ky, cx - rx, cy);
  _stream->curveTo(cx - rx, cy - ky, cx - kx, cy - ry, cx, cy - ry);
  _stream->curveTo(cx + kx, cy - ry, cx + rx, cy - ky, cx + rx, cy);
  _stream->closePath();
}

void PDFWriter::emitRoundedRectPath(float x, float y, float w, float h, float r) {
  float maxR = std::min(w, h) / 2.0f;
  r = std::min(r, maxR);
  float k = r * kKappa;

  _stream->moveTo(x + r, y);
  _stream->lineTo(x + w - r, y);
  _stream->curveTo(x + w - r + k, y, x + w, y + r - k, x + w, y + r);
  _stream->lineTo(x + w, y + h - r);
  _stream->curveTo(x + w, y + h - r + k, x + w - r + k, y + h, x + w - r, y + h);
  _stream->lineTo(x + r, y + h);
  _stream->curveTo(x + r - k, y + h, x, y + h - r + k, x, y + h - r);
  _stream->lineTo(x, y + r);
  _stream->curveTo(x, y + r - k, x + r - k, y, x + r, y);
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
    std::string patName = _resources->addLinearGradient(grad);
    if (!patName.empty()) {
      return {ColorRefType::Pattern, 0, 0, 0, 1.0f, patName};
    }
  }

  if (source->nodeType() == NodeType::RadialGradient) {
    auto grad = static_cast<const RadialGradient*>(source);
    std::string patName = _resources->addRadialGradient(grad);
    if (!patName.empty()) {
      return {ColorRefType::Pattern, 0, 0, 0, 1.0f, patName};
    }
  }

  if (source->nodeType() == NodeType::ImagePattern) {
    auto pattern = static_cast<const ImagePattern*>(source);
    std::string patName = _resources->addImagePattern(pattern);
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

void PDFWriter::paintShape(const PDFFillStrokeInfo& fs, FillRule fillRule) {
  bool hasFill = fs.fill && fs.fill->color;
  bool hasStroke = fs.stroke && fs.stroke->color;
  if (!hasFill && !hasStroke) {
    _stream->endPath();
    return;
  }

  ColorRef fillRef = {};
  ColorRef strokeRef = {};
  if (hasFill) {
    fillRef = resolveColorSource(fs.fill->color);
  }
  if (hasStroke) {
    strokeRef = resolveColorSource(fs.stroke->color);
  }

  // Apply opacity via ExtGState when needed
  float fillAlpha = hasFill ? (fillRef.alpha * fs.fill->alpha) : 1.0f;
  float strokeAlpha = hasStroke ? (strokeRef.alpha * fs.stroke->alpha) : 1.0f;
  if (fillAlpha < 1.0f || strokeAlpha < 1.0f) {
    _stream->setExtGState(_resources->getExtGState(fillAlpha, strokeAlpha));
  }

  // Set stroke attributes before painting
  if (hasStroke) {
    applyStrokeAttrs(fs.stroke);
    if (strokeRef.type == ColorRefType::Solid) {
      _stream->setStrokeRGB(strokeRef.r, strokeRef.g, strokeRef.b);
    } else {
      _stream->setStrokePattern(strokeRef.patternName);
    }
  }

  // Set fill color and paint
  if (hasFill) {
    if (fillRef.type == ColorRefType::Solid) {
      _stream->setFillRGB(fillRef.r, fillRef.g, fillRef.b);
    } else {
      _stream->setFillPattern(fillRef.patternName);
    }
  }

  bool isEvenOdd = (fillRule == FillRule::EvenOdd);
  if (hasFill && hasStroke) {
    isEvenOdd ? _stream->fillEvenOddAndStroke() : _stream->fillAndStroke();
  } else if (hasFill) {
    isEvenOdd ? _stream->fillEvenOdd() : _stream->fill();
  } else {
    _stream->stroke();
  }
}

//==============================================================================
// PDFWriter – shape elements
//==============================================================================

void PDFWriter::writeRectangle(const Rectangle* rect, const PDFFillStrokeInfo& fs,
                               const Matrix& transform) {
  float x = rect->position.x - rect->size.width / 2;
  float y = rect->position.y - rect->size.height / 2;

  _stream->save();
  if (!transform.isIdentity()) {
    _stream->concatMatrix(transform);
  }

  if (rect->roundness > 0) {
    emitRoundedRectPath(x, y, rect->size.width, rect->size.height, rect->roundness);
  } else {
    _stream->rect(x, y, rect->size.width, rect->size.height);
  }

  FillRule rule = fs.fill ? fs.fill->fillRule : FillRule::Winding;
  paintShape(fs, rule);
  _stream->restore();
}

void PDFWriter::writeEllipse(const Ellipse* ellipse, const PDFFillStrokeInfo& fs,
                             const Matrix& transform) {
  float rx = ellipse->size.width / 2;
  float ry = ellipse->size.height / 2;

  _stream->save();
  if (!transform.isIdentity()) {
    _stream->concatMatrix(transform);
  }

  emitEllipsePath(ellipse->position.x, ellipse->position.y, rx, ry);

  FillRule rule = fs.fill ? fs.fill->fillRule : FillRule::Winding;
  paintShape(fs, rule);
  _stream->restore();
}

void PDFWriter::writePath(const Path* path, const PDFFillStrokeInfo& fs, const Matrix& transform) {
  if (!path->data || path->data->isEmpty()) {
    return;
  }

  _stream->save();
  if (!transform.isIdentity()) {
    _stream->concatMatrix(transform);
  }

  emitPathData(*path->data);

  FillRule rule = fs.fill ? fs.fill->fillRule : FillRule::Winding;
  paintShape(fs, rule);
  _stream->restore();
}

//==============================================================================
// PDFWriter – text elements
//==============================================================================

void PDFWriter::writeTextAsPath(const Text* text, const PDFFillStrokeInfo& fs,
                                const Matrix& transform) {
  float textPosX = text->position.x;
  float textPosY = text->position.y;

  auto glyphPaths = ComputeGlyphPathsPDF(*text, textPosX, textPosY);
  if (glyphPaths.empty()) {
    return;
  }

  _stream->save();
  if (!transform.isIdentity()) {
    _stream->concatMatrix(transform);
  }

  // Set up colors/opacity once for all glyphs
  bool hasFill = fs.fill && fs.fill->color;
  bool hasStroke = fs.stroke && fs.stroke->color;
  ColorRef fillRef = {};
  ColorRef strokeRef = {};

  if (hasFill) {
    fillRef = resolveColorSource(fs.fill->color);
  }
  if (hasStroke) {
    strokeRef = resolveColorSource(fs.stroke->color);
  }

  float fillAlpha = hasFill ? (fillRef.alpha * fs.fill->alpha) : 1.0f;
  float strokeAlpha = hasStroke ? (strokeRef.alpha * fs.stroke->alpha) : 1.0f;
  if (fillAlpha < 1.0f || strokeAlpha < 1.0f) {
    _stream->setExtGState(_resources->getExtGState(fillAlpha, strokeAlpha));
  }
  if (hasFill) {
    if (fillRef.type == ColorRefType::Solid) {
      _stream->setFillRGB(fillRef.r, fillRef.g, fillRef.b);
    } else {
      _stream->setFillPattern(fillRef.patternName);
    }
  }
  if (hasStroke) {
    applyStrokeAttrs(fs.stroke);
    if (strokeRef.type == ColorRefType::Solid) {
      _stream->setStrokeRGB(strokeRef.r, strokeRef.g, strokeRef.b);
    } else {
      _stream->setStrokePattern(strokeRef.patternName);
    }
  }

  FillRule rule = fs.fill ? fs.fill->fillRule : FillRule::Winding;
  bool isEvenOdd = (rule == FillRule::EvenOdd);

  for (const auto& gp : glyphPaths) {
    _stream->save();
    _stream->concatMatrix(gp.transform);
    emitPathData(*gp.pathData);
    if (hasFill && hasStroke) {
      isEvenOdd ? _stream->fillEvenOddAndStroke() : _stream->fillAndStroke();
    } else if (hasFill) {
      isEvenOdd ? _stream->fillEvenOdd() : _stream->fill();
    } else if (hasStroke) {
      _stream->stroke();
    }
    _stream->restore();
  }

  _stream->restore();
}

void PDFWriter::writeTextAsPDFText(const Text* text, const PDFFillStrokeInfo& fs,
                                   const Matrix& transform) {
  if (text->text.empty()) {
    return;
  }

  _resources->ensureDefaultFont();

  _stream->save();
  if (!transform.isIdentity()) {
    _stream->concatMatrix(transform);
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

  _stream->restore();
}

void PDFWriter::writeText(const Text* text, const PDFFillStrokeInfo& fs, const Matrix& transform) {
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
  Matrix layerMatrix = BuildLayerMatrixPDF(maskLayer);
  Matrix combined = parentMatrix * layerMatrix;

  for (const auto* element : maskLayer->contents) {
    auto type = element->nodeType();
    switch (type) {
      case NodeType::Rectangle: {
        auto rect = static_cast<const Rectangle*>(element);
        float x = rect->position.x - rect->size.width / 2;
        float y = rect->position.y - rect->size.height / 2;
        float w = rect->size.width;
        float h = rect->size.height;
        if (rect->roundness > 0) {
          float maxR = std::min(w, h) / 2.0f;
          float r = std::min(rect->roundness, maxR);
          float k = r * kKappa;
          Point pts[] = {
              combined.mapPoint({x + r, y}),
              combined.mapPoint({x + w - r, y}),
              combined.mapPoint({x + w - r + k, y}),
              combined.mapPoint({x + w, y + r - k}),
              combined.mapPoint({x + w, y + r}),
              combined.mapPoint({x + w, y + h - r}),
              combined.mapPoint({x + w, y + h - r + k}),
              combined.mapPoint({x + w - r + k, y + h}),
              combined.mapPoint({x + w - r, y + h}),
              combined.mapPoint({x + r, y + h}),
              combined.mapPoint({x + r - k, y + h}),
              combined.mapPoint({x, y + h - r + k}),
              combined.mapPoint({x, y + h - r}),
              combined.mapPoint({x, y + r}),
              combined.mapPoint({x, y + r - k}),
              combined.mapPoint({x + r - k, y}),
              combined.mapPoint({x + r, y}),
          };
          _stream->moveTo(pts[0].x, pts[0].y);
          _stream->lineTo(pts[1].x, pts[1].y);
          _stream->curveTo(pts[2].x, pts[2].y, pts[3].x, pts[3].y, pts[4].x, pts[4].y);
          _stream->lineTo(pts[5].x, pts[5].y);
          _stream->curveTo(pts[6].x, pts[6].y, pts[7].x, pts[7].y, pts[8].x, pts[8].y);
          _stream->lineTo(pts[9].x, pts[9].y);
          _stream->curveTo(pts[10].x, pts[10].y, pts[11].x, pts[11].y, pts[12].x, pts[12].y);
          _stream->lineTo(pts[13].x, pts[13].y);
          _stream->curveTo(pts[14].x, pts[14].y, pts[15].x, pts[15].y, pts[16].x, pts[16].y);
          _stream->closePath();
        } else if (combined.isIdentity()) {
          _stream->rect(x, y, w, h);
        } else {
          Point p0 = combined.mapPoint({x, y});
          Point p1 = combined.mapPoint({x + w, y});
          Point p2 = combined.mapPoint({x + w, y + h});
          Point p3 = combined.mapPoint({x, y + h});
          _stream->moveTo(p0.x, p0.y);
          _stream->lineTo(p1.x, p1.y);
          _stream->lineTo(p2.x, p2.y);
          _stream->lineTo(p3.x, p3.y);
          _stream->closePath();
        }
        break;
      }
      case NodeType::Ellipse: {
        auto ellipse = static_cast<const Ellipse*>(element);
        float cx = ellipse->position.x;
        float cy = ellipse->position.y;
        float rx = ellipse->size.width / 2;
        float ry = ellipse->size.height / 2;
        float kx = rx * kKappa;
        float ky = ry * kKappa;
        Point pts[] = {
            combined.mapPoint({cx + rx, cy}),
            combined.mapPoint({cx + rx, cy + ky}),
            combined.mapPoint({cx + kx, cy + ry}),
            combined.mapPoint({cx, cy + ry}),
            combined.mapPoint({cx - kx, cy + ry}),
            combined.mapPoint({cx - rx, cy + ky}),
            combined.mapPoint({cx - rx, cy}),
            combined.mapPoint({cx - rx, cy - ky}),
            combined.mapPoint({cx - kx, cy - ry}),
            combined.mapPoint({cx, cy - ry}),
            combined.mapPoint({cx + kx, cy - ry}),
            combined.mapPoint({cx + rx, cy - ky}),
            combined.mapPoint({cx + rx, cy}),
        };
        _stream->moveTo(pts[0].x, pts[0].y);
        _stream->curveTo(pts[1].x, pts[1].y, pts[2].x, pts[2].y, pts[3].x, pts[3].y);
        _stream->curveTo(pts[4].x, pts[4].y, pts[5].x, pts[5].y, pts[6].x, pts[6].y);
        _stream->curveTo(pts[7].x, pts[7].y, pts[8].x, pts[8].y, pts[9].x, pts[9].y);
        _stream->curveTo(pts[10].x, pts[10].y, pts[11].x, pts[11].y, pts[12].x, pts[12].y);
        _stream->closePath();
        break;
      }
      case NodeType::Path: {
        auto path = static_cast<const Path*>(element);
        if (path->data && !path->data->isEmpty()) {
          if (combined.isIdentity()) {
            emitPathData(*path->data);
          } else {
            Point cur = {};
            Point subpathStart = {};
            path->data->forEach([this, &cur, &subpathStart, &combined](PathVerb verb,
                                                                       const Point* pts) {
              switch (verb) {
                case PathVerb::Move: {
                  Point p = combined.mapPoint(pts[0]);
                  _stream->moveTo(p.x, p.y);
                  cur = p;
                  subpathStart = p;
                  break;
                }
                case PathVerb::Line: {
                  Point p = combined.mapPoint(pts[0]);
                  _stream->lineTo(p.x, p.y);
                  cur = p;
                  break;
                }
                case PathVerb::Quad: {
                  Point cp = combined.mapPoint(pts[0]);
                  Point end = combined.mapPoint(pts[1]);
                  float c1x = cur.x + 2.0f / 3.0f * (cp.x - cur.x);
                  float c1y = cur.y + 2.0f / 3.0f * (cp.y - cur.y);
                  float c2x = end.x + 2.0f / 3.0f * (cp.x - end.x);
                  float c2y = end.y + 2.0f / 3.0f * (cp.y - end.y);
                  _stream->curveTo(c1x, c1y, c2x, c2y, end.x, end.y);
                  cur = end;
                  break;
                }
                case PathVerb::Cubic: {
                  Point cp1 = combined.mapPoint(pts[0]);
                  Point cp2 = combined.mapPoint(pts[1]);
                  Point end = combined.mapPoint(pts[2]);
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
// PDFWriter – element list and layer writing
//==============================================================================

void PDFWriter::writeElements(const std::vector<Element*>& elements, const Matrix& transform) {
  auto fs = CollectFillStrokePDF(elements);

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
        Matrix groupMatrix = BuildGroupMatrixPDF(group);
        Matrix combined = transform * groupMatrix;

        bool hasAlpha = group->alpha < 1.0f;
        if (hasAlpha || !combined.isIdentity()) {
          _stream->save();
          if (!combined.isIdentity()) {
            _stream->concatMatrix(combined);
          }
          if (hasAlpha) {
            _stream->setExtGState(_resources->getExtGState(group->alpha, group->alpha));
          }
          writeElements(group->elements, {});
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
                    layer->x != 0.0f || layer->y != 0.0f;

  if (!needsGroup) {
    writeLayerContents(layer);
    for (const auto* child : layer->children) {
      writeLayer(child);
    }
    return;
  }

  _stream->save();

  Matrix layerMatrix = BuildLayerMatrixPDF(layer);
  if (!layerMatrix.isIdentity()) {
    _stream->concatMatrix(layerMatrix);
  }

  if (layer->alpha < 1.0f) {
    _stream->setExtGState(_resources->getExtGState(layer->alpha, layer->alpha));
  }

  if (layer->mask != nullptr) {
    FillRule clipRule = DetectMaskFillRule(layer->mask);
    writeClipPath(layer->mask);
    if (clipRule == FillRule::EvenOdd) {
      _stream->clipEvenOdd();
    } else {
      _stream->clip();
    }
    _stream->endPath();
  }

  writeLayerContents(layer);

  for (const auto* child : layer->children) {
    writeLayer(child);
  }

  _stream->restore();
}

//==============================================================================
// Main Export functions
//==============================================================================

std::string PDFExporter::ToPDF(const PAGXDocument& doc, const Options& options) {
  PDFObjectStore store;
  PDFResourceManager resources(&store);
  PDFStream stream;

  PDFWriter writer(&stream, &resources, options.convertTextToPath);

  // Apply page-level Y-flip so PAGX top-left origin maps to PDF bottom-left origin.
  stream.save();
  Matrix yFlip = {};
  yFlip.a = 1;
  yFlip.b = 0;
  yFlip.c = 0;
  yFlip.d = -1;
  yFlip.tx = 0;
  yFlip.ty = doc.height;
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
  store.set(pageId, "<< /Type /Page /Parent " + std::to_string(pagesId) + " 0 R /MediaBox [0 0 " +
                        FloatToString(doc.width) + " " + FloatToString(doc.height) +
                        "] /Contents " + std::to_string(contentId) + " 0 R /Resources " +
                        resourceDict + " >>");

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
