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

#include "pagx/SVGExporter.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
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
#include "pagx/types/Rect.h"
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
#include "pagx/svg/SVGBlendMode.h"
#include "pagx/svg/SVGPathParser.h"
#include "pagx/svg/SVGTextLayout.h"
#include "pagx/utils/Base64.h"
#include "pagx/utils/StringParser.h"

namespace pagx {

using pag::DegreesToRadians;
using pag::FloatNearlyZero;

//==============================================================================
// SVGBuilder - SVG generation helper
//==============================================================================

class SVGBuilder {
 public:
  explicit SVGBuilder(int indentSpaces, int initialIndentLevel = 0,
                      size_t reserveCapacity = 4096)
      : _indentLevel(initialIndentLevel), _indentSpaces(indentSpaces) {
    _tagStack.reserve(32);
    _buffer.reserve(reserveCapacity);
  }

  void appendDeclaration() {
    _buffer += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  }

  void openElement(const char* tag) {
    writeIndent();
    _buffer += "<";
    _buffer += tag;
    _tagStack.push_back(tag);
  }

  void addAttribute(const char* name, const char* value) {
    if (value && value[0] != '\0') {
      _buffer += " ";
      _buffer += name;
      _buffer += "=\"";
      _buffer += value;
      _buffer += "\"";
    }
  }

  void addAttribute(const char* name, const std::string& value) {
    if (!value.empty()) {
      _buffer += " ";
      _buffer += name;
      _buffer += "=\"";
      _buffer += escapeXML(value);
      _buffer += "\"";
    }
  }

  void addAttribute(const char* name, float value) {
    _buffer += " ";
    _buffer += name;
    _buffer += "=\"";
    _buffer += FloatToString(value);
    _buffer += "\"";
  }

  void addAttributeIfNonZero(const char* name, float value) {
    if (value != 0.0f) {
      addAttribute(name, value);
    }
  }

  void closeElementStart() {
    _buffer += ">\n";
    _indentLevel++;
  }

  void closeElementSelfClosing() {
    _buffer += "/>\n";
    _tagStack.pop_back();
  }

  void closeElement() {
    _indentLevel--;
    writeIndent();
    _buffer += "</";
    _buffer += _tagStack.back();
    _buffer += ">\n";
    _tagStack.pop_back();
  }

  void addTextContent(const std::string& text) {
    _buffer += escapeXML(text);
  }

  void addRawContent(const std::string& content) {
    _buffer += content;
  }

  void closeElementWithText(const std::string& text) {
    _buffer += ">";
    _buffer += escapeXML(text);
    _buffer += "</";
    _buffer += _tagStack.back();
    _buffer += ">\n";
    _tagStack.pop_back();
  }

  std::string release() {
    return std::move(_buffer);
  }

 private:
  std::string _buffer = {};
  std::vector<const char*> _tagStack = {};
  int _indentLevel = 0;
  int _indentSpaces = 2;

  void writeIndent() {
    _buffer.append(static_cast<size_t>(_indentLevel * _indentSpaces), ' ');
  }

