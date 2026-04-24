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
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include "base/utils/MathUtil.h"
#include "pagx/LayoutContext.h"
#include "pagx/PAGXDocument.h"
#include "pagx/TextLayout.h"
#include "pagx/TextLayoutParams.h"
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
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/svg/SVGBlendMode.h"
#include "pagx/svg/SVGFeatureProbe.h"
#include "pagx/svg/SVGPathParser.h"
#include "pagx/types/Rect.h"
#include "pagx/utils/Base64.h"
#include "pagx/utils/ExporterUtils.h"
#include "pagx/utils/ModifierResolver.h"
#include "pagx/utils/StringParser.h"
#include "pagx/xml/XMLBuilder.h"
#include "renderer/LayerBuilder.h"

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

// Emits a CSS color(display-p3 ...) value. Only used when the color source has
// Display P3 color space values; sRGB colors use the standard #RRGGBB format.
static std::string ColorToDisplayP3String(const Color& color) {
  return "color(display-p3 " + FloatToString(color.red) + " " + FloatToString(color.green) + " " +
         FloatToString(color.blue) + ")";
}

// feGaussianBlur stdDeviation string: one value when blurX == blurY, otherwise two.
static std::string FormatBlurStdDev(float blurX, float blurY) {
  std::string stdDev = FloatToString(blurX);
  if (blurX != blurY) {
    stdDev += " ";
    stdDev += FloatToString(blurY);
  }
  return stdDev;
}

