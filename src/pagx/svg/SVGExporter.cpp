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
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include "pagx/LayoutContext.h"
#include "pagx/TextLayoutParams.h"
#include "base/utils/MathUtil.h"
#include "pagx/PAGXDocument.h"
#include "pagx/TextLayout.h"
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
#include "pagx/svg/SVGBlendMode.h"
#include "pagx/svg/SVGPathParser.h"
#include "pagx/types/Rect.h"
#include "pagx/utils/Base64.h"
#include "pagx/utils/ExporterUtils.h"
#include "pagx/utils/StringParser.h"
#include "pagx/xml/XMLBuilder.h"

namespace pagx {

using pag::FloatNearlyZero;
using SVGBuilder = XMLBuilder;

// SVGBuilder is provided by pagx/xml/XMLBuilder.h (aliased above as SVGBuilder = XMLBuilder)

//==============================================================================
// Utility types and static helpers
//==============================================================================

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

static std::string GetImageHref(const Image* image) {
  if (image->data) {
    return "data:image/png;base64," + Base64Encode(image->data->bytes(), image->data->size());
  }
  if (!image->filePath.empty()) {
    return image->filePath;
  }
  return {};
}

static std::string BuildLayerTransform(const Layer* layer) {
  Matrix m = BuildLayerMatrix(layer);
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
  SVGWriter(SVGBuilder* defs, SVGWriterContext* context, int indentSpaces = 2,
            bool convertTextToPath = true, LayoutContext* layoutContext = nullptr)
      : _defs(defs), _context(context), _indentSpaces(indentSpaces),
        _convertTextToPath(convertTextToPath), _layoutContext(layoutContext) {
  }

  void writeLayer(SVGBuilder& out, const Layer* layer);

 private:
  SVGBuilder* _defs = nullptr;
  SVGWriterContext* _context = nullptr;
  int _indentSpaces = 2;
  bool _convertTextToPath = true;
  LayoutContext* _layoutContext = nullptr;

  std::string generateId(const std::string& prefix) {
    return _context->generateId(prefix);
  }

  // Layer / element writing
  void writeLayerContents(SVGBuilder& out, const Layer* layer, const Matrix& transform = {});
  void writeElements(SVGBuilder& out, const std::vector<Element*>& elements,
                     const Matrix& transform = {});

  // Shape writing
  void writeRectangle(SVGBuilder& out, const Rectangle* rect, const FillStrokeInfo& fs,
                      const std::string& transform = "");
  void writeEllipse(SVGBuilder& out, const Ellipse* ellipse, const FillStrokeInfo& fs,
                    const std::string& transform = "");
  void writePath(SVGBuilder& out, const Path* path, const FillStrokeInfo& fs,
                 const std::string& transform = "");
  void writeText(SVGBuilder& out, const Text* text, const FillStrokeInfo& fs,
                 const std::string& transform = "");
  void writeTextAsPath(SVGBuilder& out, const Text* text, const FillStrokeInfo& fs,
                       const std::string& transform = "");
  void writeTextWithLayout(SVGBuilder& out, const Text* text, const FillStrokeInfo& fs,
                           const TextLayoutResult& layoutResult, const std::string& transform);
  void writeTextBoxGroup(SVGBuilder& out, const Group* textBox,
                         const std::vector<Element*>& elements, const Matrix& transform);

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
  std::string writeMaskOrClipDef(const Layer* maskLayer, const char* tag, const char* idPrefix,
                                 ContentWriter writer, MaskType maskType = MaskType::Contour);
  void writeMaskContent(SVGBuilder& out, const Layer* layer);
  void writeClipPathContent(SVGBuilder& out, const Layer* layer);
  void writeClipPathContentRecursive(SVGBuilder& out, const Layer* layer,
                                     const Matrix& parentMatrix = {});
  std::string writeMaskDef(const Layer* maskLayer, MaskType maskType = MaskType::Alpha);
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

std::string SVGWriter::writeImagePatternDef(const ImagePattern* pattern, const Rect& shapeBounds) {
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
                                          const char* idPrefix, ContentWriter writer,
                                          MaskType maskType) {
  std::string defId = maskLayer->id.empty() ? generateId(idPrefix) : maskLayer->id;
  SVGBuilder paintDefs(true, _indentSpaces);
  _defs->openElement(tag);
  _defs->addAttribute("id", defId);
  if (maskType == MaskType::Alpha) {
    _defs->addAttribute("style", "mask-type:alpha");
  }
  _defs->closeElementStart();
  SVGWriter nestedWriter(&paintDefs, _context, _indentSpaces, _convertTextToPath, _layoutContext);
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
                                              const Matrix& parentMatrix) {
  Matrix layerMatrix = BuildLayerMatrix(layer);
  Matrix combined = parentMatrix * layerMatrix;

  writeLayerContents(out, layer, combined);
  for (const auto* child : layer->children) {
    writeClipPathContentRecursive(out, child, combined);
  }
}

std::string SVGWriter::writeMaskDef(const Layer* maskLayer, MaskType maskType) {
  return writeMaskOrClipDef(maskLayer, "mask", "mask", &SVGWriter::writeMaskContent, maskType);
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
  float x = rect->position.x - rect->size.width / 2;
  float y = rect->position.y - rect->size.height / 2;
  out.openElement("rect");
  if (!transform.empty()) {
    out.addAttribute("transform", transform);
  }
  out.addAttributeIfNonZero("x", x);
  out.addAttributeIfNonZero("y", y);
  out.addRequiredAttribute("width", rect->size.width);
  out.addRequiredAttribute("height", rect->size.height);
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
    out.addRequiredAttribute("cx", ellipse->position.x);
    out.addRequiredAttribute("cy", ellipse->position.y);
    out.addRequiredAttribute("r", rx);
  } else {
    out.openElement("ellipse");
    if (!transform.empty()) {
      out.addAttribute("transform", transform);
    }
    out.addRequiredAttribute("cx", ellipse->position.x);
    out.addRequiredAttribute("cy", ellipse->position.y);
    out.addRequiredAttribute("rx", rx);
    out.addRequiredAttribute("ry", ry);
  }
  Rect bounds = Rect::MakeXYWH(ellipse->position.x - rx, ellipse->position.y - ry, rx * 2, ry * 2);
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
  bool hasPosition = path->position.x != 0.0f || path->position.y != 0.0f;
  if (hasPosition) {
    std::string posTransform = "translate(" + FloatToString(path->position.x) + "," +
                               FloatToString(path->position.y) + ")";
    if (!transform.empty()) {
      out.addAttribute("transform", transform + " " + posTransform);
    } else {
      out.addAttribute("transform", posTransform);
    }
  } else if (!transform.empty()) {
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

// ── writeTextAsPath ─────────────────────────────────────────────────────────

void SVGWriter::writeTextAsPath(SVGBuilder& out, const Text* text, const FillStrokeInfo& fs,
                                const std::string& transform) {
  float textPosX = text->position.x;
  float textPosY = text->position.y;

  auto glyphPaths = ComputeGlyphPaths(*text, textPosX, textPosY);
  if (glyphPaths.empty()) {
    return;
  }

  out.openElement("g");
  if (!transform.empty()) {
    out.addAttribute("transform", transform);
  }
  std::string p3Style;
  applyFillAttributes(out, fs.fill, {}, &p3Style);
  applyStrokeAttributes(out, fs.stroke, {}, &p3Style);
  applyP3Style(out, p3Style);
  out.closeElementStart();

  for (const auto& gp : glyphPaths) {
    out.openElement("path");
    out.addAttribute("transform", MatrixToSVGTransform(gp.transform));
    out.addAttribute("d", PathDataToSVGString(*gp.pathData));
    out.closeElementSelfClosing();
  }

  out.closeElement();
}

// ── writeText ───────────────────────────────────────────────────────────────

static void WriteSharedTextAttrs(SVGBuilder& out, const Text* text, TextAnchor anchor) {
  if (!text->fontFamily.empty()) {
    out.addAttribute("font-family", text->fontFamily);
  }
  out.addAttribute("font-size", FloatToString(text->fontSize));
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
}

void SVGWriter::writeTextWithLayout(SVGBuilder& out, const Text* text, const FillStrokeInfo& fs,
                                    const TextLayoutResult& layoutResult,
                                    const std::string& transform) {
  auto* mutableText = const_cast<Text*>(text);
  auto* lines = layoutResult.getTextLines(mutableText);
  if (!lines || lines->empty()) {
    return;
  }

  TextAnchor anchor = TextAnchor::Start;
  float anchorX = 0;
  float offsetY = 0;
  if (fs.textBox) {
    float paddingLeft = fs.textBox->padding.left;
    float paddingTop = fs.textBox->padding.top;
    switch (fs.textBox->textAlign) {
      case TextAlign::Center:
        anchor = TextAnchor::Center;
        anchorX = fs.textBox->position.x + paddingLeft +
                  (fs.textBox->width - paddingLeft - fs.textBox->padding.right) / 2;
        break;
      case TextAlign::End:
        anchor = TextAnchor::End;
        anchorX = fs.textBox->position.x + fs.textBox->width - fs.textBox->padding.right;
        break;
      default:
        anchorX = fs.textBox->position.x + paddingLeft;
        break;
    }
    offsetY = fs.textBox->position.y + paddingTop;
  } else {
    anchor = text->textAnchor;
    anchorX = text->position.x;
    offsetY = text->position.y;
  }

  for (auto& lineInfo : *lines) {
    if (lineInfo.byteStart >= lineInfo.byteEnd) {
      continue;
    }
    std::string lineText =
        text->text.substr(lineInfo.byteStart, lineInfo.byteEnd - lineInfo.byteStart);
    if (lineText.empty()) {
      continue;
    }
    out.openElement("text");
    if (!transform.empty()) {
      out.addAttribute("transform", transform);
    }
    WriteSharedTextAttrs(out, text, anchor);
    std::string p3Style;
    applyFillAttributes(out, fs.fill, {}, &p3Style);
    applyStrokeAttributes(out, fs.stroke, {}, &p3Style);
    applyP3Style(out, p3Style);
    out.addRequiredAttribute("x", anchorX);
    out.addRequiredAttribute("y", offsetY + lineInfo.baselineY);
    out.closeElementWithText(lineText);
  }
}

void SVGWriter::writeTextBoxGroup(SVGBuilder& out, const Group* textBox,
                                  const std::vector<Element*>& elements, const Matrix& transform) {
  auto* box = static_cast<const TextBox*>(textBox);
  auto fs = CollectFillStroke(elements);
  std::vector<Text*> childTexts;
  auto mutableElements = elements;
  TextLayout::CollectTextElements(mutableElements, childTexts);
  if (childTexts.empty()) {
    return;
  }

  bool hasPadding = !box->padding.isZero();
  float boxW = box->width;
  float boxH = box->height;
  if (hasPadding) {
    if (!std::isnan(boxW)) {
      boxW = std::max(0.0f, boxW - box->padding.left - box->padding.right);
    }
    if (!std::isnan(boxH)) {
      boxH = std::max(0.0f, boxH - box->padding.top - box->padding.bottom);
    }
  }

  TextLayoutParams params = {};
  params.boxWidth = boxW;
  params.boxHeight = boxH;
  params.textAlign = box->textAlign;
  params.paragraphAlign = box->paragraphAlign;
  params.writingMode = box->writingMode;
  params.lineHeight = box->lineHeight;
  params.wordWrap = box->wordWrap;
  params.overflow = box->overflow;

  auto layoutResult = TextLayout::Layout(childTexts, params, _layoutContext);
  std::string transformStr = MatrixToSVGTransform(transform);

  // Build a temporary FillStrokeInfo with the TextBox position/padding for writeTextWithLayout.
  FillStrokeInfo boxFs = fs;
  boxFs.textBox = box;

  for (auto* childText : childTexts) {
    if (childText->text.empty()) {
      continue;
    }
    writeTextWithLayout(out, childText, boxFs, layoutResult, transformStr);
  }
}

void SVGWriter::writeText(SVGBuilder& out, const Text* text, const FillStrokeInfo& fs,
                          const std::string& transform) {
  if (text->text.empty()) {
    return;
  }

  if (_convertTextToPath && !text->glyphRuns.empty()) {
    writeTextAsPath(out, text, fs, transform);
    return;
  }

  TextLayoutParams params = {};
  if (fs.textBox) {
    bool hasPadding = !fs.textBox->padding.isZero();
    float boxW = fs.textBox->width;
    float boxH = fs.textBox->height;
    if (hasPadding) {
      if (!std::isnan(boxW)) {
        boxW = std::max(0.0f, boxW - fs.textBox->padding.left - fs.textBox->padding.right);
      }
      if (!std::isnan(boxH)) {
        boxH = std::max(0.0f, boxH - fs.textBox->padding.top - fs.textBox->padding.bottom);
      }
    }
    params.boxWidth = boxW;
    params.boxHeight = boxH;
    params.textAlign = fs.textBox->textAlign;
    params.paragraphAlign = fs.textBox->paragraphAlign;
    params.writingMode = fs.textBox->writingMode;
    params.lineHeight = fs.textBox->lineHeight;
    params.wordWrap = fs.textBox->wordWrap;
    params.overflow = fs.textBox->overflow;
  } else {
    params.baseline = text->baseline;
    switch (text->textAnchor) {
      case TextAnchor::Start:
        params.textAlign = TextAlign::Start;
        break;
      case TextAnchor::Center:
        params.textAlign = TextAlign::Center;
        break;
      case TextAnchor::End:
        params.textAlign = TextAlign::End;
        break;
    }
  }
  auto mutableText = const_cast<Text*>(text);
  auto layoutResult = TextLayout::Layout({mutableText}, params, _layoutContext);
  auto* textLines = layoutResult.getTextLines(mutableText);
  if (textLines && !textLines->empty()) {
    writeTextWithLayout(out, text, fs, layoutResult, transform);
    return;
  }

  // Fallback for when TextLayout produces no line info (e.g. empty text after shaping).
  out.openElement("text");
  if (!transform.empty()) {
    out.addAttribute("transform", transform);
  }
  WriteSharedTextAttrs(out, text, text->textAnchor);
  std::string p3Style;
  applyFillAttributes(out, fs.fill, {}, &p3Style);
  applyStrokeAttributes(out, fs.stroke, {}, &p3Style);
  applyP3Style(out, p3Style);
  out.addAttributeIfNonZero("x", text->position.x);
  out.addAttributeIfNonZero("y", text->position.y);
  out.closeElementWithText(text->text);
}

//==============================================================================
// SVGWriter – element list and layer writing
//==============================================================================

void SVGWriter::writeElements(SVGBuilder& out, const std::vector<Element*>& elements,
                              const Matrix& transform) {
  auto fs = CollectFillStroke(elements);
  std::string transformStr = MatrixToSVGTransform(transform);

  for (const auto* element : elements) {
    auto type = element->nodeType();
    if (type == NodeType::Fill || type == NodeType::Stroke) {
      continue;
    }
    if (type == NodeType::TextBox) {
      auto* group = static_cast<const Group*>(element);
      if (!group->elements.empty() && !_convertTextToPath) {
        Matrix groupMatrix = BuildGroupMatrix(group);
        Matrix combined = transform * groupMatrix;
        writeTextBoxGroup(out, group, group->elements, combined);
      }
      continue;
    }
    switch (type) {
      case NodeType::Rectangle:
        writeRectangle(out, static_cast<const Rectangle*>(element), fs, transformStr);
        break;
      case NodeType::Ellipse:
        writeEllipse(out, static_cast<const Ellipse*>(element), fs, transformStr);
        break;
      case NodeType::Path:
        writePath(out, static_cast<const Path*>(element), fs, transformStr);
        break;
      case NodeType::Text:
        writeText(out, static_cast<const Text*>(element), fs, transformStr);
        break;
      case NodeType::Group: {
        auto* group = static_cast<const Group*>(element);
        Matrix groupMatrix = BuildGroupMatrix(group);
        bool hasGroupTransform = !groupMatrix.isIdentity();
        bool hasAlpha = group->alpha < 1.0f;
        if (hasGroupTransform || hasAlpha) {
          out.openElement("g");
          Matrix combined = transform * groupMatrix;
          std::string combinedStr = MatrixToSVGTransform(combined);
          if (!combinedStr.empty()) {
            out.addAttribute("transform", combinedStr);
          }
          if (hasAlpha) {
            out.addAttribute("opacity", FloatToString(group->alpha));
          }
          out.closeElementStart();
          writeElements(out, group->elements, {});
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

void SVGWriter::writeLayerContents(SVGBuilder& out, const Layer* layer, const Matrix& transform) {
  writeElements(out, layer->contents, transform);
}

void SVGWriter::writeLayer(SVGBuilder& out, const Layer* layer) {
  if (!layer->visible && layer->mask == nullptr) {
    return;
  }

  bool needsGroup = !layer->matrix.isIdentity() || layer->alpha < 1.0f || !layer->id.empty() ||
                    !layer->filters.empty() || layer->mask != nullptr || layer->x != 0.0f ||
                    layer->y != 0.0f || !layer->customData.empty() ||
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
      auto maskId = writeMaskDef(layer->mask, layer->maskType);
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
  SVGBuilder svg(true, options.indent);
  SVGBuilder defs(true, options.indent, 2);
  SVGWriterContext context;
  auto layoutContext = std::make_unique<LayoutContext>(options.fontConfig);
  SVGWriter writer(&defs, &context, options.indent, options.convertTextToPath,
                   layoutContext.get());

  if (options.xmlDeclaration) {
    svg.appendDeclaration();
  }

  svg.openElement("svg");
  svg.addAttribute("xmlns", "http://www.w3.org/2000/svg");
  svg.addRequiredAttribute("width", doc.width);
  svg.addRequiredAttribute("height", doc.height);
  std::string viewBox = "0 0 " + FloatToString(doc.width) + " " + FloatToString(doc.height);
  svg.addAttribute("viewBox", viewBox);
  svg.closeElementStart();

  SVGBuilder bodyContent(true, options.indent, 1);
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
