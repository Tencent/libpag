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
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/BlurFilter.h"
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
#include "pagx/svg/SVGPathParser.h"
#include "pagx/utils/Base64.h"
#include "pagx/utils/StringParser.h"

namespace pagx {

//==============================================================================
// SVGBuilder - SVG generation helper
//==============================================================================

class SVGBuilder {
 public:
  explicit SVGBuilder(int indentSpaces) : _indentSpaces(indentSpaces) {
    _tagStack.reserve(32);
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
// SVGExportContext - tracks defs and generates unique IDs
//==============================================================================

struct SVGExportContext {
  int nextDefId = 0;
  // Deferred <defs> content that will be written after collecting all references.
  std::string defsContent = {};

  std::string generateId(const std::string& prefix) {
    return prefix + std::to_string(nextDefId++);
  }
};

//==============================================================================
// Forward declarations
//==============================================================================

struct FillStrokeInfo {
  const Fill* fill = nullptr;
  const Stroke* stroke = nullptr;
  const TextBox* textBox = nullptr;
};

static std::string colorToSVGString(const Color& color);
static std::string colorToSVGStringWithAlpha(const Color& color, float* outAlpha);
static std::string matrixToSVGTransform(const Matrix& matrix);
static void writeColorSourceDef(SVGBuilder& defs, const ColorSource* source, const std::string& id,
                                SVGExportContext& ctx);
static std::string getColorSourceRef(const ColorSource* source, float* outAlpha,
                                     SVGExportContext& ctx, SVGBuilder& defs,
                                     const Rect& shapeBounds = {});
static void writeLayerContents(SVGBuilder& svg, const Layer* layer, SVGExportContext& ctx,
                               SVGBuilder& defs, const std::string& transform = "");
static void writeLayer(SVGBuilder& svg, const Layer* layer, SVGExportContext& ctx,
                       SVGBuilder& defs);

//==============================================================================
// Color conversion helpers
//==============================================================================

static std::string colorToSVGString(const Color& color) {
  int r = static_cast<int>(std::round(color.red * 255.0f));
  int g = static_cast<int>(std::round(color.green * 255.0f));
  int b = static_cast<int>(std::round(color.blue * 255.0f));
  r = std::max(0, std::min(255, r));
  g = std::max(0, std::min(255, g));
  b = std::max(0, std::min(255, b));
  char buf[8];
  snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
  return buf;
}

static std::string colorToSVGStringWithAlpha(const Color& color, float* outAlpha) {
  if (outAlpha) {
    *outAlpha = color.alpha;
  }
  return colorToSVGString(color);
}

//==============================================================================
// Matrix conversion: PAGX Matrix → SVG transform attribute
//==============================================================================

static std::string matrixToSVGTransform(const Matrix& matrix) {
  if (matrix.isIdentity()) {
    return {};
  }
  // SVG matrix(a, b, c, d, e, f) matches the 2D affine matrix:
  // [a c e]   [scaleX skewX  transX]
  // [b d f] = [skewY  scaleY transY]
  // [0 0 1]   [0      0      1     ]
  return "matrix(" + FloatToString(matrix.a) + "," + FloatToString(matrix.b) + "," +
         FloatToString(matrix.c) + "," + FloatToString(matrix.d) + "," +
         FloatToString(matrix.tx) + "," + FloatToString(matrix.ty) + ")";
}

//==============================================================================
// Write gradient/pattern <defs>
//==============================================================================

static void writeGradientStops(SVGBuilder& svg, const std::vector<ColorStop*>& stops) {
  for (const auto* stop : stops) {
    svg.openElement("stop");
    svg.addAttribute("offset", FloatToString(stop->offset));
    float alpha = stop->color.alpha;
    svg.addAttribute("stop-color", colorToSVGString(stop->color));
    if (alpha < 1.0f) {
      svg.addAttribute("stop-opacity", FloatToString(alpha));
    }
    svg.closeElementSelfClosing();
  }
}

static void writeColorSourceDef(SVGBuilder& defs, const ColorSource* source, const std::string& id,
                                SVGExportContext& /*ctx*/) {
  switch (source->nodeType()) {
    case NodeType::LinearGradient: {
      auto grad = static_cast<const LinearGradient*>(source);
      defs.openElement("linearGradient");
      defs.addAttribute("id", id);
      defs.addAttribute("x1", grad->startPoint.x);
      defs.addAttribute("y1", grad->startPoint.y);
      defs.addAttribute("x2", grad->endPoint.x);
      defs.addAttribute("y2", grad->endPoint.y);
      defs.addAttribute("gradientUnits", std::string("userSpaceOnUse"));
      if (!grad->matrix.isIdentity()) {
        defs.addAttribute("gradientTransform", matrixToSVGTransform(grad->matrix));
      }
      if (grad->colorStops.empty()) {
        defs.closeElementSelfClosing();
      } else {
        defs.closeElementStart();
        writeGradientStops(defs, grad->colorStops);
        defs.closeElement();
      }
      break;
    }
    case NodeType::RadialGradient: {
      auto grad = static_cast<const RadialGradient*>(source);
      defs.openElement("radialGradient");
      defs.addAttribute("id", id);
      defs.addAttribute("cx", grad->center.x);
      defs.addAttribute("cy", grad->center.y);
      defs.addAttribute("r", grad->radius);
      defs.addAttribute("gradientUnits", std::string("userSpaceOnUse"));
      if (!grad->matrix.isIdentity()) {
        defs.addAttribute("gradientTransform", matrixToSVGTransform(grad->matrix));
      }
      if (grad->colorStops.empty()) {
        defs.closeElementSelfClosing();
      } else {
        defs.closeElementStart();
        writeGradientStops(defs, grad->colorStops);
        defs.closeElement();
      }
      break;
    }
    default:
      break;
  }
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
  if (path.find("data:") == 0) {
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

static std::string getColorSourceRef(const ColorSource* source, float* outAlpha,
                                     SVGExportContext& ctx, SVGBuilder& defs,
                                     const Rect& shapeBounds) {
  if (!source) {
    return "none";
  }
  if (source->nodeType() == NodeType::SolidColor) {
    auto solid = static_cast<const SolidColor*>(source);
    return colorToSVGStringWithAlpha(solid->color, outAlpha);
  }
  if (source->nodeType() == NodeType::LinearGradient ||
      source->nodeType() == NodeType::RadialGradient) {
    std::string defId = ctx.generateId("grad");
    writeColorSourceDef(defs, source, defId, ctx);
    if (outAlpha) {
      *outAlpha = 1.0f;
    }
    return "url(#" + defId + ")";
  }
  if (source->nodeType() == NodeType::ImagePattern) {
    auto pattern = static_cast<const ImagePattern*>(source);
    if (pattern->image) {
      std::string href;
      if (pattern->image->data) {
        href = "data:image/png;base64," +
               Base64Encode(pattern->image->data->bytes(), pattern->image->data->size());
      } else if (!pattern->image->filePath.empty()) {
        href = pattern->image->filePath;
      }
      if (!href.empty()) {
        std::string defId = ctx.generateId("pattern");
        bool needsTiling = (pattern->tileModeX == TileMode::Repeat ||
                            pattern->tileModeY == TileMode::Repeat);
        int imgW = 0;
        int imgH = 0;
        bool canUseOBB = false;
        if (needsTiling && !shapeBounds.isEmpty()) {
          if (pattern->image->data) {
            canUseOBB = GetPNGDimensions(pattern->image->data->bytes(),
                                         pattern->image->data->size(), &imgW, &imgH);
          } else if (!pattern->image->filePath.empty()) {
            canUseOBB = GetPNGDimensionsFromPath(pattern->image->filePath, &imgW, &imgH);
          }
        }

        defs.openElement("pattern");
        defs.addAttribute("id", defId);
        defs.addAttribute("a", canUseOBB);
        if (canUseOBB) {
          // Convert forward matrix (image pixels → screen) to objectBoundingBox space (0-1).
          // imageToOBB = Scale(1/W, 1/H) * Translate(-X, -Y) * forwardMatrix
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

          float tileW = static_cast<float>(imgW) * obbMatrix.a;
          float tileH = static_cast<float>(imgH) * obbMatrix.d;

          defs.addAttribute("patternContentUnits", std::string("objectBoundingBox"));
          defs.addAttribute("width", tileW);
          defs.addAttribute("height", tileH);
          defs.closeElementStart();
          defs.openElement("image");
          defs.addAttribute("href", href);
          defs.addAttribute("transform", matrixToSVGTransform(obbMatrix));
        } else {
          defs.addAttribute("patternUnits", std::string("userSpaceOnUse"));
          defs.addAttribute("width", std::string("100%"));
          defs.addAttribute("height", std::string("100%"));
          defs.closeElementStart();
          defs.openElement("image");
          defs.addAttribute("href", href);
          if (!pattern->matrix.isIdentity()) {
            defs.addAttribute("transform", matrixToSVGTransform(pattern->matrix));
          }
        }
        defs.closeElementSelfClosing();
        defs.closeElement();
        if (outAlpha) {
          *outAlpha = 1.0f;
        }
        return "url(#" + defId + ")";
      }
    }
  }
  return "none";
}

//==============================================================================
// Write SVG filter defs
//==============================================================================

static std::string writeFilterDefs(SVGBuilder& defs, const std::vector<LayerFilter*>& filters,
                                   SVGExportContext& ctx) {
  if (filters.empty()) {
    return {};
  }

  std::string filterId = ctx.generateId("filter");
  defs.openElement("filter");
  defs.addAttribute("id", filterId);
  defs.addAttribute("x", std::string("-50%"));
  defs.addAttribute("y", std::string("-50%"));
  defs.addAttribute("width", std::string("200%"));
  defs.addAttribute("height", std::string("200%"));
  defs.addAttribute("color-interpolation-filters", std::string("sRGB"));
  defs.closeElementStart();

  int shadowIndex = 0;
  std::vector<std::string> dropShadowResults;
  std::vector<std::string> innerShadowResults;
  bool needSourceGraphic = false;

  for (const auto* filter : filters) {
    switch (filter->nodeType()) {
      case NodeType::BlurFilter: {
        auto blur = static_cast<const BlurFilter*>(filter);
        defs.openElement("feGaussianBlur");
        defs.addAttribute("in", std::string("SourceGraphic"));
        std::string stdDev = FloatToString(blur->blurX);
        if (blur->blurX != blur->blurY) {
          stdDev += " " + FloatToString(blur->blurY);
        }
        defs.addAttribute("stdDeviation", stdDev);
        defs.closeElementSelfClosing();
        break;
      }
      case NodeType::DropShadowFilter: {
        auto shadow = static_cast<const DropShadowFilter*>(filter);
        std::string idx = std::to_string(shadowIndex++);
        std::string blurResult = "shadowBlur" + idx;
        std::string offsetResult = "shadowOffset" + idx;
        std::string shadowResult = "shadow" + idx;

        defs.openElement("feGaussianBlur");
        defs.addAttribute("in", std::string("SourceAlpha"));
        std::string stdDev = FloatToString(shadow->blurX);
        if (shadow->blurX != shadow->blurY) {
          stdDev += " " + FloatToString(shadow->blurY);
        }
        defs.addAttribute("stdDeviation", stdDev);
        defs.addAttribute("result", blurResult);
        defs.closeElementSelfClosing();

        defs.openElement("feOffset");
        defs.addAttribute("in", blurResult);
        defs.addAttributeIfNonZero("dx", shadow->offsetX);
        defs.addAttributeIfNonZero("dy", shadow->offsetY);
        defs.addAttribute("result", offsetResult);
        defs.closeElementSelfClosing();

        defs.openElement("feColorMatrix");
        defs.addAttribute("in", offsetResult);
        defs.addAttribute("type", std::string("matrix"));
        auto& c = shadow->color;
        std::string matrixValues = "0 0 0 0 " + FloatToString(c.red) + " " +
                                   "0 0 0 0 " + FloatToString(c.green) + " " +
                                   "0 0 0 0 " + FloatToString(c.blue) + " " +
                                   "0 0 0 " + FloatToString(c.alpha) + " 0";
        defs.addAttribute("values", matrixValues);
        defs.addAttribute("result", shadowResult);
        defs.closeElementSelfClosing();

        dropShadowResults.push_back(shadowResult);
        if (!shadow->shadowOnly) {
          needSourceGraphic = true;
        }
        break;
      }
      case NodeType::InnerShadowFilter: {
        auto shadow = static_cast<const InnerShadowFilter*>(filter);
        std::string idx = std::to_string(shadowIndex++);
        std::string blurResult = "innerBlur" + idx;
        std::string offsetResult = "innerOffset" + idx;
        std::string compositeResult = "innerComposite" + idx;
        std::string shadowResult = "innerShadow" + idx;

        defs.openElement("feGaussianBlur");
        defs.addAttribute("in", std::string("SourceAlpha"));
        std::string stdDev = FloatToString(shadow->blurX);
        if (shadow->blurX != shadow->blurY) {
          stdDev += " " + FloatToString(shadow->blurY);
        }
        defs.addAttribute("stdDeviation", stdDev);
        defs.addAttribute("result", blurResult);
        defs.closeElementSelfClosing();

        defs.openElement("feOffset");
        defs.addAttribute("in", blurResult);
        defs.addAttributeIfNonZero("dx", shadow->offsetX);
        defs.addAttributeIfNonZero("dy", shadow->offsetY);
        defs.addAttribute("result", offsetResult);
        defs.closeElementSelfClosing();

        defs.openElement("feComposite");
        defs.addAttribute("in", std::string("SourceAlpha"));
        defs.addAttribute("in2", offsetResult);
        defs.addAttribute("operator", std::string("arithmetic"));
        defs.addAttribute("k2", std::string("-1"));
        defs.addAttribute("k3", std::string("1"));
        defs.addAttribute("result", compositeResult);
        defs.closeElementSelfClosing();

        defs.openElement("feColorMatrix");
        defs.addAttribute("in", compositeResult);
        defs.addAttribute("type", std::string("matrix"));
        auto& c = shadow->color;
        std::string matrixValues =
            "0 0 0 0 " + FloatToString(c.red) + " " +
            "0 0 0 0 " + FloatToString(c.green) + " " +
            "0 0 0 0 " + FloatToString(c.blue) + " " +
            "0 0 0 " + FloatToString(c.alpha) + " 0";
        defs.addAttribute("values", matrixValues);
        defs.addAttribute("result", shadowResult);
        defs.closeElementSelfClosing();

        innerShadowResults.push_back(shadowResult);
        if (!shadow->shadowOnly) {
          needSourceGraphic = true;
        }
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
      defs.openElement("feMerge");
      defs.closeElementStart();
      for (const auto& result : dropShadowResults) {
        defs.openElement("feMergeNode");
        defs.addAttribute("in", result);
        defs.closeElementSelfClosing();
      }
      if (needSourceGraphic) {
        defs.openElement("feMergeNode");
        defs.addAttribute("in", std::string("SourceGraphic"));
        defs.closeElementSelfClosing();
      }
      for (const auto& result : innerShadowResults) {
        defs.openElement("feMergeNode");
        defs.addAttribute("in", result);
        defs.closeElementSelfClosing();
      }
      defs.closeElement();
    }
  }

  defs.closeElement();  // </filter>
  return filterId;
}

//==============================================================================
// Collect fill/stroke info from a layer's contents
//==============================================================================

static FillStrokeInfo collectFillStroke(const std::vector<Element*>& contents) {
  FillStrokeInfo info = {};
  for (const auto* element : contents) {
    if (element->nodeType() == NodeType::Fill && !info.fill) {
      info.fill = static_cast<const Fill*>(element);
    } else if (element->nodeType() == NodeType::Stroke && !info.stroke) {
      info.stroke = static_cast<const Stroke*>(element);
    } else if (element->nodeType() == NodeType::TextBox && !info.textBox) {
      info.textBox = static_cast<const TextBox*>(element);
    }
  }
  return info;
}

//==============================================================================
// Write mask defs
//==============================================================================

static std::string writeMaskDef(SVGBuilder& defs, const Layer* maskLayer, SVGExportContext& ctx,
                                SVGBuilder& nestedDefs);

static void writeMaskLayerContent(SVGBuilder& defs, const Layer* layer, SVGExportContext& ctx,
                                  SVGBuilder& nestedDefs) {
  writeLayerContents(defs, layer, ctx, nestedDefs);
  for (const auto* child : layer->children) {
    writeLayer(defs, child, ctx, nestedDefs);
  }
}

static std::string writeMaskDef(SVGBuilder& defs, const Layer* maskLayer, SVGExportContext& ctx,
                                SVGBuilder& nestedDefs) {
  std::string maskId = maskLayer->id.empty() ? ctx.generateId("mask") : maskLayer->id;
  defs.openElement("mask");
  defs.addAttribute("id", maskId);
  defs.closeElementStart();
  writeMaskLayerContent(defs, maskLayer, ctx, nestedDefs);
  defs.closeElement();
  return maskId;
}

static std::string buildLayerTransform(const Layer* layer) {
  if (layer->x != 0.0f || layer->y != 0.0f) {
    if (!layer->matrix.isIdentity()) {
      Matrix combined = Matrix::Translate(layer->x, layer->y) * layer->matrix;
      return matrixToSVGTransform(combined);
    }
    return "translate(" + FloatToString(layer->x) + "," + FloatToString(layer->y) + ")";
  }
  if (!layer->matrix.isIdentity()) {
    return matrixToSVGTransform(layer->matrix);
  }
  return "";
}

static void writeClipPathLayerContent(SVGBuilder& svg, const Layer* layer, SVGExportContext& ctx,
                                      SVGBuilder& defs,
                                      const std::string& parentTransform = "") {
  std::string layerTransform = buildLayerTransform(layer);
  std::string transform;
  if (!parentTransform.empty() && !layerTransform.empty()) {
    transform = parentTransform + " " + layerTransform;
  } else if (!parentTransform.empty()) {
    transform = parentTransform;
  } else {
    transform = layerTransform;
  }

  writeLayerContents(svg, layer, ctx, defs, transform);
  for (const auto* child : layer->children) {
    writeClipPathLayerContent(svg, child, ctx, defs, transform);
  }
}

static std::string writeClipPathDef(SVGBuilder& defs, const Layer* maskLayer,
                                    SVGExportContext& ctx, SVGBuilder& nestedDefs) {
  std::string clipId = maskLayer->id.empty() ? ctx.generateId("clip") : maskLayer->id;
  defs.openElement("clipPath");
  defs.addAttribute("id", clipId);
  defs.closeElementStart();
  writeClipPathLayerContent(defs, maskLayer, ctx, nestedDefs);
  defs.closeElement();
  return clipId;
}

static void applyFillAttributes(SVGBuilder& svg, const Fill* fill, SVGExportContext& ctx,
                                SVGBuilder& defs, const Rect& shapeBounds = {}) {
  if (!fill) {
    svg.addAttribute("fill", std::string("none"));
    return;
  }
  float alpha = 1.0f;
  std::string fillStr = getColorSourceRef(fill->color, &alpha, ctx, defs, shapeBounds);
  svg.addAttribute("fill", fillStr);
  float effectiveAlpha = alpha * fill->alpha;
  if (effectiveAlpha < 1.0f) {
    svg.addAttribute("fill-opacity", FloatToString(effectiveAlpha));
  }
  if (fill->fillRule == FillRule::EvenOdd) {
    svg.addAttribute("fill-rule", std::string("evenodd"));
  }
}

static void applyStrokeAttributes(SVGBuilder& svg, const Stroke* stroke, SVGExportContext& ctx,
                                  SVGBuilder& defs, const Rect& shapeBounds = {}) {
  if (!stroke) {
    return;
  }
  float alpha = 1.0f;
  std::string strokeStr = getColorSourceRef(stroke->color, &alpha, ctx, defs, shapeBounds);
  svg.addAttribute("stroke", strokeStr);
  float effectiveAlpha = alpha * stroke->alpha;
  if (effectiveAlpha < 1.0f) {
    svg.addAttribute("stroke-opacity", FloatToString(effectiveAlpha));
  }
  if (stroke->width != 1.0f) {
    svg.addAttribute("stroke-width", FloatToString(stroke->width));
  }
  if (stroke->cap == LineCap::Round) {
    svg.addAttribute("stroke-linecap", std::string("round"));
  } else if (stroke->cap == LineCap::Square) {
    svg.addAttribute("stroke-linecap", std::string("square"));
  }
  if (stroke->join == LineJoin::Round) {
    svg.addAttribute("stroke-linejoin", std::string("round"));
  } else if (stroke->join == LineJoin::Bevel) {
    svg.addAttribute("stroke-linejoin", std::string("bevel"));
  }
  if (stroke->join == LineJoin::Miter && stroke->miterLimit != 4.0f) {
    svg.addAttribute("stroke-miterlimit", FloatToString(stroke->miterLimit));
  }
  if (!stroke->dashes.empty()) {
    std::string dashStr;
    for (size_t i = 0; i < stroke->dashes.size(); i++) {
      if (i > 0) {
        dashStr += ",";
      }
      dashStr += FloatToString(stroke->dashes[i]);
    }
    svg.addAttribute("stroke-dasharray", dashStr);
  }
  if (stroke->dashOffset != 0.0f) {
    svg.addAttribute("stroke-dashoffset", FloatToString(stroke->dashOffset));
  }
}

//==============================================================================
// Write individual SVG shape elements
//==============================================================================

static void writeRectangle(SVGBuilder& svg, const Rectangle* rect, const FillStrokeInfo& fs,
                           SVGExportContext& ctx, SVGBuilder& defs,
                           const std::string& transform = "") {
  float x = rect->center.x - rect->size.width / 2;
  float y = rect->center.y - rect->size.height / 2;
  svg.openElement("rect");
  if (!transform.empty()) {
    svg.addAttribute("transform", transform);
  }
  svg.addAttributeIfNonZero("x", x);
  svg.addAttributeIfNonZero("y", y);
  svg.addAttribute("width", rect->size.width);
  svg.addAttribute("height", rect->size.height);
  if (rect->roundness > 0) {
    svg.addAttribute("rx", rect->roundness);
    svg.addAttribute("ry", rect->roundness);
  }
  Rect bounds = Rect::MakeXYWH(x, y, rect->size.width, rect->size.height);
  applyFillAttributes(svg, fs.fill, ctx, defs, bounds);
  applyStrokeAttributes(svg, fs.stroke, ctx, defs, bounds);
  svg.closeElementSelfClosing();
}

static void writeEllipse(SVGBuilder& svg, const Ellipse* ellipse, const FillStrokeInfo& fs,
                         SVGExportContext& ctx, SVGBuilder& defs,
                         const std::string& transform = "") {
  float rx = ellipse->size.width / 2;
  float ry = ellipse->size.height / 2;
  if (std::abs(rx - ry) < 0.001f) {
    svg.openElement("circle");
    if (!transform.empty()) {
      svg.addAttribute("transform", transform);
    }
    svg.addAttribute("cx", ellipse->center.x);
    svg.addAttribute("cy", ellipse->center.y);
    svg.addAttribute("r", rx);
  } else {
    svg.openElement("ellipse");
    if (!transform.empty()) {
      svg.addAttribute("transform", transform);
    }
    svg.addAttribute("cx", ellipse->center.x);
    svg.addAttribute("cy", ellipse->center.y);
    svg.addAttribute("rx", rx);
    svg.addAttribute("ry", ry);
  }
  Rect bounds = Rect::MakeXYWH(ellipse->center.x - rx, ellipse->center.y - ry, rx * 2, ry * 2);
  applyFillAttributes(svg, fs.fill, ctx, defs, bounds);
  applyStrokeAttributes(svg, fs.stroke, ctx, defs, bounds);
  svg.closeElementSelfClosing();
}

static void writePath(SVGBuilder& svg, const Path* path, const FillStrokeInfo& fs,
                      SVGExportContext& ctx, SVGBuilder& defs,
                      const std::string& transform = "") {
  if (!path->data || path->data->isEmpty()) {
    return;
  }
  svg.openElement("path");
  if (!transform.empty()) {
    svg.addAttribute("transform", transform);
  }
  svg.addAttribute("d", PathDataToSVGString(*path->data));
  Rect bounds = path->data->getBounds();
  applyFillAttributes(svg, fs.fill, ctx, defs, bounds);
  applyStrokeAttributes(svg, fs.stroke, ctx, defs, bounds);
  svg.closeElementSelfClosing();
}

static void writeText(SVGBuilder& svg, const Text* text, const FillStrokeInfo& fs,
                      SVGExportContext& ctx, SVGBuilder& defs,
                      const std::string& transform = "") {
  if (text->text.empty()) {
    return;
  }

  float x = text->position.x;
  float y = text->position.y;
  TextAnchor anchor = text->textAnchor;

  auto* textBox = fs.textBox;
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
    switch (textBox->paragraphAlign) {
      case ParagraphAlign::Middle:
        y = textBox->position.y + textBox->size.height / 2 + text->fontSize * 0.35f;
        break;
      case ParagraphAlign::Far:
        y = textBox->position.y + textBox->size.height;
        break;
      default:
        y = textBox->position.y + text->fontSize;
        break;
    }
  }

  svg.openElement("text");
  if (!transform.empty()) {
    svg.addAttribute("transform", transform);
  }
  svg.addAttributeIfNonZero("x", x);
  svg.addAttributeIfNonZero("y", y);
  if (!text->fontFamily.empty()) {
    svg.addAttribute("font-family", text->fontFamily);
  }
  if (text->fontSize != 12.0f) {
    svg.addAttribute("font-size", FloatToString(text->fontSize));
  }
  if (text->letterSpacing != 0.0f) {
    svg.addAttribute("letter-spacing", FloatToString(text->letterSpacing));
  }
  if (anchor == TextAnchor::Center) {
    svg.addAttribute("text-anchor", std::string("middle"));
  } else if (anchor == TextAnchor::End) {
    svg.addAttribute("text-anchor", std::string("end"));
  }
  if (!text->fontStyle.empty()) {
    bool hasBold = text->fontStyle.find("Bold") != std::string::npos;
    bool hasItalic = text->fontStyle.find("Italic") != std::string::npos;
    if (hasBold) {
      svg.addAttribute("font-weight", std::string("bold"));
    }
    if (hasItalic) {
      svg.addAttribute("font-style", std::string("italic"));
    }
  }
  applyFillAttributes(svg, fs.fill, ctx, defs);
  applyStrokeAttributes(svg, fs.stroke, ctx, defs);
  svg.closeElementWithText(text->text);
}

//==============================================================================
// Write a list of elements (shared by Group and Layer)
//==============================================================================

static void writeElements(SVGBuilder& svg, const std::vector<Element*>& elements,
                          SVGExportContext& ctx, SVGBuilder& defs,
                          const std::string& transform = "") {
  auto fs = collectFillStroke(elements);

  for (const auto* element : elements) {
    auto type = element->nodeType();
    if (type == NodeType::Fill || type == NodeType::Stroke || type == NodeType::TextBox) {
      continue;
    }
    switch (type) {
      case NodeType::Rectangle:
        writeRectangle(svg, static_cast<const Rectangle*>(element), fs, ctx, defs, transform);
        break;
      case NodeType::Ellipse:
        writeEllipse(svg, static_cast<const Ellipse*>(element), fs, ctx, defs, transform);
        break;
      case NodeType::Path:
        writePath(svg, static_cast<const Path*>(element), fs, ctx, defs, transform);
        break;
      case NodeType::Text:
        writeText(svg, static_cast<const Text*>(element), fs, ctx, defs, transform);
        break;
      case NodeType::Group:
        writeElements(svg, static_cast<const Group*>(element)->elements, ctx, defs, transform);
        break;
      default:
        break;
    }
  }
}

//==============================================================================
// Write Layer → <g>
//==============================================================================

static void writeLayerContents(SVGBuilder& svg, const Layer* layer, SVGExportContext& ctx,
                               SVGBuilder& defs, const std::string& transform) {
  writeElements(svg, layer->contents, ctx, defs, transform);
}

static void writeLayer(SVGBuilder& svg, const Layer* layer, SVGExportContext& ctx,
                       SVGBuilder& defs) {
  if (!layer->visible && layer->mask == nullptr) {
    return;
  }

  bool needsGroup = !layer->matrix.isIdentity() || layer->alpha < 1.0f ||
                    !layer->id.empty() || !layer->filters.empty() || layer->mask != nullptr ||
                    layer->x != 0.0f || layer->y != 0.0f || !layer->customData.empty();

  if (!needsGroup) {
    writeLayerContents(svg, layer, ctx, defs);
    for (const auto* child : layer->children) {
      writeLayer(svg, child, ctx, defs);
    }
    return;
  }

  svg.openElement("g");

  if (!layer->id.empty()) {
    svg.addAttribute("id", layer->id);
  }

  for (const auto& [key, value] : layer->customData) {
    svg.addAttribute(("data-" + key).c_str(), value);
  }

  std::string transform = buildLayerTransform(layer);
  if (!transform.empty()) {
    svg.addAttribute("transform", transform);
  }

  if (layer->alpha < 1.0f) {
    svg.addAttribute("opacity", FloatToString(layer->alpha));
  }

  // Handle filters.
  if (!layer->filters.empty()) {
    auto filterId = writeFilterDefs(defs, layer->filters, ctx);
    if (!filterId.empty()) {
      svg.addAttribute("filter", "url(#" + filterId + ")");
    }
  }

  // Handle mask.
  if (layer->mask != nullptr) {
    if (layer->maskType == MaskType::Alpha) {
      auto clipId = writeClipPathDef(defs, layer->mask, ctx, defs);
      svg.addAttribute("clip-path", "url(#" + clipId + ")");
    } else {
      auto maskId = writeMaskDef(defs, layer->mask, ctx, defs);
      svg.addAttribute("mask", "url(#" + maskId + ")");
    }
  }

  bool hasContent = !layer->contents.empty() || !layer->children.empty();
  if (!hasContent) {
    svg.closeElementSelfClosing();
    return;
  }

  svg.closeElementStart();

  writeLayerContents(svg, layer, ctx, defs);

  for (const auto* child : layer->children) {
    writeLayer(svg, child, ctx, defs);
  }

  svg.closeElement();
}

//==============================================================================
// Main Export function
//==============================================================================

std::string SVGExporter::ToSVG(const PAGXDocument& doc, const Options& options) {
  SVGExportContext ctx = {};
  SVGBuilder svg(options.indent);
  SVGBuilder defs(options.indent);

  if (options.xmlDeclaration) {
    svg.appendDeclaration();
  }

  svg.openElement("svg");
  svg.addAttribute("xmlns", std::string("http://www.w3.org/2000/svg"));
  svg.addAttribute("width", doc.width);
  svg.addAttribute("height", doc.height);
  std::string viewBox = FloatToString(0) + " " + FloatToString(0) + " " +
                        FloatToString(doc.width) + " " + FloatToString(doc.height);
  svg.addAttribute("viewBox", viewBox);
  svg.closeElementStart();

  // First pass: generate all layer SVG content (this also generates defs).
  SVGBuilder bodyContent(options.indent);
  for (const auto* layer : doc.layers) {
    writeLayer(bodyContent, layer, ctx, defs);
  }

  // Write <defs> section if any defs were generated.
  std::string defsStr = defs.release();
  if (!defsStr.empty()) {
    svg.openElement("defs");
    svg.closeElementStart();
    svg.addRawContent(defsStr);
    svg.closeElement();
  }

  // Write body content.
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