// mix-blend-mode CSS fragment appended to the inline style string when a blend mode
// other than Normal is present on a Fill or Stroke. No-op for Normal.
static void AppendBlendModeStyle(std::string& styleStr, BlendMode mode) {
  if (mode == BlendMode::Normal) {
    return;
  }
  auto blendStr = BlendModeToSVGString(mode);
  if (!blendStr) {
    return;
  }
  styleStr += "mix-blend-mode:";
  styleStr += blendStr;
  styleStr += ';';
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
            bool convertTextToPath = true, LayoutContext* layoutContext = nullptr,
            PAGXDocument* doc = nullptr, bool rasterizeUnsupportedFeatures = true,
            int rasterDPI = 192)
      : _defs(defs), _context(context), _indentSpaces(indentSpaces),
        _convertTextToPath(convertTextToPath),
        _rasterizeUnsupportedFeatures(rasterizeUnsupportedFeatures), _rasterDPI(rasterDPI),
        _layoutContext(layoutContext), _resolver(doc), _doc(doc) {
  }

  void writeLayer(SVGBuilder& out, const Layer* layer);

 private:
  SVGBuilder* _defs = nullptr;
  SVGWriterContext* _context = nullptr;
  int _indentSpaces = 2;
  bool _convertTextToPath = true;
  bool _rasterizeUnsupportedFeatures = true;
  int _rasterDPI = 192;
  LayoutContext* _layoutContext = nullptr;
  ModifierResolver _resolver;
  PAGXDocument* _doc = nullptr;
  GPUContext _gpu;
  LayerBuildResult _buildResult = {};
  bool _buildResultReady = false;

  std::string generateId(const std::string& prefix) {
    return _context->generateId(prefix);
  }

  const LayerBuildResult& ensureBuildResult();

  // Rasterizes the given layer to a PNG and emits it as a positioned <image>. Returns true iff a
  // picture was emitted.
  bool rasterizeLayerAsImage(SVGBuilder& out, const Layer* layer);

  // One geometry instance captured during the scope walk in writeElements.
  // Transform/alpha are baked at collection time so that later painters can
  // emit the geometry without knowing about the surrounding Group/TextBox
  // stack. `textBox` carries the in-scope <TextBox> modifier so Text geometry
  // still picks up box-level layout when rendered by a downstream painter
  // (matches the legacy CollectFillStroke().textBox rule).
  struct AccumulatedGeometry {
    const Element* element = nullptr;
    Matrix transform = {};
    float alpha = 1.0f;
    const TextBox* textBox = nullptr;
  };

  // Layer / element writing
  void writeLayerContents(SVGBuilder& out, const Layer* layer, const Matrix& transform = {});
  void writeElements(SVGBuilder& out, const std::vector<Element*>& elements,
                     const Matrix& transform, float alpha, const TextBox* parentTextBox = nullptr);
  void processVectorScope(SVGBuilder& out, const std::vector<Element*>& elements,
                          const Matrix& transform, float alpha, const TextBox* parentTextBox,
                          std::vector<AccumulatedGeometry>& accumulator, size_t scopeStart);
  void emitGeometryWithFs(SVGBuilder& out, const AccumulatedGeometry& entry,
                          const FillStrokeInfo& fs);

  // Shape writing
  void writeRectangle(SVGBuilder& out, const Rectangle* rect, const FillStrokeInfo& fs,
                      const Matrix& m, float alpha);
  void writeEllipse(SVGBuilder& out, const Ellipse* ellipse, const FillStrokeInfo& fs,
                    const Matrix& m, float alpha);
  void writePath(SVGBuilder& out, const Path* path, const FillStrokeInfo& fs, const Matrix& m,
                 float alpha);
  void writeText(SVGBuilder& out, const Text* text, const FillStrokeInfo& fs, const Matrix& m,
                 float alpha);
  void writeTextAsPath(SVGBuilder& out, const Text* text, const FillStrokeInfo& fs, const Matrix& m,
                       float alpha);
  void writeTextWithLayout(SVGBuilder& out, const Text* text, const FillStrokeInfo& fs,
                           const TextLayoutResult& layoutResult, const Matrix& m, float alpha);
  void writeTextBoxGroup(SVGBuilder& out, const TextBox* textBox,
                         const std::vector<Element*>& elements, const Matrix& transform,
                         float alpha);

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
  // Emits a drop-shadow fragment (blur + offset + color matrix) and returns the
  // result name of the final colored shadow. Shared between DropShadowFilter and
  // DropShadowStyle which differ only in source node type.
  std::string writeDropShadowFragment(float blurX, float blurY, float offsetX, float offsetY,
                                      const Color& color, int shadowIndex);
  // Emits an inner-shadow fragment (blur + offset + invert composite + color matrix)
  // and returns the result name of the final colored inner shadow. Shared between
  // InnerShadowFilter and InnerShadowStyle.
  std::string writeInnerShadowFragment(float blurX, float blurY, float offsetX, float offsetY,
                                       const Color& color, int shadowIndex);
  // Standalone filter primitives that don't take part in the feMerge aggregation.
  // Each one drains directly into the surrounding <filter> chain.
  void writeColorMatrixFilter(const ColorMatrixFilter* cm);
  void writeBlendFilter(const BlendFilter* blend, int& shadowIndex);
  // Collected per-filter state fed into the final feMerge aggregation.
  struct ShadowAggregate {
    std::vector<std::string> dropShadowResults;
    std::vector<std::string> innerShadowResults;
    bool needSourceGraphic = false;
  };
  void writeFilterList(const std::vector<LayerFilter*>& filters, int& shadowIndex,
                       ShadowAggregate& agg);
  void writeStyleList(const std::vector<LayerStyle*>& styles, int& shadowIndex,
                      ShadowAggregate& agg);
  void writeShadowMerge(const ShadowAggregate& agg);
  std::string writeFilterAndStyleDefs(const std::vector<LayerFilter*>& filters,
                                      const std::vector<LayerStyle*>& styles);

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
  // Emits a <clipPath> in _defs for layer->scrollRect and returns the generated id.
  // Caller wires the id onto the group as clip-path="url(#...)".
  std::string writeScrollRectClipDef(const Layer* layer);
  // Writes the non-content attributes of a layer's <g> (id, data-*, transform,
  // opacity, style, filter, mask/clipPath, scrollRect clip). Geometry and child
  // layers are emitted separately after closeElementStart() in writeLayer.
  void writeLayerGroupAttributes(SVGBuilder& out, const Layer* layer);
  // Emits contents + composition layers + child layers. Used both for the
  // needs-group path (inside the <g>) and for the bare-through path.
  void writeLayerBody(SVGBuilder& out, const Layer* layer);

  // Fill / stroke attribute helpers
  void applyFillAttributes(SVGBuilder& out, const Fill* fill, const Rect& shapeBounds = {},
                           std::string* p3Style = nullptr, float alphaMultiplier = 1.0f);
  void applyStrokeAttributes(SVGBuilder& out, const Stroke* stroke, const Rect& shapeBounds = {},
                             std::string* p3Style = nullptr, float alphaMultiplier = 1.0f);
  // Writes fill, stroke, and the optional collected P3 style fragment as SVG attributes.
  // Every geometry writer ends with this three-call sequence so keeping it together
  // avoids forgetting a step and makes the painter apply order explicit.
  void applyPainters(SVGBuilder& out, const FillStrokeInfo& fs, const Rect& shapeBounds,
                     float alpha);
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
      _defs->addRequiredAttribute("x1", grad->startPoint.x);
      _defs->addRequiredAttribute("y1", grad->startPoint.y);
      _defs->addRequiredAttribute("x2", grad->endPoint.x);
      _defs->addRequiredAttribute("y2", grad->endPoint.y);
      finishGradientDef(grad->matrix, grad->colorStops);
      break;
    }
    case NodeType::RadialGradient: {
      auto grad = static_cast<const RadialGradient*>(source);
      _defs->openElement("radialGradient");
      _defs->addAttribute("id", id);
      _defs->addRequiredAttribute("cx", grad->center.x);
      _defs->addRequiredAttribute("cy", grad->center.y);
      _defs->addRequiredAttribute("r", grad->radius);
      finishGradientDef(grad->matrix, grad->colorStops);
      break;
    }
    case NodeType::ConicGradient: {
      // SVG has no conic/sweep gradient primitive (CSS `conic-gradient` is a
      // paint only available in HTML). Degrade to a radial gradient centred
      // at the conic's center so the stops still appear; the angular
      // distribution is lost — matches PPT's path="circle" fallback. The
      // conic has no radius; pick 50% of the viewport as a sensible default
      // via gradientUnits="objectBoundingBox" would need a rework of the
      // gradient machinery, so fall back to a fixed pixel radius here.
      auto grad = static_cast<const ConicGradient*>(source);
      _defs->addRawContent(std::string(_indentSpaces, ' ') +
                           "<!-- conic gradient degraded to radial -->\n");
      _defs->openElement("radialGradient");
      _defs->addAttribute("id", id);
      _defs->addRequiredAttribute("cx", grad->center.x);
      _defs->addRequiredAttribute("cy", grad->center.y);
      _defs->addAttribute("r", 100.0f);
      finishGradientDef(grad->matrix, grad->colorStops);
      break;
    }
    case NodeType::DiamondGradient: {
      // SVG has no diamond gradient either; radial is the closest portable
      // approximation. Same authoring caveat as ConicGradient.
      auto grad = static_cast<const DiamondGradient*>(source);
      _defs->addRawContent(std::string(_indentSpaces, ' ') +
                           "<!-- diamond gradient degraded to radial -->\n");
      _defs->openElement("radialGradient");
      _defs->addAttribute("id", id);
      _defs->addRequiredAttribute("cx", grad->center.x);
      _defs->addRequiredAttribute("cy", grad->center.y);
      _defs->addRequiredAttribute("r", grad->radius);
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

  // Detect which tile modes we need to handle
  bool needsNativeTiling =
      (pattern->tileModeX == TileMode::Repeat || pattern->tileModeY == TileMode::Repeat);
  bool needsBaking =
      (pattern->tileModeX == TileMode::Mirror || pattern->tileModeY == TileMode::Mirror ||
       pattern->tileModeX == TileMode::Clamp || pattern->tileModeY == TileMode::Clamp ||
       pattern->tileModeX == TileMode::Decal || pattern->tileModeY == TileMode::Decal);

  // Try baking for unsupported tile modes (Mirror, Clamp, Decal)
  if (needsBaking && !shapeBounds.isEmpty()) {
    int w = static_cast<int>(ceilf(shapeBounds.width));
    int h = static_cast<int>(ceilf(shapeBounds.height));
    float offsetX = pattern->matrix.tx - shapeBounds.x;
    float offsetY = pattern->matrix.ty - shapeBounds.y;
    float pixelScale = static_cast<float>(_rasterDPI) / 96.0f;

    auto tiledPng = RenderTiledPattern(&_gpu, pattern, w, h, offsetX, offsetY, pixelScale);
    if (tiledPng && tiledPng->size() > 0) {
      // Successfully baked the pattern to PNG - embed as data URI
      std::string base64Data = Base64Encode(tiledPng->bytes(), tiledPng->size());
      href = "data:image/png;base64," + base64Data;

      _defs->openElement("pattern");
      _defs->addAttribute("id", defId);
      _defs->addAttribute("patternUnits", "userSpaceOnUse");
      _defs->addAttributeIfNonZero("x", shapeBounds.x);
      _defs->addAttributeIfNonZero("y", shapeBounds.y);
      _defs->addAttribute("width", static_cast<float>(w));
      _defs->addAttribute("height", static_cast<float>(h));
      _defs->closeElementStart();
      _defs->openElement("image");
      _defs->addAttribute("href", href);
      _defs->addAttribute("x", 0.0f);
      _defs->addAttribute("y", 0.0f);
      _defs->addAttribute("width", static_cast<float>(w));
      _defs->addAttribute("height", static_cast<float>(h));
      _defs->addAttribute("preserveAspectRatio", "none");
      _defs->closeElementSelfClosing();
      _defs->closeElement();
      return "url(#" + defId + ")";
    }
    // If baking failed, fall through to try native tiling for Repeat modes
  }

  // Fall back to native SVG pattern handling for Repeat mode and non-bakeable cases.
  // All paths use patternUnits="userSpaceOnUse" so that <image> width/height are in
  // pixel coordinates — objectBoundingBox would reinterpret them as bbox fractions,
  // making the raster content invisible or distorted.
  int imgW = 0;
  int imgH = 0;
  bool hasImageSize = GetImagePNGDimensions(pattern->image, &imgW, &imgH) && imgW > 0 && imgH > 0;

  _defs->openElement("pattern");
  _defs->addAttribute("id", defId);
  _defs->addAttribute("patternUnits", "userSpaceOnUse");

  if (hasImageSize && !shapeBounds.isEmpty() && needsNativeTiling) {
    // Tile size equals the source image pixel dimensions; the pattern matrix is
    // applied via patternTransform so the tile grid scales/rotates/translates
    // correctly without distorting the raster content.
    _defs->addRequiredAttribute("width", static_cast<float>(imgW));
    _defs->addRequiredAttribute("height", static_cast<float>(imgH));
    if (!pattern->matrix.isIdentity()) {
      _defs->addAttribute("patternTransform", MatrixToSVGTransform(pattern->matrix));
    }
    _defs->closeElementStart();
    _defs->openElement("image");
    _defs->addAttribute("href", href);
    _defs->addRequiredAttribute("width", static_cast<float>(imgW));
    _defs->addRequiredAttribute("height", static_cast<float>(imgH));
  } else if (hasImageSize && !shapeBounds.isEmpty()) {
    // Non-tiling fallback: single tile covers the entire shape bounds so the
    // image appears exactly once. The pattern matrix positions the image inside.
    _defs->addAttributeIfNonZero("x", shapeBounds.x);
    _defs->addAttributeIfNonZero("y", shapeBounds.y);
    _defs->addRequiredAttribute("width", shapeBounds.width);
    _defs->addRequiredAttribute("height", shapeBounds.height);
    _defs->closeElementStart();
    _defs->openElement("image");
    _defs->addAttribute("href", href);
    _defs->addRequiredAttribute("width", static_cast<float>(imgW));
    _defs->addRequiredAttribute("height", static_cast<float>(imgH));
    if (!pattern->matrix.isIdentity()) {
      Matrix localMatrix = pattern->matrix;
      localMatrix.tx -= shapeBounds.x;
      localMatrix.ty -= shapeBounds.y;
      _defs->addAttribute("transform", MatrixToSVGTransform(localMatrix));
    }
  } else {
    _defs->addAttribute("width", "100%");
    _defs->addAttribute("height", "100%");
    _defs->closeElementStart();
    _defs->openElement("image");
    _defs->addAttribute("href", href);
    if (hasImageSize) {
      _defs->addRequiredAttribute("width", static_cast<float>(imgW));
      _defs->addRequiredAttribute("height", static_cast<float>(imgH));
    }
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
    if (outAlpha) {
      *outAlpha = solid->color.alpha;
    }
    return ColorToSVGString(solid->color);
  }
  if (source->nodeType() == NodeType::LinearGradient ||
      source->nodeType() == NodeType::RadialGradient ||
      source->nodeType() == NodeType::ConicGradient ||
      source->nodeType() == NodeType::DiamondGradient) {
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
  _defs->addAttribute("stdDeviation", FormatBlurStdDev(blurX, blurY));
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

std::string SVGWriter::writeDropShadowFragment(float blurX, float blurY, float offsetX,
                                               float offsetY, const Color& color, int shadowIndex) {
  std::string idx = std::to_string(shadowIndex);
  std::string offsetResult = "shadowOffset" + idx;
  std::string shadowResult = "shadow" + idx;
  writeShadowBlurAndOffset(blurX, blurY, offsetX, offsetY, "shadowBlur" + idx, offsetResult);
  writeShadowColorMatrix(color, offsetResult, shadowResult);
  return shadowResult;
}

std::string SVGWriter::writeInnerShadowFragment(float blurX, float blurY, float offsetX,
                                                float offsetY, const Color& color,
                                                int shadowIndex) {
  std::string idx = std::to_string(shadowIndex);
  std::string offsetResult = "innerOffset" + idx;
  std::string compositeResult = "innerComposite" + idx;
  std::string shadowResult = "innerShadow" + idx;
  writeShadowBlurAndOffset(blurX, blurY, offsetX, offsetY, "innerBlur" + idx, offsetResult);

  _defs->openElement("feComposite");
  _defs->addAttribute("in", "SourceAlpha");
  _defs->addAttribute("in2", offsetResult);
  _defs->addAttribute("operator", "arithmetic");
  _defs->addAttribute("k2", "-1");
  _defs->addAttribute("k3", "1");
  _defs->addAttribute("result", compositeResult);
  _defs->closeElementSelfClosing();

  writeShadowColorMatrix(color, compositeResult, shadowResult);
  return shadowResult;
}

void SVGWriter::writeColorMatrixFilter(const ColorMatrixFilter* cm) {
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
}

void SVGWriter::writeBlendFilter(const BlendFilter* blend, int& shadowIndex) {
  std::string idx = std::to_string(shadowIndex++);
  std::string floodResult = "blendFlood" + idx;
  _defs->openElement("feFlood");
  _defs->addAttribute("flood-color", ColorToSVGString(blend->color));
  if (blend->color.alpha < 1.0f) {
    _defs->addAttribute("flood-opacity", FloatToString(blend->color.alpha));
  }
  _defs->addAttribute("result", floodResult);
  _defs->closeElementSelfClosing();

  std::string blendResult = "blendResult" + idx;
  _defs->openElement("feBlend");
  _defs->addAttribute("in", "SourceGraphic");
  _defs->addAttribute("in2", floodResult);
  auto modeStr = BlendModeToSVGString(blend->blendMode);
  if (modeStr) {
    _defs->addAttribute("mode", modeStr);
  }
  _defs->addAttribute("result", blendResult);
  _defs->closeElementSelfClosing();

  _defs->openElement("feComposite");
  _defs->addAttribute("in", blendResult);
  _defs->addAttribute("in2", "SourceGraphic");
  _defs->addAttribute("operator", "in");
  _defs->closeElementSelfClosing();
}

void SVGWriter::writeFilterList(const std::vector<LayerFilter*>& filters, int& shadowIndex,
                                ShadowAggregate& agg) {
  for (const auto* filter : filters) {
    switch (filter->nodeType()) {
      case NodeType::BlurFilter: {
        auto blur = static_cast<const BlurFilter*>(filter);
        _defs->openElement("feGaussianBlur");
        _defs->addAttribute("in", "SourceGraphic");
        _defs->addAttribute("stdDeviation", FormatBlurStdDev(blur->blurX, blur->blurY));
        _defs->closeElementSelfClosing();
        break;
      }
      case NodeType::DropShadowFilter: {
        auto shadow = static_cast<const DropShadowFilter*>(filter);
        agg.dropShadowResults.push_back(writeDropShadowFragment(shadow->blurX, shadow->blurY,
                                                                shadow->offsetX, shadow->offsetY,
                                                                shadow->color, shadowIndex++));
        if (!shadow->shadowOnly) {
          agg.needSourceGraphic = true;
        }
        break;
      }
      case NodeType::InnerShadowFilter: {
        auto shadow = static_cast<const InnerShadowFilter*>(filter);
        agg.innerShadowResults.push_back(writeInnerShadowFragment(shadow->blurX, shadow->blurY,
                                                                  shadow->offsetX, shadow->offsetY,
                                                                  shadow->color, shadowIndex++));
        if (!shadow->shadowOnly) {
          agg.needSourceGraphic = true;
        }
        break;
      }
      case NodeType::ColorMatrixFilter:
        writeColorMatrixFilter(static_cast<const ColorMatrixFilter*>(filter));
        break;
      case NodeType::BlendFilter:
        writeBlendFilter(static_cast<const BlendFilter*>(filter), shadowIndex);
        break;
      default:
        break;
    }
  }
}

// LayerStyle emission mirrors the Filter pass so DropShadowStyle / InnerShadowStyle
// share the feMerge aggregation logic. BackgroundBlurStyle is silently skipped because SVG has
// no portable backdrop-blur primitive (the deprecated enable-background is not supported by
// modern renderers).
void SVGWriter::writeStyleList(const std::vector<LayerStyle*>& styles, int& shadowIndex,
                               ShadowAggregate& agg) {
  for (const auto* style : styles) {
    switch (style->nodeType()) {
      case NodeType::DropShadowStyle: {
        auto shadow = static_cast<const DropShadowStyle*>(style);
        agg.dropShadowResults.push_back(writeDropShadowFragment(shadow->blurX, shadow->blurY,
                                                                shadow->offsetX, shadow->offsetY,
                                                                shadow->color, shadowIndex++));
        // DropShadowStyle always composites below the source graphic (it is a
        // style layer, not a filter that can hide the source).
        agg.needSourceGraphic = true;
        break;
      }
      case NodeType::InnerShadowStyle: {
        auto shadow = static_cast<const InnerShadowStyle*>(style);
        agg.innerShadowResults.push_back(writeInnerShadowFragment(shadow->blurX, shadow->blurY,
                                                                  shadow->offsetX, shadow->offsetY,
                                                                  shadow->color, shadowIndex++));
        agg.needSourceGraphic = true;
        break;
      }
      default:
        break;
    }
  }
}

void SVGWriter::writeShadowMerge(const ShadowAggregate& agg) {
  bool hasShadows = !agg.dropShadowResults.empty() || !agg.innerShadowResults.empty();
  if (!hasShadows) {
    return;
  }
  bool multipleShadows = (agg.dropShadowResults.size() + agg.innerShadowResults.size()) > 1;
  if (!agg.needSourceGraphic && !multipleShadows) {
    return;
  }
  _defs->openElement("feMerge");
  _defs->closeElementStart();
  for (const auto& result : agg.dropShadowResults) {
    _defs->openElement("feMergeNode");
    _defs->addAttribute("in", result);
    _defs->closeElementSelfClosing();
  }
  if (agg.needSourceGraphic) {
    _defs->openElement("feMergeNode");
    _defs->addAttribute("in", "SourceGraphic");
    _defs->closeElementSelfClosing();
  }
  for (const auto& result : agg.innerShadowResults) {
    _defs->openElement("feMergeNode");
    _defs->addAttribute("in", result);
    _defs->closeElementSelfClosing();
  }
  _defs->closeElement();
}

std::string SVGWriter::writeFilterAndStyleDefs(const std::vector<LayerFilter*>& filters,
                                               const std::vector<LayerStyle*>& styles) {
  if (filters.empty() && styles.empty()) {
    return {};
  }

  // BackgroundBlurStyle is silently skipped (SVG has no portable backdrop-blur). Check whether
  // any style will actually produce SVG filter primitives so we don't emit an empty <filter>
  // element, which would make the layer invisible.
  bool hasEffectiveStyle = false;
  for (const auto* style : styles) {
    if (style->nodeType() != NodeType::BackgroundBlurStyle) {
      hasEffectiveStyle = true;
      break;
    }
  }
  if (filters.empty() && !hasEffectiveStyle) {
    return {};
  }

  // Pre-scan all filters and styles to find the maximum extent the filter
  // region must cover beyond the source bounding box. Drop shadows expand
  // outward by ~3*stdDeviation + offset in each direction; blur filters
  // expand symmetrically by ~3*stdDeviation. Inner shadows and background
  // blur are clipped to the source and need no extra room.
  // The SVG filter region is specified as a percentage of the source bbox,
  // so we use a fixed baseline of 50% and bump it up when any effect needs
  // more. Using percentages (rather than absolute pixels) keeps the region
  // correct regardless of the element's actual size.
  float marginLeft = 0;
  float marginRight = 0;
  float marginTop = 0;
  float marginBottom = 0;
  constexpr float BLUR_MULTIPLIER = 3.0f;

  for (const auto* filter : filters) {
    switch (filter->nodeType()) {
      case NodeType::BlurFilter: {
        auto blur = static_cast<const BlurFilter*>(filter);
        float ex = blur->blurX * BLUR_MULTIPLIER;
        float ey = blur->blurY * BLUR_MULTIPLIER;
        marginLeft = std::max(marginLeft, ex);
        marginRight = std::max(marginRight, ex);
        marginTop = std::max(marginTop, ey);
        marginBottom = std::max(marginBottom, ey);
        break;
      }
      case NodeType::DropShadowFilter: {
        auto shadow = static_cast<const DropShadowFilter*>(filter);
        float ex = shadow->blurX * BLUR_MULTIPLIER;
        float ey = shadow->blurY * BLUR_MULTIPLIER;
        marginLeft = std::max(marginLeft, ex + std::max(0.0f, -shadow->offsetX));
        marginRight = std::max(marginRight, ex + std::max(0.0f, shadow->offsetX));
        marginTop = std::max(marginTop, ey + std::max(0.0f, -shadow->offsetY));
        marginBottom = std::max(marginBottom, ey + std::max(0.0f, shadow->offsetY));
        break;
      }
      default:
        break;
    }
  }
  for (const auto* style : styles) {
    if (style->nodeType() == NodeType::DropShadowStyle) {
      auto shadow = static_cast<const DropShadowStyle*>(style);
      float ex = shadow->blurX * BLUR_MULTIPLIER;
      float ey = shadow->blurY * BLUR_MULTIPLIER;
      marginLeft = std::max(marginLeft, ex + std::max(0.0f, -shadow->offsetX));
      marginRight = std::max(marginRight, ex + std::max(0.0f, shadow->offsetX));
      marginTop = std::max(marginTop, ey + std::max(0.0f, -shadow->offsetY));
      marginBottom = std::max(marginBottom, ey + std::max(0.0f, shadow->offsetY));
    }
  }

  // Convert pixel margins to percentages, with a minimum of 50% per side to
  // handle arbitrary element sizes gracefully.
  float pctLeft = std::max(50.0f, std::ceil(marginLeft));
  float pctRight = std::max(50.0f, std::ceil(marginRight));
  float pctTop = std::max(50.0f, std::ceil(marginTop));
  float pctBottom = std::max(50.0f, std::ceil(marginBottom));

  std::string filterId = generateId("filter");
  _defs->openElement("filter");
  _defs->addAttribute("id", filterId);
  _defs->addAttribute("x", "-" + FloatToString(pctLeft) + "%");
  _defs->addAttribute("y", "-" + FloatToString(pctTop) + "%");
  _defs->addAttribute("width", FloatToString(100.0f + pctLeft + pctRight) + "%");
  _defs->addAttribute("height", FloatToString(100.0f + pctTop + pctBottom) + "%");
  _defs->addAttribute("color-interpolation-filters", "sRGB");
  _defs->closeElementStart();

  int shadowIndex = 0;
  ShadowAggregate agg;
  writeFilterList(filters, shadowIndex, agg);
  writeStyleList(styles, shadowIndex, agg);
  writeShadowMerge(agg);

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
  SVGWriter nestedWriter(&paintDefs, _context, _indentSpaces, _convertTextToPath, _layoutContext,
                         _doc);
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
                                    std::string* p3Style, float alphaMultiplier) {
  if (!fill) {
    out.addAttribute("fill", "none");
    return;
  }
  float alpha = 1.0f;
  // Per PAGX spec, Fill defaults to opaque black (#000000) when no color is specified.
  std::string fillStr =
      fill->color ? getColorSourceRef(fill->color, &alpha, shapeBounds) : "#000000";
  out.addAttribute("fill", fillStr);
  float effectiveAlpha = alpha * fill->alpha * alphaMultiplier;
  if (effectiveAlpha < 1.0f) {
    out.addAttribute("fill-opacity", FloatToString(effectiveAlpha));
  }
  if (fill->fillRule == FillRule::EvenOdd) {
    out.addAttribute("fill-rule", "evenodd");
  }
  if (p3Style) {
    AppendP3ColorStyle(*p3Style, "fill", fill->color, fillStr, effectiveAlpha);
    AppendBlendModeStyle(*p3Style, fill->blendMode);
  }
}

void SVGWriter::applyStrokeAttributes(SVGBuilder& out, const Stroke* stroke,
                                      const Rect& shapeBounds, std::string* p3Style,
                                      float alphaMultiplier) {
  if (!stroke) {
    return;
  }
  float alpha = 1.0f;
  // Per PAGX spec, Stroke defaults to opaque black (#000000) when no color is specified.
  std::string strokeStr =
      stroke->color ? getColorSourceRef(stroke->color, &alpha, shapeBounds) : "#000000";
  out.addAttribute("stroke", strokeStr);
  float effectiveAlpha = alpha * stroke->alpha * alphaMultiplier;
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
    AppendBlendModeStyle(*p3Style, stroke->blendMode);
  }
}

void SVGWriter::applyP3Style(SVGBuilder& out, const std::string& p3Style) {
  if (!p3Style.empty()) {
    out.addAttribute("style", p3Style);
  }
}

void SVGWriter::applyPainters(SVGBuilder& out, const FillStrokeInfo& fs, const Rect& shapeBounds,
                              float alpha) {
  std::string p3Style;
  applyFillAttributes(out, fs.fill, shapeBounds, &p3Style, alpha);
  applyStrokeAttributes(out, fs.stroke, shapeBounds, &p3Style, alpha);
  applyP3Style(out, p3Style);
}

//==============================================================================
// SVGWriter – shape elements
//==============================================================================

void SVGWriter::writeRectangle(SVGBuilder& out, const Rectangle* rect, const FillStrokeInfo& fs,
                               const Matrix& m, float alpha) {
  auto renderPos = rect->renderPosition();
  auto renderSize = rect->renderSize();
  float x = renderPos.x - renderSize.width / 2.0f;
  float y = renderPos.y - renderSize.height / 2.0f;
  float w = renderSize.width;
  float h = renderSize.height;
  float roundness = rect->roundness;
  // SVG strokes, like OOXML, are always centred on the path geometry. Emulate
  // StrokeAlign::Inside / Outside by shifting this stroke painter's geometry
  // inward or outward by half the stroke width before emitting it. Fill-only
  // painters (fs.stroke == nullptr) skip this branch so the fill geometry
  // remains untouched.
  ApplyStrokeBoxInset(fs.stroke, x, y, w, h, &roundness);

  std::string transform = MatrixToSVGTransform(m);
  out.openElement("rect");
  if (!transform.empty()) {
    out.addAttribute("transform", transform);
  }
  out.addAttributeIfNonZero("x", x);
  out.addAttributeIfNonZero("y", y);
  out.addRequiredAttribute("width", w);
  out.addRequiredAttribute("height", h);
  if (roundness > 0) {
    out.addAttribute("rx", roundness);
    out.addAttribute("ry", roundness);
  }
  Rect bounds = Rect::MakeXYWH(x, y, w, h);
  applyPainters(out, fs, bounds, alpha);
  out.closeElementSelfClosing();
}

void SVGWriter::writeEllipse(SVGBuilder& out, const Ellipse* ellipse, const FillStrokeInfo& fs,
                             const Matrix& m, float alpha) {
  auto renderSize = ellipse->renderSize();
  auto renderPos = ellipse->renderPosition();
  float rx = renderSize.width / 2.0f;
  float ry = renderSize.height / 2.0f;
  float x = renderPos.x - rx;
  float y = renderPos.y - ry;
  float w = renderSize.width;
  float h = renderSize.height;
  ApplyStrokeBoxInset(fs.stroke, x, y, w, h);
  rx = w / 2.0f;
  ry = h / 2.0f;
  float cx = x + rx;
  float cy = y + ry;

  std::string transform = MatrixToSVGTransform(m);
  if (FloatNearlyZero(rx - ry)) {
    out.openElement("circle");
    if (!transform.empty()) {
      out.addAttribute("transform", transform);
    }
    out.addRequiredAttribute("cx", cx);
    out.addRequiredAttribute("cy", cy);
    out.addRequiredAttribute("r", rx);
  } else {
    out.openElement("ellipse");
    if (!transform.empty()) {
      out.addAttribute("transform", transform);
    }
    out.addRequiredAttribute("cx", cx);
    out.addRequiredAttribute("cy", cy);
    out.addRequiredAttribute("rx", rx);
    out.addRequiredAttribute("ry", ry);
  }
  Rect bounds = Rect::MakeXYWH(x, y, w, h);
  applyPainters(out, fs, bounds, alpha);
  out.closeElementSelfClosing();
}

void SVGWriter::writePath(SVGBuilder& out, const Path* path, const FillStrokeInfo& fs,
                          const Matrix& m, float alpha) {
  if (!path->data || path->data->isEmpty()) {
    return;
  }
  auto renderPos = path->renderPosition();
  out.openElement("path");
  std::string transform = MatrixToSVGTransform(m);
  bool hasPosition = renderPos.x != 0.0f || renderPos.y != 0.0f;
  if (hasPosition) {
    std::string posTransform =
        "translate(" + FloatToString(renderPos.x) + "," + FloatToString(renderPos.y) + ")";
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
  applyPainters(out, fs, bounds, alpha);
  out.closeElementSelfClosing();
}

// ── writeTextAsPath ─────────────────────────────────────────────────────────

void SVGWriter::writeTextAsPath(SVGBuilder& out, const Text* text, const FillStrokeInfo& fs,
                                const Matrix& m, float alpha) {
  auto renderPos = text->renderPosition();
  // Apply TextBox padding as an additional offset (origin is top-left of box,
  // not centred). Matches writeTextWithLayout's anchor computation.
  if (fs.textBox) {
    // For container-mode text inside a TextBox the caller has already composed
    // the box transform; padding is applied here as an origin shift into the
    // inset rect.
    renderPos.x += fs.textBox->padding.left;
    renderPos.y += fs.textBox->padding.top;
  }
  std::vector<GlyphPath> glyphPaths;
  std::vector<GlyphImage> glyphImages;
  ComputeGlyphPathsAndImages(*text, renderPos.x, renderPos.y, &glyphPaths, &glyphImages);
  if (glyphPaths.empty() && glyphImages.empty()) {
    return;
  }

  std::string transform = MatrixToSVGTransform(m);
  out.openElement("g");
  if (!transform.empty()) {
    out.addAttribute("transform", transform);
  }
  // Fill/stroke here only apply to the vector glyph <path> children; <image>
  // children have an intrinsic bitmap and ignore fill. We still emit the group
  // attributes so SVG renderers inherit them onto the glyph paths.
  applyPainters(out, fs, {}, alpha);
  out.closeElementStart();

  for (const auto& gp : glyphPaths) {
    out.openElement("path");
    out.addAttribute("transform", MatrixToSVGTransform(gp.transform));
    out.addAttribute("d", PathDataToSVGString(*gp.pathData));
    out.closeElementSelfClosing();
  }

  // Bitmap glyphs (emoji / colour fonts). Each image is emitted as an <image>
  // element transformed into its per-glyph pixel-space origin. Mirror PPT's
  // writeTextAsPath bitmap loop (PPTExporter.cpp:1566).
  for (const auto& gi : glyphImages) {
    if (!gi.image) {
      continue;
    }
    std::string href = GetImageHref(gi.image);
    if (href.empty()) {
      continue;
    }
    int imgW = 0;
    int imgH = 0;
    if (!GetImageDimensions(gi.image, &imgW, &imgH) || imgW <= 0 || imgH <= 0) {
      continue;
    }
    out.openElement("image");
    out.addAttribute("href", href);
    out.addRequiredAttribute("width", static_cast<float>(imgW));
    out.addRequiredAttribute("height", static_cast<float>(imgH));
    out.addAttribute("transform", MatrixToSVGTransform(gi.transform));
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
  // fauxBold / fauxItalic take precedence over the fontStyle name: a font may
  // lack a real bold/italic face but the authoring tool still asks the
  // renderer to synthesize one. Mirrors BuildRunStyle in PPTExporter.cpp.
  bool hasBold = text->fauxBold || text->fontStyle.find("Bold") != std::string::npos;
  bool hasItalic = text->fauxItalic || text->fontStyle.find("Italic") != std::string::npos;
  if (hasBold) {
    out.addAttribute("font-weight", "bold");
  }
  if (hasItalic) {
    out.addAttribute("font-style", "italic");
  }
}

// Emits writing-mode and text-align-last degradation notes on a <text> element.
// Called inline inside every <text> in writeTextWithLayout / writeText fallback.
static void ApplyTextBoxBodyAttrs(SVGBuilder& out, const TextBox* box) {
  if (box == nullptr) {
    return;
  }
  // CSS writing-mode "tb" (top-to-bottom) approximates PAGX's vertical CJK
  // layout. The modern value "vertical-rl" produces the same column order but
  // some older SVG renderers only understand the legacy "tb" keyword, so we
  // emit it for broadest compatibility.
  if (box->writingMode == WritingMode::Vertical) {
    out.addAttribute("writing-mode", "tb");
  }
}

// Counts the number of word boundaries (spaces) in the text for word-spacing calculation.
static int CountWordSpaces(const std::string& text) {
  int count = 0;
  for (char c : text) {
    if (c == ' ') {
      count++;
    }
  }
  return count;
}

// The positioning/anchor parameters derived from a TextBox or a standalone Text.
// Computed once by ResolveTextAnchorAndOffset and then consumed per-line by
// writeTextWithLayout.
struct TextAnchoring {
  TextAnchor anchor = TextAnchor::Start;
  float anchorX = 0;
  float offsetY = 0;
  float justifyWidth = 0;  // > 0 only when horizontal-justify is in effect.
};

// Vertical writing mode: textAlign controls the inline axis (vertical),
// paragraphAlign controls the block axis (horizontal, columns right-to-left).
static void ResolveVerticalAnchoring(const Text* text, const TextBox* box,
                                     const TextLayoutResult& layoutResult, TextAnchoring* out) {
  float paddingLeft = box->padding.left;
  float paddingTop = box->padding.top;
  float paddingRight = box->padding.right;
  float paddingBottom = box->padding.bottom;
  float effectiveWidth = EffectiveTextBoxWidth(box);
  float effectiveHeight = EffectiveTextBoxHeight(box);
  auto textBounds = layoutResult.getTextBounds(const_cast<Text*>(text));
  float contentWidth = textBounds.isEmpty() ? text->fontSize : textBounds.width;
  float innerWidth = effectiveWidth - paddingLeft - paddingRight;
  float innerHeight =
      (!std::isnan(effectiveHeight)) ? effectiveHeight - paddingTop - paddingBottom : 0;
  // Block axis (horizontal): paragraphAlign positions columns right-to-left.
  // Near = right edge, Far = left edge.
  switch (box->paragraphAlign) {
    case ParagraphAlign::Middle:
      out->anchorX = paddingLeft + (innerWidth + contentWidth) / 2 - text->fontSize / 2;
      break;
    case ParagraphAlign::Far:
      out->anchorX = paddingLeft + contentWidth - text->fontSize / 2;
      break;
    default:
      out->anchorX = effectiveWidth - paddingRight - text->fontSize / 2;
      break;
  }
  // Inline axis (vertical): textAlign positions text top-to-bottom.
  switch (box->textAlign) {
    case TextAlign::Center:
      out->anchor = TextAnchor::Center;
      out->offsetY = paddingTop + innerHeight / 2;
      break;
    case TextAlign::End:
      out->anchor = TextAnchor::End;
      out->offsetY = paddingTop + innerHeight;
      break;
    case TextAlign::Justify:
      // Vertical mode has no textLines — SVG textLength cannot be applied.
      out->anchor = TextAnchor::Start;
      out->offsetY = paddingTop;
      break;
    default:
      out->anchor = TextAnchor::Start;
      out->offsetY = paddingTop;
      break;
  }
}

static void ResolveHorizontalAnchoring(const TextBox* box, TextAnchoring* out) {
  float paddingLeft = box->padding.left;
  float paddingTop = box->padding.top;
  float paddingRight = box->padding.right;
  float effectiveWidth = EffectiveTextBoxWidth(box);
  switch (box->textAlign) {
    case TextAlign::Center:
      out->anchor = TextAnchor::Center;
      out->anchorX = paddingLeft + (effectiveWidth - paddingLeft - paddingRight) / 2;
      break;
    case TextAlign::End:
      out->anchor = TextAnchor::End;
      out->anchorX = effectiveWidth - paddingRight;
      break;
    case TextAlign::Justify:
      out->anchor = TextAnchor::Start;
      out->anchorX = paddingLeft;
      out->justifyWidth = effectiveWidth - paddingLeft - paddingRight;
      break;
    default:
      out->anchor = TextAnchor::Start;
      out->anchorX = paddingLeft;
      break;
  }
  out->offsetY = paddingTop;
}

// The transform matrix passed into writeTextWithLayout already includes
// renderPosition via BuildGroupMatrix, so anchor x/y are in TextBox-local
// coordinates starting from (0,0). Standalone Text (no TextBox) uses the
// element's own anchor / renderPosition directly.
static TextAnchoring ResolveTextAnchorAndOffset(const Text* text, const TextBox* box,
                                                const TextLayoutResult& layoutResult) {
  TextAnchoring out;
  if (box == nullptr) {
    out.anchor = text->textAnchor;
    auto renderPos = text->renderPosition();
    out.anchorX = renderPos.x;
    out.offsetY = renderPos.y;
    return out;
  }
  if (box->writingMode == WritingMode::Vertical) {
    ResolveVerticalAnchoring(text, box, layoutResult, &out);
  } else {
    ResolveHorizontalAnchoring(box, &out);
  }
  return out;
}

// Last non-empty line index for justify last-line detection.
static size_t FindLastNonEmptyLineIndex(const std::string& text,
                                        const std::vector<TextLayoutLineInfo>& lines) {
  size_t lastLineIndex = 0;
  for (size_t li = 0; li < lines.size(); ++li) {
    const auto& info = lines[li];
    if (info.byteStart < info.byteEnd &&
        info.byteEnd - info.byteStart <= text.size() - info.byteStart) {
      if (!text.substr(info.byteStart, info.byteEnd - info.byteStart).empty()) {
        lastLineIndex = li;
      }
    }
  }
  return lastLineIndex;
}

void SVGWriter::writeTextWithLayout(SVGBuilder& out, const Text* text, const FillStrokeInfo& fs,
                                    const TextLayoutResult& layoutResult, const Matrix& m,
                                    float alpha) {
  auto* mutableText = const_cast<Text*>(text);
  auto* lines = layoutResult.getTextLines(mutableText);

  bool isVertical = fs.textBox && fs.textBox->writingMode == WritingMode::Vertical;
  TextAnchoring anchoring = ResolveTextAnchorAndOffset(text, fs.textBox, layoutResult);

  std::string transform = MatrixToSVGTransform(m);
  if (!lines || lines->empty()) {
    // Vertical writing mode skips horizontal line layout (see TextLayout.cpp:318),
    // so textLines is nullptr and we must fall back to a single <text> element
    // that carries the writing-mode attribute. The vertical-mode text flows
    // top-to-bottom via CSS writing-mode="tb"; SVG renderers lay out the
    // characters along the block-flow axis of the <text> element.
    out.openElement("text");
    if (!transform.empty()) {
      out.addAttribute("transform", transform);
    }
    WriteSharedTextAttrs(out, text, anchoring.anchor);
    ApplyTextBoxBodyAttrs(out, fs.textBox);
    applyPainters(out, fs, {}, alpha);
    out.addRequiredAttribute("x", anchoring.anchorX);
    // In vertical mode offsetY already points to the correct inline-axis start;
    // in horizontal fallback the y coordinate needs a baseline shift by fontSize.
    out.addRequiredAttribute("y",
                             isVertical ? anchoring.offsetY : anchoring.offsetY + text->fontSize);
    out.closeElementWithText(text->text);
    return;
  }

  size_t lastLineIndex =
      anchoring.justifyWidth > 0 ? FindLastNonEmptyLineIndex(text->text, *lines) : 0;

  for (size_t lineIdx = 0; lineIdx < lines->size(); ++lineIdx) {
    auto& lineInfo = (*lines)[lineIdx];
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
    WriteSharedTextAttrs(out, text, anchoring.anchor);
    ApplyTextBoxBodyAttrs(out, fs.textBox);
    applyPainters(out, fs, {}, alpha);
    out.addRequiredAttribute("x", anchoring.anchorX);
    out.addRequiredAttribute("y", anchoring.offsetY + lineInfo.baselineY);
    if (anchoring.justifyWidth > 0 && lineIdx != lastLineIndex) {
      int spaceCount = CountWordSpaces(lineText);
      if (spaceCount > 0) {
        float extraSpace = anchoring.justifyWidth - lineInfo.lineWidth;
        float wordSpacing = extraSpace / static_cast<float>(spaceCount);
        out.addAttribute("word-spacing", FloatToString(wordSpacing));
      }
    }
    out.closeElementWithText(lineText);
  }
}

// One "run" inside a rich TextBox: a Text together with the Fill and Stroke
// painters that apply to it. Mirrors PPTWriter::RichTextRun so container-mode
// TextBox reproduces per-run fill/stroke pairing.
namespace {

struct SVGRichTextRun {
  const Text* text = nullptr;
  const Fill* fill = nullptr;
  const Stroke* stroke = nullptr;
};

// Walk TextBox children in source order, pairing every Text with its nearest
// enclosing Fill/Stroke. Direct Text children inherit `parentFill`/`parentStroke`;
// Texts nested in a Group use that Group's locally-collected Fill/Stroke,
// falling back to the parent's. Same contract as PPTExporter::CollectRichTextRuns.
void CollectSVGRichTextRuns(const std::vector<Element*>& elements, const Fill* parentFill,
                            const Stroke* parentStroke, std::vector<SVGRichTextRun>& outRuns) {
  for (auto* element : elements) {
    auto type = element->nodeType();
    if (type == NodeType::Text) {
      auto* t = static_cast<const Text*>(element);
      if (!t->text.empty()) {
        outRuns.push_back({t, parentFill, parentStroke});
      }
    } else if (type == NodeType::Group) {
      auto* g = static_cast<const Group*>(element);
      auto groupFs = CollectFillStroke(g->elements);
      const Fill* effectiveFill = groupFs.fill ? groupFs.fill : parentFill;
      const Stroke* effectiveStroke = groupFs.stroke ? groupFs.stroke : parentStroke;
      CollectSVGRichTextRuns(g->elements, effectiveFill, effectiveStroke, outRuns);
    }
  }
}

struct SVGLineEntry {
  size_t runIndex = 0;
  float baselineY = 0;
  float lineWidth = 0;
  uint32_t byteStart = 0;
  uint32_t byteEnd = 0;
};

bool CompareSVGLineEntryByBaselineY(const SVGLineEntry& a, const SVGLineEntry& b) {
  return a.baselineY < b.baselineY;
}

}  // namespace

void SVGWriter::writeTextBoxGroup(SVGBuilder& out, const TextBox* textBox,
                                  const std::vector<Element*>& elements, const Matrix& transform,
                                  float alpha) {
  auto topLevelFs = CollectFillStroke(elements);
  std::vector<SVGRichTextRun> runs;
  CollectSVGRichTextRuns(elements, topLevelFs.fill, topLevelFs.stroke, runs);
  if (runs.empty()) {
    return;
  }

  std::vector<Text*> mutableTexts;
  mutableTexts.reserve(runs.size());
  for (const auto& run : runs) {
    mutableTexts.push_back(const_cast<Text*>(run.text));
  }

  auto params = MakeTextBoxParams(textBox);
  auto layoutResult = TextLayout::Layout(mutableTexts, params, _layoutContext);

  // Collect per-run line metadata from the layout result. Each run may span
  // multiple visual lines (if it contains wrapping or newlines), and multiple
  // runs may share the same visual line (rich text spans on one line).
  bool useLineLayout = textBox->writingMode != WritingMode::Vertical;
  std::vector<SVGLineEntry> lineEntries;
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
        lineEntries.push_back({i, li.baselineY, li.lineWidth, li.byteStart, li.byteEnd});
      }
    }
    if (useLineLayout && lineEntries.empty()) {
      useLineLayout = false;
    }
  }

  if (!useLineLayout) {
    // Fallback: vertical writing mode or missing line info — emit per-run
    // independent <text> elements (original behavior).
    for (const auto& run : runs) {
      if (run.text->text.empty()) {
        continue;
      }
      FillStrokeInfo runFs = {};
      runFs.fill = run.fill ? run.fill : topLevelFs.fill;
      runFs.stroke = run.stroke ? run.stroke : topLevelFs.stroke;
      runFs.textBox = textBox;
      writeTextWithLayout(out, run.text, runFs, layoutResult, transform, alpha);
    }
    return;
  }

  // Compute TextBox-level positioning (shared by all lines). TextBox rich-text
  // is always horizontal here (vertical falls back above), so ResolveHorizontalAnchoring
  // gives us anchor/anchorX/offsetY/justifyWidth directly.
  TextAnchoring anchoring;
  ResolveHorizontalAnchoring(textBox, &anchoring);
  std::string svgTransform = MatrixToSVGTransform(transform);

  // Stable-sort entries by baselineY so that runs from different Text
  // elements sharing the same visual line stay in source order.
  std::stable_sort(lineEntries.begin(), lineEntries.end(), CompareSVGLineEntryByBaselineY);
  constexpr float baselineEpsilon = 0.5f;

  // Find the last visual line's baseline for justify last-line detection.
  float lastBaseline = 0;
  if (anchoring.justifyWidth > 0 && !lineEntries.empty()) {
    lastBaseline = lineEntries.back().baselineY;
  }

  // Group entries by visual line (matching baselineY) and emit one <text>
  // per line with <tspan> children for each run fragment.
  size_t i = 0;
  while (i < lineEntries.size()) {
    float baseline = lineEntries[i].baselineY;
    float lineWidth = lineEntries[i].lineWidth;
    bool isLastLine = std::fabs(baseline - lastBaseline) < baselineEpsilon;
    // Count word spaces across all runs on this visual line for word-spacing.
    int totalSpaces = 0;
    if (anchoring.justifyWidth > 0 && !isLastLine) {
      for (size_t j = i; j < lineEntries.size() &&
                         std::fabs(lineEntries[j].baselineY - baseline) < baselineEpsilon;
           ++j) {
        const auto& e = lineEntries[j];
        std::string t = runs[e.runIndex].text->text.substr(e.byteStart, e.byteEnd - e.byteStart);
        totalSpaces += CountWordSpaces(t);
      }
    }
    out.openElement("text");
    if (!svgTransform.empty()) {
      out.addAttribute("transform", svgTransform);
    }
    if (anchoring.anchor == TextAnchor::Center) {
      out.addAttribute("text-anchor", "middle");
    } else if (anchoring.anchor == TextAnchor::End) {
      out.addAttribute("text-anchor", "end");
    }
    ApplyTextBoxBodyAttrs(out, textBox);
    out.addRequiredAttribute("x", anchoring.anchorX);
    out.addRequiredAttribute("y", anchoring.offsetY + baseline);
    if (anchoring.justifyWidth > 0 && !isLastLine && totalSpaces > 0) {
      float wordSpacing = (anchoring.justifyWidth - lineWidth) / static_cast<float>(totalSpaces);
      out.addAttribute("word-spacing", FloatToString(wordSpacing));
    }
    out.closeElementStart();

    while (i < lineEntries.size() &&
           std::fabs(lineEntries[i].baselineY - baseline) < baselineEpsilon) {
      const auto& entry = lineEntries[i];
      const auto& run = runs[entry.runIndex];
      const Fill* effectiveFill = run.fill ? run.fill : topLevelFs.fill;
      const Stroke* effectiveStroke = run.stroke ? run.stroke : topLevelFs.stroke;
      std::string lineText =
          run.text->text.substr(entry.byteStart, entry.byteEnd - entry.byteStart);
      if (lineText.empty()) {
        ++i;
        continue;
      }
      out.openElement("tspan");
      WriteSharedTextAttrs(out, run.text, TextAnchor::Start);
      FillStrokeInfo runFs = {};
      runFs.fill = effectiveFill;
      runFs.stroke = effectiveStroke;
      applyPainters(out, runFs, {}, alpha);
      out.closeElementWithText(lineText);
      ++i;
    }
    out.closeElement();
  }
}