  static std::string escapeXML(const std::string& input) {
    size_t extraSize = 0;
    for (char c : input) {
      switch (c) {
        case '&':
          extraSize += 4;
          break;
        case '<':
          extraSize += 3;
          break;
        case '>':
          extraSize += 3;
          break;
        case '"':
          extraSize += 5;
          break;
        case '\'':
          extraSize += 5;
          break;
        default:
          break;
      }
    }
    if (extraSize == 0) {
      return input;
    }
    std::string result;
    result.reserve(input.size() + extraSize);
    for (char c : input) {
      switch (c) {
        case '&':
          result += "&amp;";
          break;
        case '<':
          result += "&lt;";
          break;
        case '>':
          result += "&gt;";
          break;
        case '"':
          result += "&quot;";
          break;
        case '\'':
          result += "&apos;";
          break;
        default:
          result += c;
          break;
      }
    }
    return result;
  }
};

//==============================================================================
// Utility types and static helpers
//==============================================================================

struct FillStrokeInfo {
  const Fill* fill = nullptr;
  const Stroke* stroke = nullptr;
  const TextBox* textBox = nullptr;
};

// Returns only the RGB hex string (#RRGGBB). Alpha is handled separately via
// fill-opacity/stroke-opacity attributes, following standard SVG practice.
static std::string ColorToSVGString(const Color& color) {
  int r = std::clamp(static_cast<int>(std::round(color.red * 255.0f)), 0, 255);
  int g = std::clamp(static_cast<int>(std::round(color.green * 255.0f)), 0, 255);
  int b = std::clamp(static_cast<int>(std::round(color.blue * 255.0f)), 0, 255);
  char buf[8];
  snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
  return buf;
}

static std::string ColorToSVGStringWithAlpha(const Color& color, float* outAlpha) {
  if (outAlpha) {
    *outAlpha = color.alpha;
  }
  return ColorToSVGString(color);
}

// Emits a CSS color(display-p3 ...) value. Only used when the color source has
// Display P3 color space values; sRGB colors use the standard #RRGGBB format.
static std::string ColorToDisplayP3String(const Color& color) {
  return "color(display-p3 " + FloatToString(color.red) + " " + FloatToString(color.green) + " " +
         FloatToString(color.blue) + ")";
}

// Appends Display P3 color via CSS color() function when the color source has
// ColorSpace::DisplayP3. P3 colors are written conditionally based on the color space
// of each individual color source, not controlled by a global export option.
// The sRGB fallback is written first, followed by the P3 override, so browsers that
// don't support color() gracefully degrade to sRGB.
static void AppendP3ColorStyle(std::string& styleStr, const char* property,
                               const ColorSource* colorSource, const std::string& srgbHex,
                               float effectiveAlpha) {
  if (!colorSource || colorSource->nodeType() != NodeType::SolidColor) {
    return;
  }
  auto solid = static_cast<const SolidColor*>(colorSource);
  if (solid->color.colorSpace != ColorSpace::DisplayP3) {
    return;
  }
  styleStr += property;
  styleStr += ':';
  styleStr += srgbHex;
  styleStr += ';';
  styleStr += property;
  styleStr += ':';
  styleStr += ColorToDisplayP3String(solid->color);
  styleStr += ';';
  styleStr += property;
  styleStr += "-opacity:";
  styleStr += FloatToString(effectiveAlpha);
  styleStr += ';';
}

static std::string MatrixToSVGTransform(const Matrix& matrix) {
  if (matrix.isIdentity()) {
    return {};
  }
  // SVG matrix(a, b, c, d, e, f) matches the 2D affine matrix:
  // [a c e]   [scaleX skewX  transX]
  // [b d f] = [skewY  scaleY transY]
  // [0 0 1]   [0      0      1     ]
  std::string result;
  result.reserve(80);
  result += "matrix(";
  result += FloatToString(matrix.a);
  result += ',';
  result += FloatToString(matrix.b);
  result += ',';
  result += FloatToString(matrix.c);
  result += ',';
  result += FloatToString(matrix.d);
  result += ',';
  result += FloatToString(matrix.tx);
  result += ',';
  result += FloatToString(matrix.ty);
  result += ')';
  return result;
}

static bool GetPNGDimensions(const uint8_t* data, size_t size, int* width, int* height) {
  if (size < 24) {
    return false;
  }
  static const uint8_t kPNGSignature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
  if (memcmp(data, kPNGSignature, 8) != 0) {
    return false;
  }
  *width = static_cast<int>((data[16] << 24) | (data[17] << 16) | (data[18] << 8) | data[19]);
  *height = static_cast<int>((data[20] << 24) | (data[21] << 16) | (data[22] << 8) | data[23]);
  return *width > 0 && *height > 0;
}

static bool GetPNGDimensionsFromPath(const std::string& path, int* width, int* height) {
  if (path.rfind("data:", 0) == 0) {
    auto decoded = DecodeBase64DataURI(path);
    if (!decoded) {
      return false;
    }
    return GetPNGDimensions(decoded->bytes(), decoded->size(), width, height);
  }
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  uint8_t header[24];
  if (!file.read(reinterpret_cast<char*>(header), 24)) {
    return false;
  }
  return GetPNGDimensions(header, 24, width, height);
}

static bool GetImagePNGDimensions(const Image* image, int* width, int* height) {
  if (image->data) {
    return GetPNGDimensions(image->data->bytes(), image->data->size(), width, height);
  }
  if (!image->filePath.empty()) {
    return GetPNGDimensionsFromPath(image->filePath, width, height);
  }
  return false;
}

static std::string GetImageHref(const Image* image) {
  if (image->data) {
    return "data:image/png;base64," + Base64Encode(image->data->bytes(), image->data->size());
  }
  if (!image->filePath.empty()) {
    return image->filePath;
  }
  return {};
}

static FillStrokeInfo CollectFillStroke(const std::vector<Element*>& contents) {
  FillStrokeInfo info = {};
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

static std::string BuildLayerTransform(const Layer* layer) {
  if (layer->x != 0.0f || layer->y != 0.0f) {
    if (!layer->matrix.isIdentity()) {
      Matrix combined = Matrix::Translate(layer->x, layer->y) * layer->matrix;
      return MatrixToSVGTransform(combined);
    }
    return "translate(" + FloatToString(layer->x) + "," + FloatToString(layer->y) + ")";
  }
  if (!layer->matrix.isIdentity()) {
    return MatrixToSVGTransform(layer->matrix);
  }
  return "";
}

static std::string BuildGroupTransform(const Group* group) {
  bool hasAnchor = group->anchor.x != 0 || group->anchor.y != 0;
  bool hasPosition = group->position.x != 0 || group->position.y != 0;
  bool hasRotation = group->rotation != 0;
  bool hasScale = group->scale.x != 1 || group->scale.y != 1;
  bool hasSkew = group->skew != 0;

  if (!hasAnchor && !hasPosition && !hasRotation && !hasScale && !hasSkew) {
    return {};
  }

  // Transform order per PAGX spec:
  // 1. translate(-anchor) → 2. scale → 3. skew → 4. rotate → 5. translate(position)
  // Column-vector composition: M = T(pos) * R(rot) * Skew * S(scale) * T(-anchor)
  Matrix m = {};

  if (hasAnchor) {
    m = Matrix::Translate(-group->anchor.x, -group->anchor.y);
  }
  if (hasScale) {
    m = Matrix::Scale(group->scale.x, group->scale.y) * m;
  }
  if (hasSkew) {
    // Skew per spec: R(skewAxis) → ShearX(tan(skew)) → R(-skewAxis)
    // Column-vector: R(-skewAxis) * ShearX * R(skewAxis)
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

  return MatrixToSVGTransform(m);
}

//==============================================================================
// SVGWriterContext - shared state across SVGWriter instances
//==============================================================================

class SVGWriterContext {
 public:
  std::string generateId(const std::string& prefix) {
    return prefix + std::to_string(nextDefId++);
  }

 private:
  int nextDefId = 0;
};

//==============================================================================
// SVGWriter - encapsulates all SVG writing operations
//==============================================================================

class SVGWriter {
 public:
  SVGWriter(SVGBuilder* defs, SVGWriterContext* context) : _defs(defs), _context(context) {}

  void writeLayer(SVGBuilder& out, const Layer* layer);

 private:
  SVGBuilder* _defs;
  SVGWriterContext* _context;

  std::string generateId(const std::string& prefix) {
    return _context->generateId(prefix);
  }

  // Layer / element writing
  void writeLayerContents(SVGBuilder& out, const Layer* layer,
                          const std::string& transform = "");
  void writeElements(SVGBuilder& out, const std::vector<Element*>& elements,
                     const std::string& transform = "");

  // Shape writing
  void writeRectangle(SVGBuilder& out, const Rectangle* rect, const FillStrokeInfo& fs,
                      const std::string& transform = "");
  void writeEllipse(SVGBuilder& out, const Ellipse* ellipse, const FillStrokeInfo& fs,
                    const std::string& transform = "");
  void writePath(SVGBuilder& out, const Path* path, const FillStrokeInfo& fs,
                 const std::string& transform = "");
  void writeText(SVGBuilder& out, const Text* text, const FillStrokeInfo& fs,
                 const std::string& transform = "");

  // Gradient / pattern defs (write to _defs)
  void writeGradientStops(const std::vector<ColorStop*>& stops);
  void finishGradientDef(const Matrix& matrix, const std::vector<ColorStop*>& stops);
  void writeColorSourceDef(const ColorSource* source, const std::string& id);
  std::string writeImagePatternDef(const ImagePattern* pattern, const Rect& shapeBounds);
  std::string getColorSourceRef(const ColorSource* source, float* outAlpha,
                                const Rect& shapeBounds = {});

  // Filter defs (write to _defs)
  void writeShadowBlurAndOffset(float blurX, float blurY, float offsetX, float offsetY,
                                const std::string& blurResult, const std::string& offsetResult);
  void writeShadowColorMatrix(const Color& c, const std::string& inResult,
                              const std::string& outResult);
  std::string writeFilterDefs(const std::vector<LayerFilter*>& filters);

  // Mask / clip-path defs
  using ContentWriter = void (SVGWriter::*)(SVGBuilder&, const Layer*);
  std::string writeMaskOrClipDef(const Layer* maskLayer, const char* tag,
                                 const char* idPrefix, ContentWriter writer);
  void writeMaskContent(SVGBuilder& out, const Layer* layer);
  void writeClipPathContent(SVGBuilder& out, const Layer* layer);
  void writeClipPathContentRecursive(SVGBuilder& out, const Layer* layer,
                                     const std::string& parentTransform = "");
  std::string writeMaskDef(const Layer* maskLayer);
  std::string writeClipPathDef(const Layer* maskLayer);

  // Fill / stroke attribute helpers
  void applyFillAttributes(SVGBuilder& out, const Fill* fill, const Rect& shapeBounds = {},
                            std::string* p3Style = nullptr);
  void applyStrokeAttributes(SVGBuilder& out, const Stroke* stroke, const Rect& shapeBounds = {},
                              std::string* p3Style = nullptr);
  static void applyP3Style(SVGBuilder& out, const std::string& p3Style);
};

//==============================================================================
// SVGWriter – gradient / pattern defs
//==============================================================================

void SVGWriter::writeGradientStops(const std::vector<ColorStop*>& stops) {
  for (const auto* stop : stops) {
    _defs->openElement("stop");
    _defs->addAttribute("offset", FloatToString(stop->offset));
    float alpha = stop->color.alpha;
    std::string srgbHex = ColorToSVGString(stop->color);
    _defs->addAttribute("stop-color", srgbHex);
    if (alpha < 1.0f) {
      _defs->addAttribute("stop-opacity", FloatToString(alpha));
    }
    if (stop->color.colorSpace == ColorSpace::DisplayP3) {
      std::string style;
      style.reserve(120);
      style += "stop-color:";
      style += srgbHex;
      style += ";stop-color:";
      style += ColorToDisplayP3String(stop->color);
      style += ";stop-opacity:";
      style += FloatToString(alpha);
      style += ';';
      _defs->addAttribute("style", style);
    }
    _defs->closeElementSelfClosing();
  }
}

void SVGWriter::finishGradientDef(const Matrix& matrix, const std::vector<ColorStop*>& stops) {
  _defs->addAttribute("gradientUnits", "userSpaceOnUse");
  if (!matrix.isIdentity()) {
    _defs->addAttribute("gradientTransform", MatrixToSVGTransform(matrix));
  }
  if (stops.empty()) {
    _defs->closeElementSelfClosing();
  } else {
    _defs->closeElementStart();
    writeGradientStops(stops);
    _defs->closeElement();
  }
}

void SVGWriter::writeColorSourceDef(const ColorSource* source, const std::string& id) {
  switch (source->nodeType()) {
    case NodeType::LinearGradient: {
      auto grad = static_cast<const LinearGradient*>(source);
      _defs->openElement("linearGradient");
      _defs->addAttribute("id", id);
      _defs->addAttribute("x1", grad->startPoint.x);
      _defs->addAttribute("y1", grad->startPoint.y);
      _defs->addAttribute("x2", grad->endPoint.x);
      _defs->addAttribute("y2", grad->endPoint.y);
      finishGradientDef(grad->matrix, grad->colorStops);
      break;
    }
    case NodeType::RadialGradient: {
      auto grad = static_cast<const RadialGradient*>(source);
      _defs->openElement("radialGradient");
      _defs->addAttribute("id", id);
      _defs->addAttribute("cx", grad->center.x);
      _defs->addAttribute("cy", grad->center.y);
      _defs->addAttribute("r", grad->radius);
      finishGradientDef(grad->matrix, grad->colorStops);
      break;
    }
    default:
      break;
  }
}

std::string SVGWriter::writeImagePatternDef(const ImagePattern* pattern,
                                            const Rect& shapeBounds) {
  if (!pattern->image) {
    return {};
  }
  std::string href = GetImageHref(pattern->image);
  if (href.empty()) {
    return {};
  }

  std::string defId = generateId("pattern");
  bool needsTiling =
      (pattern->tileModeX == TileMode::Repeat || pattern->tileModeY == TileMode::Repeat);

  int imgW = 0;
  int imgH = 0;
  bool canUseOBB = false;
  if (!shapeBounds.isEmpty()) {
    canUseOBB = needsTiling ? GetImagePNGDimensions(pattern->image, &imgW, &imgH) : true;
  }

  _defs->openElement("pattern");
  _defs->addAttribute("id", defId);

  if (canUseOBB) {
    float W = shapeBounds.width;
    float H = shapeBounds.height;
    float X = shapeBounds.x;
    float Y = shapeBounds.y;
    const auto& M = pattern->matrix;
    Matrix obbMatrix = {};
    obbMatrix.a = M.a / W;
    obbMatrix.b = M.b / H;
    obbMatrix.c = M.c / W;
    obbMatrix.d = M.d / H;
    obbMatrix.tx = (M.tx - X) / W;
    obbMatrix.ty = (M.ty - Y) / H;

    _defs->addAttribute("patternContentUnits", "objectBoundingBox");
    if (needsTiling) {
      _defs->addAttribute("width", static_cast<float>(imgW) * obbMatrix.a);
      _defs->addAttribute("height", static_cast<float>(imgH) * obbMatrix.d);
    } else {
      _defs->addAttribute("width", 1.0f);
      _defs->addAttribute("height", 1.0f);
    }
    _defs->closeElementStart();
    _defs->openElement("image");
    _defs->addAttribute("href", href);
    _defs->addAttribute("transform", MatrixToSVGTransform(obbMatrix));
  } else {
    _defs->addAttribute("patternUnits", "userSpaceOnUse");
    _defs->addAttribute("width", "100%");
    _defs->addAttribute("height", "100%");
    _defs->closeElementStart();
    _defs->openElement("image");
    _defs->addAttribute("href", href);
    if (!pattern->matrix.isIdentity()) {
      _defs->addAttribute("transform", MatrixToSVGTransform(pattern->matrix));
    }
  }

  _defs->closeElementSelfClosing();
  _defs->closeElement();
  return "url(#" + defId + ")";
}

std::string SVGWriter::getColorSourceRef(const ColorSource* source, float* outAlpha,
                                         const Rect& shapeBounds) {
  if (!source) {
    return "none";
  }
  if (source->nodeType() == NodeType::SolidColor) {
    auto solid = static_cast<const SolidColor*>(source);
    return ColorToSVGStringWithAlpha(solid->color, outAlpha);
  }
  if (source->nodeType() == NodeType::LinearGradient ||
      source->nodeType() == NodeType::RadialGradient) {
    std::string defId = generateId("grad");
    writeColorSourceDef(source, defId);
    if (outAlpha) {
      *outAlpha = 1.0f;
    }
    return "url(#" + defId + ")";
  }
  if (source->nodeType() == NodeType::ImagePattern) {
    auto ref = writeImagePatternDef(static_cast<const ImagePattern*>(source), shapeBounds);
    if (!ref.empty()) {
      if (outAlpha) {
        *outAlpha = 1.0f;
      }
      return ref;
    }
  }
  return "none";
}

//==============================================================================
// SVGWriter – filter defs
//==============================================================================

void SVGWriter::writeShadowBlurAndOffset(float blurX, float blurY, float offsetX, float offsetY,
                                         const std::string& blurResult,
                                         const std::string& offsetResult) {
  _defs->openElement("feGaussianBlur");
  _defs->addAttribute("in", "SourceAlpha");
  std::string stdDev = FloatToString(blurX);
  if (blurX != blurY) {
    stdDev += " " + FloatToString(blurY);
  }
  _defs->addAttribute("stdDeviation", stdDev);
  _defs->addAttribute("result", blurResult);
  _defs->closeElementSelfClosing();

  _defs->openElement("feOffset");
  _defs->addAttribute("in", blurResult);
  _defs->addAttributeIfNonZero("dx", offsetX);
  _defs->addAttributeIfNonZero("dy", offsetY);
  _defs->addAttribute("result", offsetResult);
  _defs->closeElementSelfClosing();
}

void SVGWriter::writeShadowColorMatrix(const Color& c, const std::string& inResult,
                                       const std::string& outResult) {
  _defs->openElement("feColorMatrix");
  _defs->addAttribute("in", inResult);
  _defs->addAttribute("type", "matrix");
  std::string values;
  values.reserve(80);
  values += "0 0 0 0 ";
  values += FloatToString(c.red);
  values += " 0 0 0 0 ";
  values += FloatToString(c.green);
  values += " 0 0 0 0 ";
  values += FloatToString(c.blue);
  values += " 0 0 0 ";
  values += FloatToString(c.alpha);
  values += " 0";
  _defs->addAttribute("values", values);
  _defs->addAttribute("result", outResult);
  _defs->closeElementSelfClosing();
}

std::string SVGWriter::writeFilterDefs(const std::vector<LayerFilter*>& filters) {
  if (filters.empty()) {
    return {};
  }

  std::string filterId = generateId("filter");
  _defs->openElement("filter");
  _defs->addAttribute("id", filterId);
  _defs->addAttribute("x", "-50%");
  _defs->addAttribute("y", "-50%");
  _defs->addAttribute("width", "200%");
  _defs->addAttribute("height", "200%");
  _defs->addAttribute("color-interpolation-filters", "sRGB");
  _defs->closeElementStart();

  int shadowIndex = 0;
  std::vector<std::string> dropShadowResults;
  std::vector<std::string> innerShadowResults;
  bool needSourceGraphic = false;

  for (const auto* filter : filters) {
    switch (filter->nodeType()) {
      case NodeType::BlurFilter: {
        auto blur = static_cast<const BlurFilter*>(filter);
        _defs->openElement("feGaussianBlur");
        _defs->addAttribute("in", "SourceGraphic");
        std::string stdDev = FloatToString(blur->blurX);
        if (blur->blurX != blur->blurY) {
          stdDev += " " + FloatToString(blur->blurY);
        }
        _defs->addAttribute("stdDeviation", stdDev);
        _defs->closeElementSelfClosing();
        break;
      }
      case NodeType::DropShadowFilter: {
        auto shadow = static_cast<const DropShadowFilter*>(filter);
        std::string idx = std::to_string(shadowIndex++);
        std::string offsetResult = "shadowOffset" + idx;
        std::string shadowResult = "shadow" + idx;
        writeShadowBlurAndOffset(shadow->blurX, shadow->blurY, shadow->offsetX, shadow->offsetY,
                                 "shadowBlur" + idx, offsetResult);
        writeShadowColorMatrix(shadow->color, offsetResult, shadowResult);
        dropShadowResults.push_back(shadowResult);
        if (!shadow->shadowOnly) {
          needSourceGraphic = true;
        }
        break;
      }
      case NodeType::InnerShadowFilter: {
        auto shadow = static_cast<const InnerShadowFilter*>(filter);
        std::string idx = std::to_string(shadowIndex++);
        std::string offsetResult = "innerOffset" + idx;
        std::string compositeResult = "innerComposite" + idx;
        std::string shadowResult = "innerShadow" + idx;
        writeShadowBlurAndOffset(shadow->blurX, shadow->blurY, shadow->offsetX, shadow->offsetY,
                                 "innerBlur" + idx, offsetResult);

        _defs->openElement("feComposite");
        _defs->addAttribute("in", "SourceAlpha");
        _defs->addAttribute("in2", offsetResult);
        _defs->addAttribute("operator", "arithmetic");
        _defs->addAttribute("k2", "-1");
        _defs->addAttribute("k3", "1");
        _defs->addAttribute("result", compositeResult);
        _defs->closeElementSelfClosing();

        writeShadowColorMatrix(shadow->color, compositeResult, shadowResult);
        innerShadowResults.push_back(shadowResult);
        if (!shadow->shadowOnly) {
          needSourceGraphic = true;
        }
        break;
      }
      case NodeType::ColorMatrixFilter: {
        auto cm = static_cast<const ColorMatrixFilter*>(filter);
        _defs->openElement("feColorMatrix");
        _defs->addAttribute("in", "SourceGraphic");
        _defs->addAttribute("type", "matrix");
        std::string values;
        values.reserve(200);
        for (size_t i = 0; i < cm->matrix.size(); i++) {
          if (i > 0) {
            values += " ";
          }
          values += FloatToString(cm->matrix[i]);
        }
        _defs->addAttribute("values", values);
        _defs->closeElementSelfClosing();
        break;
      }
      case NodeType::BlendFilter: {
        auto blend = static_cast<const BlendFilter*>(filter);
        std::string idx = std::to_string(shadowIndex++);
        std::string floodResult = "blendFlood" + idx;
        _defs->openElement("feFlood");
        _defs->addAttribute("flood-color", ColorToSVGString(blend->color));
        if (blend->color.alpha < 1.0f) {
          _defs->addAttribute("flood-opacity", FloatToString(blend->color.alpha));
        }
        _defs->addAttribute("result", floodResult);
        _defs->closeElementSelfClosing();

        _defs->openElement("feBlend");
        _defs->addAttribute("in", "SourceGraphic");
        _defs->addAttribute("in2", floodResult);
        auto modeStr = BlendModeToSVGString(blend->blendMode);
        if (modeStr) {
          _defs->addAttribute("mode", modeStr);
        }
        _defs->closeElementSelfClosing();
        break;
      }
      default:
        break;
    }
  }

  bool hasShadows = !dropShadowResults.empty() || !innerShadowResults.empty();
  if (hasShadows) {
    bool multipleShadows = (dropShadowResults.size() + innerShadowResults.size()) > 1;
    if (needSourceGraphic || multipleShadows) {
      _defs->openElement("feMerge");
      _defs->closeElementStart();
      for (const auto& result : dropShadowResults) {
        _defs->openElement("feMergeNode");
        _defs->addAttribute("in", result);
        _defs->closeElementSelfClosing();
      }
      if (needSourceGraphic) {
        _defs->openElement("feMergeNode");
        _defs->addAttribute("in", "SourceGraphic");
        _defs->closeElementSelfClosing();
      }
      for (const auto& result : innerShadowResults) {
        _defs->openElement("feMergeNode");
        _defs->addAttribute("in", result);
        _defs->closeElementSelfClosing();
      }
      _defs->closeElement();
    }
  }

  _defs->closeElement();  // </filter>
  return filterId;
}

//==============================================================================
// SVGWriter – mask / clip-path defs
//==============================================================================

std::string SVGWriter::writeMaskOrClipDef(const Layer* maskLayer, const char* tag,
                                          const char* idPrefix, ContentWriter writer) {
  std::string defId = maskLayer->id.empty() ? generateId(idPrefix) : maskLayer->id;
  SVGBuilder paintDefs(2);
  _defs->openElement(tag);
  _defs->addAttribute("id", defId);
  _defs->closeElementStart();
  SVGWriter nestedWriter(&paintDefs, _context);
  (nestedWriter.*writer)(*_defs, maskLayer);
  _defs->closeElement();
  std::string paintDefsStr = paintDefs.release();
  if (!paintDefsStr.empty()) {
    _defs->addRawContent(paintDefsStr);
  }
  return defId;
}

void SVGWriter::writeMaskContent(SVGBuilder& out, const Layer* layer) {
  writeLayerContents(out, layer);
  for (const auto* child : layer->children) {
    writeLayer(out, child);
  }
}

void SVGWriter::writeClipPathContent(SVGBuilder& out, const Layer* layer) {
  writeClipPathContentRecursive(out, layer);
}

void SVGWriter::writeClipPathContentRecursive(SVGBuilder& out, const Layer* layer,
                                              const std::string& parentTransform) {
  std::string layerTransform = BuildLayerTransform(layer);
  std::string transform;
  if (!parentTransform.empty() && !layerTransform.empty()) {
    transform = parentTransform + " " + layerTransform;
  } else if (!parentTransform.empty()) {
    transform = parentTransform;
  } else {
    transform = layerTransform;
  }

  writeLayerContents(out, layer, transform);
  for (const auto* child : layer->children) {
    writeClipPathContentRecursive(out, child, transform);
  }
}

std::string SVGWriter::writeMaskDef(const Layer* maskLayer) {
  return writeMaskOrClipDef(maskLayer, "mask", "mask", &SVGWriter::writeMaskContent);
}

std::string SVGWriter::writeClipPathDef(const Layer* maskLayer) {
  return writeMaskOrClipDef(maskLayer, "clipPath", "clip", &SVGWriter::writeClipPathContent);
}

//==============================================================================
// SVGWriter – fill / stroke attribute helpers
//==============================================================================

void SVGWriter::applyFillAttributes(SVGBuilder& out, const Fill* fill, const Rect& shapeBounds,
                                    std::string* p3Style) {
  if (!fill) {
    out.addAttribute("fill", "none");
    return;
  }
  float alpha = 1.0f;
  std::string fillStr = getColorSourceRef(fill->color, &alpha, shapeBounds);
  out.addAttribute("fill", fillStr);
  float effectiveAlpha = alpha * fill->alpha;
  if (effectiveAlpha < 1.0f) {
    out.addAttribute("fill-opacity", FloatToString(effectiveAlpha));
  }
  if (fill->fillRule == FillRule::EvenOdd) {
    out.addAttribute("fill-rule", "evenodd");
  }
  if (p3Style) {
    AppendP3ColorStyle(*p3Style, "fill", fill->color, fillStr, effectiveAlpha);
  }
}

void SVGWriter::applyStrokeAttributes(SVGBuilder& out, const Stroke* stroke,
                                      const Rect& shapeBounds, std::string* p3Style) {
  if (!stroke) {
    return;
  }
  float alpha = 1.0f;
  std::string strokeStr = getColorSourceRef(stroke->color, &alpha, shapeBounds);
  out.addAttribute("stroke", strokeStr);
  float effectiveAlpha = alpha * stroke->alpha;
  if (effectiveAlpha < 1.0f) {
    out.addAttribute("stroke-opacity", FloatToString(effectiveAlpha));
  }
  if (stroke->width != 1.0f) {
    out.addAttribute("stroke-width", FloatToString(stroke->width));
  }
  if (stroke->cap == LineCap::Round) {
    out.addAttribute("stroke-linecap", "round");
  } else if (stroke->cap == LineCap::Square) {
    out.addAttribute("stroke-linecap", "square");
  }
  if (stroke->join == LineJoin::Round) {
    out.addAttribute("stroke-linejoin", "round");
  } else if (stroke->join == LineJoin::Bevel) {
    out.addAttribute("stroke-linejoin", "bevel");
  }
  if (stroke->join == LineJoin::Miter && stroke->miterLimit != 4.0f) {
    out.addAttribute("stroke-miterlimit", FloatToString(stroke->miterLimit));
  }
  if (!stroke->dashes.empty()) {
    std::string dashStr;
    for (size_t i = 0; i < stroke->dashes.size(); i++) {
      if (i > 0) {
        dashStr += ",";
      }
      dashStr += FloatToString(stroke->dashes[i]);
    }
    out.addAttribute("stroke-dasharray", dashStr);
  }
  if (stroke->dashOffset != 0.0f) {
    out.addAttribute("stroke-dashoffset", FloatToString(stroke->dashOffset));
  }
  if (p3Style) {
    AppendP3ColorStyle(*p3Style, "stroke", stroke->color, strokeStr, effectiveAlpha);
  }
}

void SVGWriter::applyP3Style(SVGBuilder& out, const std::string& p3Style) {
  if (!p3Style.empty()) {
    out.addAttribute("style", p3Style);
  }
}

//==============================================================================
// SVGWriter – shape elements
//==============================================================================

void SVGWriter::writeRectangle(SVGBuilder& out, const Rectangle* rect, const FillStrokeInfo& fs,
                               const std::string& transform) {
  float x = rect->center.x - rect->size.width / 2;
  float y = rect->center.y - rect->size.height / 2;
  out.openElement("rect");
  if (!transform.empty()) {
    out.addAttribute("transform", transform);
  }
  out.addAttributeIfNonZero("x", x);
  out.addAttributeIfNonZero("y", y);
  out.addAttribute("width", rect->size.width);
  out.addAttribute("height", rect->size.height);
  if (rect->roundness > 0) {
    out.addAttribute("rx", rect->roundness);
    out.addAttribute("ry", rect->roundness);
  }
  Rect bounds = Rect::MakeXYWH(x, y, rect->size.width, rect->size.height);
  std::string p3Style;
  applyFillAttributes(out, fs.fill, bounds, &p3Style);
  applyStrokeAttributes(out, fs.stroke, bounds, &p3Style);
  applyP3Style(out, p3Style);
  out.closeElementSelfClosing();
}

void SVGWriter::writeEllipse(SVGBuilder& out, const Ellipse* ellipse, const FillStrokeInfo& fs,
                             const std::string& transform) {
  float rx = ellipse->size.width / 2;
  float ry = ellipse->size.height / 2;
  if (FloatNearlyZero(rx - ry)) {
    out.openElement("circle");
    if (!transform.empty()) {
      out.addAttribute("transform", transform);
    }
    out.addAttribute("cx", ellipse->center.x);
    out.addAttribute("cy", ellipse->center.y);
    out.addAttribute("r", rx);
  } else {
    out.openElement("ellipse");
    if (!transform.empty()) {
      out.addAttribute("transform", transform);
    }
    out.addAttribute("cx", ellipse->center.x);
    out.addAttribute("cy", ellipse->center.y);
    out.addAttribute("rx", rx);
    out.addAttribute("ry", ry);
  }
  Rect bounds = Rect::MakeXYWH(ellipse->center.x - rx, ellipse->center.y - ry, rx * 2, ry * 2);
  std::string p3Style;
  applyFillAttributes(out, fs.fill, bounds, &p3Style);
  applyStrokeAttributes(out, fs.stroke, bounds, &p3Style);
  applyP3Style(out, p3Style);
  out.closeElementSelfClosing();
}

void SVGWriter::writePath(SVGBuilder& out, const Path* path, const FillStrokeInfo& fs,
                          const std::string& transform) {
  if (!path->data || path->data->isEmpty()) {
    return;
  }
  out.openElement("path");
  if (!transform.empty()) {
    out.addAttribute("transform", transform);
  }
  out.addAttribute("d", PathDataToSVGString(*path->data));
  Rect bounds = path->data->getBounds();
  std::string p3Style;
  applyFillAttributes(out, fs.fill, bounds, &p3Style);
  applyStrokeAttributes(out, fs.stroke, bounds, &p3Style);
  applyP3Style(out, p3Style);
  out.closeElementSelfClosing();
}

// ── writeText ───────────────────────────────────────────────────────────────

void SVGWriter::writeText(SVGBuilder& out, const Text* text, const FillStrokeInfo& fs,
                          const std::string& transform) {
  if (text->text.empty()) {
    return;
  }

  auto* textBox = fs.textBox;
  float fontSize = text->fontSize;
  bool needsMultiLine = textBox != nullptr && textBox->wordWrap && textBox->size.width > 0;

  // ── Multi-line: parse text and break into lines ──
  float boxWidth = 0;
  float lineHeight = 0;
  const std::string& content = text->text;
  std::vector<SVGCharInfo> chars;
  std::vector<SVGTextLine> lines;

  if (needsMultiLine) {
    boxWidth = textBox->size.width;
    lineHeight = textBox->lineHeight > 0 ? textBox->lineHeight : fontSize * 1.2f;

    chars.reserve(content.size());
    size_t pos = 0;
    while (pos < content.size()) {
      int32_t unichar = 0;
      size_t len = SVGDecodeUTF8Char(content.data() + pos, content.size() - pos, &unichar);
      if (len == 0) {
        pos++;
        continue;
      }
      float advance = 0;
      if (unichar != '\n') {
        advance = EstimateCharAdvance(unichar, fontSize) + text->letterSpacing;
      }
      chars.push_back({unichar, pos, len, advance});
      pos += len;
    }

    lines = BreakTextIntoLines(chars, boxWidth);

    while (lines.size() > 1 && lines.back().charCount == 0) {
      lines.pop_back();
    }
    if (lines.empty()) {
      return;
    }
  }

  // ── Compute position and anchor ──
  float x = text->position.x;
  float y = text->position.y;
  TextAnchor anchor = text->textAnchor;
  float firstLineY = 0;

  if (textBox) {
    switch (textBox->textAlign) {
      case TextAlign::Center:
        x = textBox->position.x + textBox->size.width / 2;
        anchor = TextAnchor::Center;
        break;
      case TextAlign::End:
        x = textBox->position.x + textBox->size.width;
        anchor = TextAnchor::End;
        break;
      default:
        x = textBox->position.x;
        anchor = TextAnchor::Start;
        break;
    }

    if (needsMultiLine) {
      float boxHeight = textBox->size.height;
      float totalHeight = static_cast<float>(lines.size()) * lineHeight;
      // Approximate ascent ratio for common fonts (ascender ≈ 80% of em-square).
      float ascent = fontSize * 0.8f;
      float yOffset = 0;
      if (boxHeight > 0) {
        switch (textBox->paragraphAlign) {
          case ParagraphAlign::Middle:
            yOffset = (boxHeight - totalHeight) / 2;
            break;
          case ParagraphAlign::Far:
            yOffset = boxHeight - totalHeight;
            break;
          default:
            break;
        }
      }
      firstLineY = textBox->position.y + yOffset + ascent;
    } else {
      switch (textBox->paragraphAlign) {
        case ParagraphAlign::Middle:
          // 0.35 ≈ (ascent - descent) / 2 / em, shifts baseline so text visually centers.
          y = textBox->position.y + textBox->size.height / 2 + fontSize * 0.35f;
          break;
        case ParagraphAlign::Far:
          y = textBox->position.y + textBox->size.height;
          break;
        default:
          y = textBox->position.y + fontSize;
          break;
      }
    }
  }

  // ── Open <text> element with shared attributes ──
  out.openElement("text");
  if (!transform.empty()) {
    out.addAttribute("transform", transform);
  }
  if (!needsMultiLine) {
    out.addAttributeIfNonZero("x", x);
    out.addAttributeIfNonZero("y", y);
  }
  if (!text->fontFamily.empty()) {
    out.addAttribute("font-family", text->fontFamily);
  }
  out.addAttribute("font-size", FloatToString(fontSize));
  if (text->letterSpacing != 0.0f) {
    out.addAttribute("letter-spacing", FloatToString(text->letterSpacing));
  }
  if (anchor == TextAnchor::Center) {
    out.addAttribute("text-anchor", "middle");
  } else if (anchor == TextAnchor::End) {
    out.addAttribute("text-anchor", "end");
  }
  if (!text->fontStyle.empty()) {
    bool hasBold = text->fontStyle.find("Bold") != std::string::npos;
    bool hasItalic = text->fontStyle.find("Italic") != std::string::npos;
    if (hasBold) {
      out.addAttribute("font-weight", "bold");
    }
    if (hasItalic) {
      out.addAttribute("font-style", "italic");
    }
  }
  std::string p3Style;
  applyFillAttributes(out, fs.fill, {}, &p3Style);
  applyStrokeAttributes(out, fs.stroke, {}, &p3Style);
  applyP3Style(out, p3Style);

  // ── Write content ──
  if (needsMultiLine) {
    out.closeElementStart();
    for (size_t i = 0; i < lines.size(); i++) {
      auto& line = lines[i];
      std::string lineText = ExtractLineText(content, chars, line);
      if (lineText.empty() && i < lines.size() - 1) {
        continue;
      }
      out.openElement("tspan");
      out.addAttribute("x", x);
      if (i == 0) {
        out.addAttribute("y", firstLineY);
      } else {
        out.addAttribute("dy", lineHeight);
      }
      out.closeElementWithText(lineText);
    }
    out.closeElement();
  } else {
    out.closeElementWithText(text->text);
  }
}

//==============================================================================
// SVGWriter – element list and layer writing
//==============================================================================

void SVGWriter::writeElements(SVGBuilder& out, const std::vector<Element*>& elements,
                              const std::string& transform) {
  auto fs = CollectFillStroke(elements);

  for (const auto* element : elements) {
    auto type = element->nodeType();
    if (type == NodeType::Fill || type == NodeType::Stroke || type == NodeType::TextBox) {
      continue;
    }
    switch (type) {
      case NodeType::Rectangle:
        writeRectangle(out, static_cast<const Rectangle*>(element), fs, transform);
        break;
      case NodeType::Ellipse:
        writeEllipse(out, static_cast<const Ellipse*>(element), fs, transform);
        break;
      case NodeType::Path:
        writePath(out, static_cast<const Path*>(element), fs, transform);
        break;
      case NodeType::Text:
        writeText(out, static_cast<const Text*>(element), fs, transform);
        break;
      case NodeType::Group: {
        auto* group = static_cast<const Group*>(element);
        std::string groupTransform = BuildGroupTransform(group);
        bool hasGroupTransform = !groupTransform.empty();
        bool hasAlpha = group->alpha < 1.0f;
        if (hasGroupTransform || hasAlpha) {
          out.openElement("g");
          std::string combinedTransform;
          if (!transform.empty() && hasGroupTransform) {
            combinedTransform = transform + " " + groupTransform;
          } else if (!transform.empty()) {
            combinedTransform = transform;
          } else {
            combinedTransform = groupTransform;
          }
          if (!combinedTransform.empty()) {
            out.addAttribute("transform", combinedTransform);
          }
          if (hasAlpha) {
            out.addAttribute("opacity", FloatToString(group->alpha));
          }
          out.closeElementStart();
          writeElements(out, group->elements, "");
          out.closeElement();
        } else {
          writeElements(out, group->elements, transform);
        }
        break;
      }
      default:
        break;
    }
  }
}

void SVGWriter::writeLayerContents(SVGBuilder& out, const Layer* layer,
                                   const std::string& transform) {
  writeElements(out, layer->contents, transform);
}

void SVGWriter::writeLayer(SVGBuilder& out, const Layer* layer) {
  if (!layer->visible && layer->mask == nullptr) {
    return;
  }

  bool needsGroup = !layer->matrix.isIdentity() || layer->alpha < 1.0f ||
                    !layer->id.empty() || !layer->filters.empty() || layer->mask != nullptr ||
                    layer->x != 0.0f || layer->y != 0.0f || !layer->customData.empty() ||
                    layer->blendMode != BlendMode::Normal;

  if (!needsGroup) {
    writeLayerContents(out, layer);
    for (const auto* child : layer->children) {
      writeLayer(out, child);
    }
    return;
  }

  out.openElement("g");

  if (!layer->id.empty()) {
    out.addAttribute("id", layer->id);
  }

  for (const auto& [key, value] : layer->customData) {
    out.addAttribute(("data-" + key).c_str(), value);
  }

  std::string transform = BuildLayerTransform(layer);
  if (!transform.empty()) {
    out.addAttribute("transform", transform);
  }

  if (layer->alpha < 1.0f) {
    out.addAttribute("opacity", FloatToString(layer->alpha));
  }

  if (layer->blendMode != BlendMode::Normal) {
    auto blendStr = BlendModeToSVGString(layer->blendMode);
    if (blendStr) {
      out.addAttribute("style", std::string("mix-blend-mode:") + blendStr);
    }
  }

  if (!layer->filters.empty()) {
    auto filterId = writeFilterDefs(layer->filters);
    if (!filterId.empty()) {
      out.addAttribute("filter", "url(#" + filterId + ")");
    }
  }

  if (layer->mask != nullptr) {
    if (layer->maskType == MaskType::Contour) {
      // Contour masking uses <clipPath> which only supports geometry clipping.
      auto clipId = writeClipPathDef(layer->mask);
      out.addAttribute("clip-path", "url(#" + clipId + ")");
    } else {
      // Alpha and Luminance masking use <mask> which supports alpha channel.
      auto maskId = writeMaskDef(layer->mask);
      out.addAttribute("mask", "url(#" + maskId + ")");
    }
  }

  bool hasContent = !layer->contents.empty() || !layer->children.empty();
  if (!hasContent) {
    out.closeElementSelfClosing();
    return;
  }

  out.closeElementStart();

  writeLayerContents(out, layer);

  for (const auto* child : layer->children) {
    writeLayer(out, child);
  }

  out.closeElement();
}

//==============================================================================
// Main Export function
//==============================================================================

std::string SVGExporter::ToSVG(const PAGXDocument& doc, const Options& options) {
  SVGBuilder svg(options.indent);
  SVGBuilder defs(options.indent, 2);
  SVGWriterContext context;
  SVGWriter writer(&defs, &context);

  if (options.xmlDeclaration) {
    svg.appendDeclaration();
  }

  svg.openElement("svg");
  svg.addAttribute("xmlns", "http://www.w3.org/2000/svg");
  svg.addAttribute("width", doc.width);
  svg.addAttribute("height", doc.height);
  std::string viewBox = "0 0 " + FloatToString(doc.width) + " " + FloatToString(doc.height);
  svg.addAttribute("viewBox", viewBox);
  svg.closeElementStart();

  SVGBuilder bodyContent(options.indent, 1);
  for (const auto* layer : doc.layers) {
    writer.writeLayer(bodyContent, layer);
  }

  std::string defsStr = defs.release();
  if (!defsStr.empty()) {
    svg.openElement("defs");
    svg.closeElementStart();
    svg.addRawContent(defsStr);
    svg.closeElement();
  }

  svg.addRawContent(bodyContent.release());

  svg.closeElement();  // </svg>

  return svg.release();
}

bool SVGExporter::ToFile(const PAGXDocument& document, const std::string& filePath,
                         const Options& options) {
  auto svgContent = ToSVG(document, options);
  if (svgContent.empty()) {
    return false;
  }
  std::ofstream file(filePath, std::ios::binary);
  if (!file) {
    return false;
  }
  file.write(svgContent.data(), static_cast<std::streamsize>(svgContent.size()));
  return file.good();
}

}  // namespace pagx