void SVGWriter::writeText(SVGBuilder& out, const Text* text, const FillStrokeInfo& fs,
                          const Matrix& m, float alpha) {
  if (text->text.empty()) {
    return;
  }

  if (_convertTextToPath && !text->glyphRuns.empty()) {
    writeTextAsPath(out, text, fs, m, alpha);
    return;
  }

  TextLayoutParams params = fs.textBox ? MakeTextBoxParams(fs.textBox) : MakeStandaloneParams(text);
  auto mutableText = const_cast<Text*>(text);
  auto layoutResult = TextLayout::Layout({mutableText}, params, _layoutContext);
  auto* textLines = layoutResult.getTextLines(mutableText);
  if (textLines && !textLines->empty()) {
    writeTextWithLayout(out, text, fs, layoutResult, m, alpha);
    return;
  }

  // Fallback for when TextLayout produces no line info (e.g. empty text after shaping).
  std::string transform = MatrixToSVGTransform(m);
  out.openElement("text");
  if (!transform.empty()) {
    out.addAttribute("transform", transform);
  }
  WriteSharedTextAttrs(out, text, text->textAnchor);
  ApplyTextBoxBodyAttrs(out, fs.textBox);
  applyPainters(out, fs, {}, alpha);
  auto renderPos = text->renderPosition();
  out.addAttributeIfNonZero("x", renderPos.x);
  out.addAttributeIfNonZero("y", renderPos.y);
  out.closeElementWithText(text->text);
}

//==============================================================================
// SVGWriter – rasterization fallback for SVG-unsupported features
//==============================================================================

const LayerBuildResult& SVGWriter::ensureBuildResult() {
  if (!_buildResultReady) {
    _buildResult = LayerBuilder::BuildWithMap(_doc);
    _buildResultReady = true;
  }
  return _buildResult;
}

bool SVGWriter::rasterizeLayerAsImage(SVGBuilder& out, const Layer* layer) {
  auto& buildResult = ensureBuildResult();
  auto it = buildResult.layerMap.find(layer);
  if (it == buildResult.layerMap.end() || !buildResult.root) {
    return false;
  }
  auto tgfxLayer = it->second;
  if (!tgfxLayer) {
    return false;
  }
  // The <image> is emitted into `out`, which is the parent <g>'s body — that <g>'s transform
  // chain already equals the ancestor transforms up to the target layer's parent. Compute the
  // bounds and render the PNG in that parent coordinate space so the <image>'s x/y/width/height
  // are naturally correct inside the parent <g>. Falls back to the document root when the layer
  // has no parent (e.g. top-level layers attached directly to root — parent equals root there,
  // so the behavior matches).
  auto coordinateSpace =
      tgfxLayer->parent() != nullptr ? tgfxLayer->parent() : buildResult.root.get();
  auto pixelScale = static_cast<float>(_rasterDPI) / 96.0f;
  auto pngData = RenderMaskedLayer(&_gpu, buildResult.root, tgfxLayer, coordinateSpace, pixelScale);
  if (!pngData || pngData->size() == 0) {
    return false;
  }
  auto bounds = tgfxLayer->getBounds(coordinateSpace, true);
  if (bounds.isEmpty()) {
    return false;
  }
  std::string href = "data:image/png;base64," + Base64Encode(pngData->bytes(), pngData->size());
  out.openElement("image");
  out.addAttribute("href", href);
  out.addRequiredAttribute("x", bounds.left);
  out.addRequiredAttribute("y", bounds.top);
  out.addRequiredAttribute("width", bounds.width());
  out.addRequiredAttribute("height", bounds.height());
  out.closeElementSelfClosing();
  return true;
}

//==============================================================================
// SVGWriter – element list and layer writing
//==============================================================================

namespace {

// Look for the first modifier-only <TextBox> in `elements` so it can be
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
// writer with the given painter applied. Each painter that renders a geometry
// goes through this function so that multi-fill / multi-stroke produces one
// SVG element per painter (overlapping in document order). Mirrors
// PPTWriter::emitGeometryWithFs.
void SVGWriter::emitGeometryWithFs(SVGBuilder& out, const AccumulatedGeometry& entry,
                                   const FillStrokeInfo& fs) {
  FillStrokeInfo localFs = fs;
  if (localFs.textBox == nullptr) {
    localFs.textBox = entry.textBox;
  }
  switch (entry.element->nodeType()) {
    case NodeType::Rectangle:
      writeRectangle(out, static_cast<const Rectangle*>(entry.element), localFs, entry.transform,
                     entry.alpha);
      break;
    case NodeType::Ellipse:
      writeEllipse(out, static_cast<const Ellipse*>(entry.element), localFs, entry.transform,
                   entry.alpha);
      break;
    case NodeType::Path:
      writePath(out, static_cast<const Path*>(entry.element), localFs, entry.transform,
                entry.alpha);
      break;
    case NodeType::Text: {
      auto* text = static_cast<const Text*>(entry.element);
      // GlyphRun-only Text (no readable content) carries pre-shaped glyph
      // outlines; the only way to render these is via path geometry regardless
      // of convertTextToPath — matches PPT's behaviour.
      bool glyphRunOnly = text->text.empty() && !text->glyphRuns.empty();
      if ((_convertTextToPath || glyphRunOnly) && !text->glyphRuns.empty()) {
        writeTextAsPath(out, text, localFs, entry.transform, entry.alpha);
      } else {
        writeText(out, text, localFs, entry.transform, entry.alpha);
      }
      break;
    }
    default:
      break;
  }
}

// Walk a single scope's element list, growing `accumulator` with new geometry
// and emitting a copy of every visible geometry whenever a painter is hit.
// `scopeStart` is the index where the current Group's scope begins — painters
// inside this scope can only render entries from [scopeStart, end), enforcing
// the "painters within the group only render geometry accumulated within the
// group" rule while allowing geometry to propagate upward when the scope
// unwinds. Mirrors PPTWriter::processVectorScope.
void SVGWriter::processVectorScope(SVGBuilder& out, const std::vector<Element*>& elements,
                                   const Matrix& transform, float alpha,
                                   const TextBox* parentTextBox,
                                   std::vector<AccumulatedGeometry>& accumulator,
                                   size_t scopeStart) {
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
          emitGeometryWithFs(out, accumulator[i], painterFs);
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
          // child Text is added to the accumulator with the box's transform /
          // alpha, and box-level params surface as their textBox. Resolve
          // path modifiers nested inside the box before walking.
          const std::vector<Element*> innerWalked = _resolver.resolve(tb->elements);
          processVectorScope(out, innerWalked, tbMatrix, tbAlpha, tb, accumulator,
                             accumulator.size());
        } else {
          // Native text-box rendering: container TextBox owns its own paragraph
          // shaping and fill/stroke pairing. Don't add its child Text into the
          // surrounding accumulator.
          writeTextBoxGroup(out, tb, tb->elements, tbMatrix, tbAlpha);
        }
        break;
      }
      case NodeType::Group: {
        auto* group = static_cast<const Group*>(element);
        Matrix groupMatrix = transform * BuildGroupMatrix(group);
        float groupAlpha = alpha * group->alpha;
        const std::vector<Element*> innerWalked = _resolver.resolve(group->elements);
        processVectorScope(out, innerWalked, groupMatrix, groupAlpha, localTextBox, accumulator,
                           accumulator.size());
        break;
      }
      case NodeType::Repeater:
        // Any Repeater still surviving here means the resolver intentionally
        // erased its scope (copies==0) or the content arrived already
        // flattened; silently skip so the remaining content still renders.
        break;
      default:
        // TextPath, TextModifier, RangeSelector, Polystar (post-resolution
        // should be gone), and unrecognized types fall through silently.
        break;
    }
  }
}

void SVGWriter::writeElements(SVGBuilder& out, const std::vector<Element*>& elements,
                              const Matrix& transform, float alpha, const TextBox* parentTextBox) {
  // Bake every path-modifier (Polystar -> Path, Repeater -> grouped copies,
  // TrimPath / RoundCorner / MergePath -> editable Path). Painters, Group,
  // Text, and TextBox pass through unchanged so the accumulator walk below
  // operates on normalized geometry.
  const std::vector<Element*> walked = _resolver.resolve(elements);

  std::vector<AccumulatedGeometry> accumulator;
  accumulator.reserve(walked.size());
  processVectorScope(out, walked, transform, alpha, parentTextBox, accumulator, /*scopeStart=*/0);
}

void SVGWriter::writeLayerContents(SVGBuilder& out, const Layer* layer, const Matrix& transform) {
  writeElements(out, layer->contents, transform, 1.0f, nullptr);
}

void SVGWriter::writeLayer(SVGBuilder& out, const Layer* layer) {
  if (!layer->visible && layer->mask == nullptr) {
    return;
  }

  // Probe for features SVG cannot losslessly represent (TextPath, TextModifier,
  // diamond/conic gradient). If any trip AND rasterization is enabled, bake the whole layer to a
  // PNG so the visual result is preserved. The alternative (silently dropping unsupported
  // elements / degrading gradients to radial) is what the vector path below does.
  // BackgroundBlurStyle is intentionally excluded: SVG has no portable backdrop-blur primitive,
  // so the layer is kept as vector with the blur effect silently dropped.
  if (_rasterizeUnsupportedFeatures) {
    auto features = ProbeLayerFeaturesForSVG(layer);
    if (features.needsRasterization()) {
      if (rasterizeLayerAsImage(out, layer)) {
        return;
      }
    }
  }

  auto renderPos = layer->renderPosition();
  bool needsGroup = !layer->matrix.isIdentity() || !layer->matrix3D.isIdentity() ||
                    layer->alpha < 1.0f || !layer->id.empty() || !layer->filters.empty() ||
                    !layer->styles.empty() || layer->mask != nullptr || renderPos.x != 0.0f ||
                    renderPos.y != 0.0f || !layer->customData.empty() ||
                    layer->blendMode != BlendMode::Normal || layer->hasScrollRect;

  if (!needsGroup) {
    writeLayerBody(out, layer);
    return;
  }

  out.openElement("g");
  writeLayerGroupAttributes(out, layer);

  bool hasContent =
      !layer->contents.empty() || !layer->children.empty() || layer->composition != nullptr;
  if (!hasContent) {
    out.closeElementSelfClosing();
    return;
  }

  out.closeElementStart();

  // When scrollRect is active, wrap all content in an inner <g> that shifts
  // the content by (-scrollRect.x, -scrollRect.y) so that the scroll origin
  // aligns with the viewport's (0,0).
  bool hasScrollOffset =
      layer->hasScrollRect && (layer->scrollRect.x != 0.0f || layer->scrollRect.y != 0.0f);
  if (hasScrollOffset) {
    out.openElement("g");
    out.addAttribute("transform", "translate(" + FloatToString(-layer->scrollRect.x) + "," +
                                      FloatToString(-layer->scrollRect.y) + ")");
    out.closeElementStart();
  }

  writeLayerBody(out, layer);

  if (hasScrollOffset) {
    out.closeElement();
  }

  out.closeElement();
}

void SVGWriter::writeLayerBody(SVGBuilder& out, const Layer* layer) {
  writeLayerContents(out, layer);
  if (layer->composition != nullptr) {
    for (const auto* compLayer : layer->composition->layers) {
      writeLayer(out, compLayer);
    }
  }
  for (const auto* child : layer->children) {
    writeLayer(out, child);
  }
}

void SVGWriter::writeLayerGroupAttributes(SVGBuilder& out, const Layer* layer) {
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

  if (!layer->filters.empty() || !layer->styles.empty()) {
    auto filterId = writeFilterAndStyleDefs(layer->filters, layer->styles);
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

  // scrollRect clips the layer's content to a viewport rectangle and offsets
  // the content by the scroll origin. Unlike PPT (which must bake to PNG),
  // SVG natively supports <clipPath> so we emit a vector clip.
  if (layer->hasScrollRect) {
    auto scrollClipId = writeScrollRectClipDef(layer);
    out.addAttribute("clip-path", "url(#" + scrollClipId + ")");
  }
}

std::string SVGWriter::writeScrollRectClipDef(const Layer* layer) {
  auto& sr = layer->scrollRect;
  std::string scrollClipId = generateId("scrollClip");
  _defs->openElement("clipPath");
  _defs->addAttribute("id", scrollClipId);
  _defs->closeElementStart();
  _defs->openElement("rect");
  _defs->addRequiredAttribute("width", sr.width);
  _defs->addRequiredAttribute("height", sr.height);
  _defs->closeElementSelfClosing();
  _defs->closeElement();
  return scrollClipId;
}

//==============================================================================
// Main Export function
//==============================================================================

std::string SVGExporter::ToSVG(PAGXDocument& doc, const Options& options) {
  // Mirror PPTExporter: resolve renderPosition() for every layer/group so authored x/y/position
  // attributes flow into the matrix helpers (BuildLayerMatrix / BuildGroupMatrix).
  if (!doc.isLayoutApplied()) {
    doc.applyLayout(options.fontConfig);
  }
  SVGBuilder svg(true, options.indent);
  SVGBuilder defs(true, options.indent, 2);
  SVGWriterContext context;
  auto layoutContext = std::make_unique<LayoutContext>(options.fontConfig);
  SVGWriter writer(&defs, &context, options.indent, options.convertTextToPath, layoutContext.get(),
                   &doc, options.rasterizeUnsupportedFeatures, options.rasterDPI);

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

bool SVGExporter::ToFile(PAGXDocument& document, const std::string& filePath,
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
