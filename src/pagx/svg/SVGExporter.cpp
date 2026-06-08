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
#include <limits>
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
#include "pagx/nodes/NoiseFilter.h"
#include "pagx/nodes/NoiseStyle.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/nodes/TextPath.h"
#include "pagx/svg/SVGBlendMode.h"
#include "pagx/svg/SVGFeatureProbe.h"
#include "pagx/svg/SVGPathParser.h"
#include "pagx/types/Rect.h"
#include "pagx/utils/Base64.h"
#include "pagx/utils/ExporterUtils.h"
#include "pagx/utils/ImageFormatUtils.h"
#include "pagx/utils/ModifierResolver.h"
#include "pagx/utils/RasterUtils.h"
#include "pagx/utils/StringParser.h"
#include "pagx/utils/StrokeGeometryUtils.h"
#include "pagx/utils/TextUtils.h"
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

static void ExpandBounds(float& minX, float& minY, float& maxX, float& maxY, const Rect& bounds) {
  if (bounds.isEmpty()) {
    return;
  }
  minX = std::min(minX, bounds.x);
  minY = std::min(minY, bounds.y);
  maxX = std::max(maxX, bounds.x + bounds.width);
  maxY = std::max(maxY, bounds.y + bounds.height);
}

static Rect IntersectRects(const Rect& a, const Rect& b) {
  if (a.isEmpty() || b.isEmpty()) {
    return {};
  }
  float left = std::max(a.x, b.x);
  float top = std::max(a.y, b.y);
  float right = std::min(a.x + a.width, b.x + b.width);
  float bottom = std::min(a.y + a.height, b.y + b.height);
  if (left >= right || top >= bottom) {
    return {};
  }
  return {left, top, right - left, bottom - top};
}

static Rect ComputeElementBounds(const Element* element);

// std::pow(negative, non-integer) returns NaN. Raise magnitude and reapply sign.
static float SignedPow(float base, float exp) {
  float sign = base < 0.0f ? -1.0f : 1.0f;
  return sign * std::pow(std::abs(base), exp);
}

// Apply a 2D affine matrix to a Rect, returning the axis-aligned bounding box
// of the transformed corners.
static Rect MapRect(const Matrix& m, const Rect& r) {
  if (r.isEmpty()) {
    return {};
  }
  float x0 = r.x, y0 = r.y;
  float x1 = r.x + r.width, y1 = r.y + r.height;
  auto tl = m.mapPoint({x0, y0});
  auto tr = m.mapPoint({x1, y0});
  auto bl = m.mapPoint({x0, y1});
  auto br = m.mapPoint({x1, y1});
  float minX = std::min({tl.x, tr.x, bl.x, br.x});
  float minY = std::min({tl.y, tr.y, bl.y, br.y});
  float maxX = std::max({tl.x, tr.x, bl.x, br.x});
  float maxY = std::max({tl.y, tr.y, bl.y, br.y});
  return {minX, minY, maxX - minX, maxY - minY};
}

// Build the per-copy transform matrix for a Repeater copy, mirroring the logic
// in ModifierResolver::MakeCopyGroup and BuildGroupMatrix.
static Matrix BuildRepeaterCopyMatrix(const Repeater* rep, float progress) {
  float tx = rep->position.x * progress;
  float ty = rep->position.y * progress;
  float sx = SignedPow(rep->scale.x, progress);
  float sy = SignedPow(rep->scale.y, progress);
  float rotation = rep->rotation * progress;
  Matrix m = {};
  if (!FloatNearlyZero(rep->anchor.x) || !FloatNearlyZero(rep->anchor.y)) {
    m = Matrix::Translate(-rep->anchor.x, -rep->anchor.y);
  }
  if (!FloatNearlyZero(sx - 1.0f) || !FloatNearlyZero(sy - 1.0f)) {
    m = Matrix::Scale(sx, sy) * m;
  }
  if (!FloatNearlyZero(rotation)) {
    m = Matrix::Rotate(rotation) * m;
  }
  m = Matrix::Translate(tx + rep->anchor.x, ty + rep->anchor.y) * m;
  return m;
}

static Rect ComputeLayerBounds(const Layer* layer);

static Rect ComputeElementsBounds(const std::vector<Element*>& elements) {
  float minX = std::numeric_limits<float>::max();
  float minY = std::numeric_limits<float>::max();
  float maxX = -std::numeric_limits<float>::max();
  float maxY = -std::numeric_limits<float>::max();
  std::vector<Element*> preceding;

  for (const auto* element : elements) {
    if (element->nodeType() == NodeType::Repeater) {
      auto rep = static_cast<const Repeater*>(element);
      if (rep->copies < 0.0f) {
        continue;
      }
      if (rep->copies == 0.0f) {
        return {};
      }
      if (preceding.empty()) {
        continue;
      }
      auto bodyBounds = ComputeElementsBounds(preceding);
      constexpr float MAX_REPEATER_COPIES = 10000.0f;
      float copiesF = std::min(rep->copies, MAX_REPEATER_COPIES);
      int maxCount = static_cast<int>(std::ceil(copiesF));
      for (int i = 0; i < maxCount; i++) {
        float progress = static_cast<float>(i) + rep->offset;
        auto copyMatrix = BuildRepeaterCopyMatrix(rep, progress);
        auto copyBounds = MapRect(copyMatrix, bodyBounds);
        ExpandBounds(minX, minY, maxX, maxY, copyBounds);
      }
      continue;
    }
    auto bounds = ComputeElementBounds(element);
    if (bounds.width > 0 || bounds.height > 0) {
      ExpandBounds(minX, minY, maxX, maxY, bounds);
    }
    preceding.push_back(const_cast<Element*>(element));
  }
  if (minX > maxX) {
    return {};
  }
  return {minX, minY, maxX - minX, maxY - minY};
}

static Rect ComputeElementBounds(const Element* element) {
  switch (element->nodeType()) {
    case NodeType::Rectangle: {
      auto rect = static_cast<const Rectangle*>(element);
      auto pos = rect->renderPosition();
      auto size = rect->renderSize();
      return {pos.x - size.width * 0.5f, pos.y - size.height * 0.5f, size.width, size.height};
    }
    case NodeType::Ellipse: {
      auto ellipse = static_cast<const Ellipse*>(element);
      auto pos = ellipse->renderPosition();
      auto size = ellipse->renderSize();
      return {pos.x - size.width * 0.5f, pos.y - size.height * 0.5f, size.width, size.height};
    }
    case NodeType::Path: {
      auto pathNode = static_cast<const Path*>(element);
      if (pathNode->data == nullptr || pathNode->data->isEmpty()) {
        return {};
      }
      auto bounds = pathNode->data->getBounds();
      auto scale = pathNode->renderScale();
      bounds.x *= scale;
      bounds.y *= scale;
      bounds.width *= scale;
      bounds.height *= scale;
      auto pos = pathNode->renderPosition();
      return {pos.x + bounds.x, pos.y + bounds.y, bounds.width, bounds.height};
    }
    case NodeType::Polystar: {
      auto polystar = static_cast<const Polystar*>(element);
      auto pos = polystar->renderPosition();
      auto outerRadius = polystar->renderOuterRadius();
      return {pos.x - outerRadius, pos.y - outerRadius, outerRadius * 2, outerRadius * 2};
    }
    case NodeType::Group: {
      auto group = static_cast<const Group*>(element);
      auto childBounds = ComputeElementsBounds(group->elements);
      auto groupMatrix = BuildGroupMatrix(group);
      if (groupMatrix.isIdentity()) {
        return childBounds;
      }
      return MapRect(groupMatrix, childBounds);
    }
    case NodeType::Text: {
      auto text = static_cast<const Text*>(element);
      return text->contentBounds();
    }
    case NodeType::TextPath: {
      auto textPath = static_cast<const TextPath*>(element);
      if (textPath->path == nullptr || textPath->path->isEmpty()) {
        return textPath->layoutBounds();
      }
      auto bounds = textPath->path->getBounds();
      auto scale = textPath->renderScale();
      bounds.x *= scale;
      bounds.y *= scale;
      bounds.width *= scale;
      bounds.height *= scale;
      auto pos = textPath->renderPosition();
      return {pos.x + bounds.x, pos.y + bounds.y, bounds.width, bounds.height};
    }
    case NodeType::TextBox: {
      auto textBox = static_cast<const TextBox*>(element);
      return textBox->contentBounds();
    }
    default:
      return {};
  }
}

static Rect ComputeLayerBounds(const Layer* layer) {
  float minX = std::numeric_limits<float>::max();
  float minY = std::numeric_limits<float>::max();
  float maxX = -std::numeric_limits<float>::max();
  float maxY = -std::numeric_limits<float>::max();

  auto contentsBounds = ComputeElementsBounds(layer->contents);
  ExpandBounds(minX, minY, maxX, maxY, contentsBounds);

  if (layer->composition != nullptr) {
    for (const auto* compLayer : layer->composition->layers) {
      if (!compLayer->visible) continue;
      auto compBounds = ComputeLayerBounds(compLayer);
      auto compMatrix = BuildLayerMatrix(compLayer);
      auto mappedBounds = MapRect(compMatrix, compBounds);
      if (compLayer->hasScrollRect) {
        auto scrollRect = MapRect(compMatrix, compLayer->scrollRect);
        mappedBounds = IntersectRects(mappedBounds, scrollRect);
        if (mappedBounds.isEmpty()) continue;
      }
      if (compLayer->mask != nullptr) {
        auto maskBounds = ComputeLayerBounds(compLayer->mask);
        auto maskMatrix = BuildLayerMatrix(compLayer->mask);
        auto mappedMask = MapRect(maskMatrix, maskBounds);
        mappedBounds = IntersectRects(mappedBounds, mappedMask);
        if (mappedBounds.isEmpty()) continue;
      }
      ExpandBounds(minX, minY, maxX, maxY, mappedBounds);
    }
  }

  for (const auto* child : layer->children) {
    if (!child->visible) continue;
    auto childBounds = ComputeLayerBounds(child);
    auto childMatrix = BuildLayerMatrix(child);
    auto mappedBounds = MapRect(childMatrix, childBounds);
    if (child->hasScrollRect) {
      auto scrollRect = MapRect(childMatrix, child->scrollRect);
      mappedBounds = IntersectRects(mappedBounds, scrollRect);
      if (mappedBounds.isEmpty()) continue;
    }
    if (child->mask != nullptr) {
      auto maskBounds = ComputeLayerBounds(child->mask);
      auto maskMatrix = BuildLayerMatrix(child->mask);
      auto mappedMask = MapRect(maskMatrix, maskBounds);
      mappedBounds = IntersectRects(mappedBounds, mappedMask);
      if (mappedBounds.isEmpty()) continue;
    }
    ExpandBounds(minX, minY, maxX, maxY, mappedBounds);
  }

  if (minX > maxX) {
    return {};
  }
  return {minX, minY, maxX - minX, maxY - minY};
}

// feGaussianBlur stdDeviation string: one value when blurX == blurY, otherwise two.
// Compare via the formatted strings so ULP-level differences from upstream transform
// scaling don't emit redundant anisotropic stdDeviation that browsers would honour.
static std::string FormatBlurStdDev(float blurX, float blurY) {
  std::string xStr = FloatToString(blurX);
  std::string yStr = FloatToString(blurY);
  if (xStr == yStr) {
    return xStr;
  }
  std::string stdDev = std::move(xStr);
  stdDev += " ";
  stdDev += yStr;
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

// Returns true when the painter's color source is a gradient (of any kind).
// A gradient cannot be applied to a <g> that wraps multiple child <path>s while
// each path carries its own transform: SVG resolves both objectBoundingBox and
// userSpaceOnUse gradients in each referencing element's own coordinate space,
// so every glyph re-fits the gradient to itself. The transform must therefore
// be baked into the path data so all glyphs share one coordinate space — this
// is required regardless of fitsToGeometry (true also needs the run bounds baked
// into gradientTransform; false just needs the consistent coordinate space).
static bool IsGradientSource(const ColorSource* source) {
  if (source == nullptr) {
    return false;
  }
  switch (source->nodeType()) {
    case NodeType::LinearGradient:
    case NodeType::RadialGradient:
    case NodeType::ConicGradient:
    case NodeType::DiamondGradient:
      return true;
    default:
      return false;
  }
}

// Detects an image MIME type from the leading bytes of the encoded stream so that
// data URIs declare the actual format (PNG/JPEG/WebP). Returns nullptr when the
// magic bytes match no known signature so callers can surface the degradation.
static const char* DetectImageMimeType(const uint8_t* data, size_t size) {
  if (size >= 8 && data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47) {
    return "image/png";
  }
  if (size >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) {
    return "image/jpeg";
  }
  if (size >= 12 && data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F' &&
      data[8] == 'W' && data[9] == 'E' && data[10] == 'B' && data[11] == 'P') {
    return "image/webp";
  }
  return nullptr;
}

// Encodes the image bytes as a data URI. Falls back to declaring image/png when the
// signature does not match a known format — preserves the long-standing legacy
// behaviour. When `recognized` is non-null it is set to false in the fallback path so
// callers can surface a warning to the caller-supplied diagnostics channel.
static std::string EncodeImageDataURI(const uint8_t* bytes, size_t size, bool* recognized) {
  const char* mime = DetectImageMimeType(bytes, size);
  if (recognized != nullptr) {
    *recognized = (mime != nullptr);
  }
  std::string href = "data:";
  href += (mime != nullptr ? mime : "image/png");
  href += ";base64,";
  href += Base64Encode(bytes, size);
  return href;
}

// SVG cross-environment usage (e.g. embedded in HTML, opened in a browser) cannot
// rely on absolute filesystem paths resolving, so prefer to inline the encoded image
// bytes as a data URI. Reads from disk on demand when only filePath is populated. When
// reading fails, return empty so the caller skips the asset rather than embedding a
// host-local path that would leak filesystem layout and never resolve elsewhere. The
// optional `mimeRecognized` outparam is set to false when the encoded bytes do not
// match any known format signature so the caller can record a warning.
static std::string GetImageHref(const Image* image, bool* mimeRecognized = nullptr) {
  if (image->data) {
    return EncodeImageDataURI(image->data->bytes(), image->data->size(), mimeRecognized);
  }
  if (!image->filePath.empty()) {
    auto data = GetImageData(image);
    if (data && data->size() > 0) {
      return EncodeImageDataURI(data->bytes(), data->size(), mimeRecognized);
    }
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
            PAGXDocument* doc = nullptr, bool bakeUnsupported = true, float rasterScale = 2.0f,
            bool resolveModifiers = true, std::vector<std::string>* warnings = nullptr)
      : _defs(defs), _context(context), _indentSpaces(indentSpaces),
        _convertTextToPath(convertTextToPath), _bakeUnsupported(bakeUnsupported),
        _rasterScale(rasterScale), _resolveModifiers(resolveModifiers), _warnings(warnings),
        _layoutContext(layoutContext), _resolver(doc), _doc(doc) {
  }

  void writeLayer(SVGBuilder& out, const Layer* layer);

 private:
  SVGBuilder* _defs = nullptr;
  SVGWriterContext* _context = nullptr;
  int _indentSpaces = 2;
  bool _convertTextToPath = true;
  bool _bakeUnsupported = true;
  float _rasterScale = 2.0f;
  bool _resolveModifiers = true;
  std::vector<std::string>* _warnings = nullptr;
  LayoutContext* _layoutContext = nullptr;
  ModifierResolver _resolver;
  PAGXDocument* _doc = nullptr;
  GPUContext _gpu;
  LayerBuildResult _buildResult = {};
  bool _buildResultReady = false;

  Rect _gradientUserSpaceBounds = {};

  std::string generateId(const std::string& prefix) {
    return _context->generateId(prefix);
  }

  void addWarning(std::string message) {
    if (_warnings != nullptr) {
      _warnings->push_back(std::move(message));
    }
  }

  std::vector<Element*> resolveIfEnabled(const std::vector<Element*>& elements) {
    if (_resolveModifiers) {
      return _resolver.resolve(elements);
    }
    return elements;
  }

  const LayerBuildResult& ensureBuildResult();

  bool rasterizeLayerAsImage(SVGBuilder& out, const Layer* layer);

  struct AccumulatedGeometry {
    const Element* element = nullptr;
    Matrix transform = {};
    const TextBox* textBox = nullptr;
  };

  // Layer / element writing
  void writeLayerContents(SVGBuilder& out, const Layer* layer, const Matrix& transform = {},
                          float alpha = 1.0f);
  void writeElements(SVGBuilder& out, const std::vector<Element*>& elements,
                     const Matrix& transform, float alpha, const TextBox* parentTextBox = nullptr);
  void processVectorScope(SVGBuilder& out, const std::vector<Element*>& elements,
                          const Matrix& transform, float alpha, const TextBox* parentTextBox,
                          std::vector<AccumulatedGeometry>& accumulator, size_t scopeStart);
  void emitGeometryWithFs(SVGBuilder& out, const AccumulatedGeometry& entry,
                          const FillStrokeInfo& fs, float alpha);

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
  void finishGradientDef(const Matrix& matrix, const std::vector<ColorStop*>& stops,
                         bool fitsToGeometry);
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
  // Image-transforming filter primitives. Each reads from `currentSource` and
  // updates it to its own result so that a chain of filters (e.g. Blur →
  // ColorMatrix → Blend) flows through consecutive primitives rather than
  // each re-reading the original SourceGraphic.
  void writeColorMatrixFilter(const ColorMatrixFilter* cm, int& colorMatrixIndex,
                              std::string& currentSource);
  void writeBlendFilter(const BlendFilter* blend, int& shadowIndex, std::string& currentSource);
  std::string writeNoiseTurbulence(const NoiseFilter* noise, const std::string& resultName,
                                   const Rect& contentBounds);
  std::string writeNoiseTurbulence(const NoiseStyle* noise, const std::string& resultName,
                                   const Rect& contentBounds);
  std::string writeNoiseBand(const NoiseFilter* noise, bool isDark, const std::string& label,
                             const Rect& contentBounds);
  std::string writeNoiseBand(const NoiseStyle* noise, bool isDark, const std::string& label,
                             const Rect& contentBounds);
  std::string writeNoiseFilter(const NoiseFilter* noise, int& noiseIndex,
                               std::string& currentSource, const Rect& contentBounds);
  // Collected per-filter state fed into the final feMerge aggregation.
  struct ShadowAggregate {
    std::vector<std::string> dropShadowResults;
    std::vector<std::string> aboveResults;
    std::vector<std::string> innerShadowResults;
    bool needSourceGraphic = false;
  };
  std::string writeNoiseStyle(const NoiseStyle* noise, int& noiseStyleIndex,
                              const Rect& contentBounds);
  void writeFilterList(const std::vector<LayerFilter*>& filters, int& shadowIndex,
                       ShadowAggregate& agg, std::string& currentSource, const Rect& contentBounds);
  void writeStyleList(const std::vector<LayerStyle*>& styles, int& shadowIndex,
                      ShadowAggregate& agg, const Rect& contentBounds);
  void writeShadowMerge(const ShadowAggregate& agg, const std::string& currentSource);
  std::string writeFilterAndStyleDefs(const std::vector<LayerFilter*>& filters,
                                      const std::vector<LayerStyle*>& styles,
                                      const Rect& contentBounds = {});

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
  // `emitScrollClipHere` is false when the caller will emit the scrollRect
  // clip-path on a deferred middle <g> wrapper to avoid colliding with a
  // contour-mask clip-path on the same element (both share the SVG `clip-path`
  // attribute, so writing both onto the outer <g> drops the first).
  void writeLayerGroupAttributes(SVGBuilder& out, const Layer* layer,
                                 bool emitScrollClipHere = true);
  // Emits contents + composition layers + child layers. Used both for the
  // needs-group path (inside the <g>) and for the bare-through path.
  void writeLayerBody(SVGBuilder& out, const Layer* layer, float perChildAlpha = 1.0f);

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

void SVGWriter::finishGradientDef(const Matrix& matrix, const std::vector<ColorStop*>& stops,
                                  bool fitsToGeometry) {
  // When fitsToGeometry is true the gradient coordinates live in normalized
  // (0-1) space mapped to each geometry's bounding box — SVG's
  // objectBoundingBox mode provides exactly this semantic. When false the
  // coordinates are in the parent container's document space, so userSpaceOnUse
  // is correct.
  Matrix effectiveMatrix = matrix;
  bool useUserSpace = !fitsToGeometry;
  if (fitsToGeometry && !_gradientUserSpaceBounds.isEmpty()) {
    // The painter is shared across multiple child geometries (e.g. the
    // per-glyph <path>s of a text run), so objectBoundingBox would re-fit the
    // gradient to each glyph's own box. Bake the run's bounding box into the
    // gradient transform and emit userSpaceOnUse so the normalized (0-1)
    // coordinates fit the whole run once. The unit-box → run-box matrix is
    // pre-multiplied so it applies after the gradient's own matrix, matching
    // objectBoundingBox's coordinate order.
    const Rect& b = _gradientUserSpaceBounds;
    Matrix bboxMatrix = {b.width, 0.0f, 0.0f, b.height, b.x, b.y};
    effectiveMatrix = bboxMatrix * matrix;
    useUserSpace = true;
  }
  _defs->addAttribute("gradientUnits", useUserSpace ? "userSpaceOnUse" : "objectBoundingBox");
  if (!effectiveMatrix.isIdentity()) {
    _defs->addAttribute("gradientTransform", MatrixToSVGTransform(effectiveMatrix));
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
      finishGradientDef(grad->matrix, grad->colorStops, grad->fitsToGeometry);
      break;
    }
    case NodeType::RadialGradient: {
      auto grad = static_cast<const RadialGradient*>(source);
      _defs->openElement("radialGradient");
      _defs->addAttribute("id", id);
      _defs->addRequiredAttribute("cx", grad->center.x);
      _defs->addRequiredAttribute("cy", grad->center.y);
      _defs->addRequiredAttribute("r", grad->radius);
      finishGradientDef(grad->matrix, grad->colorStops, grad->fitsToGeometry);
      break;
    }
    case NodeType::ConicGradient: {
      // SVG has no conic/sweep gradient primitive (CSS `conic-gradient` is a
      // paint only available in HTML). Degrade to a radial gradient centred
      // at the conic's center so the stops still appear; the angular
      // distribution is lost — matches PPT's path="circle" fallback. When
      // fitsToGeometry is true the center is already in normalized space and
      // objectBoundingBox handles it; when false use a fixed pixel radius.
      auto grad = static_cast<const ConicGradient*>(source);
      _defs->addRawContent(std::string(_indentSpaces, ' ') +
                           "<!-- conic gradient degraded to radial -->\n");
      _defs->openElement("radialGradient");
      _defs->addAttribute("id", id);
      _defs->addRequiredAttribute("cx", grad->center.x);
      _defs->addRequiredAttribute("cy", grad->center.y);
      _defs->addAttribute("r", grad->fitsToGeometry ? 0.5f : 100.0f);
      finishGradientDef(grad->matrix, grad->colorStops, grad->fitsToGeometry);
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
      finishGradientDef(grad->matrix, grad->colorStops, grad->fitsToGeometry);
      break;
    }
    default:
      break;
  }
}

// Computes the axis-aligned document-space rect occupied by an ImagePattern's image after
// the pattern matrix and scaleMode fit are applied. Returns false when the matrix carries
// rotation, skew, or negative scale — those cases have no faithful <image>/preserveAspectRatio
// representation and the caller must fall back to the matrix-driven path. Mirrors PPT's
// ComputePlacedImageRect so SVG and PPTX agree on the placement contract.
static bool ComputePlacedImageRectSVG(const ImagePattern* pattern, int imgW, int imgH,
                                      const Rect& shapeBounds, Rect* result) {
  const auto& M = pattern->matrix;
  if (std::fabs(M.b) > 1e-6f || std::fabs(M.c) > 1e-6f) {
    return false;
  }
  if (M.a <= 0 || M.d <= 0) {
    return false;
  }
  float transformedW = static_cast<float>(imgW) * M.a;
  float transformedH = static_cast<float>(imgH) * M.d;
  if (transformedW <= 0 || transformedH <= 0) {
    return false;
  }
  if (pattern->scaleMode == ScaleMode::None) {
    *result = Rect::MakeXYWH(M.tx, M.ty, transformedW, transformedH);
    return true;
  }
  if (shapeBounds.isEmpty()) {
    return false;
  }
  float sx = shapeBounds.width / transformedW;
  float sy = shapeBounds.height / transformedH;
  switch (pattern->scaleMode) {
    case ScaleMode::Stretch:
      *result = Rect::MakeXYWH(shapeBounds.x, shapeBounds.y, shapeBounds.width, shapeBounds.height);
      return true;
    case ScaleMode::LetterBox: {
      float s = std::min(sx, sy);
      float w = transformedW * s;
      float h = transformedH * s;
      *result = Rect::MakeXYWH(shapeBounds.x + (shapeBounds.width - w) / 2.0f,
                               shapeBounds.y + (shapeBounds.height - h) / 2.0f, w, h);
      return true;
    }
    case ScaleMode::Zoom: {
      float s = std::max(sx, sy);
      float w = transformedW * s;
      float h = transformedH * s;
      *result = Rect::MakeXYWH(shapeBounds.x + (shapeBounds.width - w) / 2.0f,
                               shapeBounds.y + (shapeBounds.height - h) / 2.0f, w, h);
      return true;
    }
    case ScaleMode::None:
      break;
  }
  return false;
}

std::string SVGWriter::writeImagePatternDef(const ImagePattern* pattern, const Rect& shapeBounds) {
  if (!pattern->image) {
    return {};
  }
  bool mimeRecognized = true;
  std::string href = GetImageHref(pattern->image, &mimeRecognized);
  if (href.empty()) {
    addWarning(
        "ImagePattern dropped: image bytes unavailable (no inline data and on-disk read "
        "from filePath '" +
        pattern->image->filePath + "' failed); the affected fill will be empty.");
    return {};
  }
  if (!mimeRecognized) {
    addWarning(
        "ImagePattern: encoded bytes do not match PNG/JPEG/WebP signature; declared as image/png "
        "fallback — viewers may fail to decode.");
  }

  std::string defId = generateId("pattern");

  // Detect which tile modes we need to handle. Decal is intentionally excluded from
  // needsBaking: a Decal-only pattern paints the image once with the matrix/scaleMode
  // placement and leaves the outside transparent, which is already what the native
  // non-baking branch below emits (and crucially RenderTiledPattern ignores scaleMode,
  // so baking would crop the image to the shape origin and lose LetterBox/Zoom/Stretch
  // fit). Mirror and Clamp have no portable SVG primitive and still require baking.
  bool needsNativeTiling =
      (pattern->tileModeX == TileMode::Repeat || pattern->tileModeY == TileMode::Repeat);
  bool needsBaking =
      (pattern->tileModeX == TileMode::Mirror || pattern->tileModeY == TileMode::Mirror ||
       pattern->tileModeX == TileMode::Clamp || pattern->tileModeY == TileMode::Clamp);

  // Try baking for unsupported tile modes (Mirror, Clamp)
  if (needsBaking && !shapeBounds.isEmpty()) {
    int w = static_cast<int>(ceilf(shapeBounds.width));
    int h = static_cast<int>(ceilf(shapeBounds.height));
    float offsetX = pattern->matrix.tx - shapeBounds.x;
    float offsetY = pattern->matrix.ty - shapeBounds.y;
    float pixelScale = _rasterScale;

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
    // Baking failed for Mirror/Clamp. SVG has no native equivalent for those wrap
    // modes, so the fall-through below either degrades them to Repeat (when the
    // other axis is Repeat) or paints a single tile across the shape. Surface the
    // semantic loss explicitly instead of silently picking the Repeat path.
    addWarning(needsNativeTiling
                   ? "ImagePattern: Mirror/Clamp tile bake failed; the Mirror/Clamp axis "
                     "falls back to Repeat tiling and loses its wrap mode."
                   : "ImagePattern: Mirror/Clamp tile bake failed; falling back to a single "
                     "image fill, the Mirror/Clamp wrap mode will be lost.");
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
    // image appears exactly once. Compute the placed image rect (matrix +
    // scaleMode) so LetterBox / Zoom / Stretch produce the correct fit instead
    // of stretching the raw image across each shape (ScaleMode::None preserves
    // the prior matrix-driven behaviour).
    _defs->addAttributeIfNonZero("x", shapeBounds.x);
    _defs->addAttributeIfNonZero("y", shapeBounds.y);
    _defs->addRequiredAttribute("width", shapeBounds.width);
    _defs->addRequiredAttribute("height", shapeBounds.height);
    _defs->closeElementStart();
    _defs->openElement("image");
    _defs->addAttribute("href", href);
    Rect placed = {};
    if (ComputePlacedImageRectSVG(pattern, imgW, imgH, shapeBounds, &placed)) {
      _defs->addAttributeIfNonZero("x", placed.x - shapeBounds.x);
      _defs->addAttributeIfNonZero("y", placed.y - shapeBounds.y);
      _defs->addRequiredAttribute("width", placed.width);
      _defs->addRequiredAttribute("height", placed.height);
      // The placed rect already factors in matrix scale and scaleMode fit, so
      // tell SVG to stretch the image bitmap exactly to that rect.
      _defs->addAttribute("preserveAspectRatio", "none");
    } else {
      // Rotation, skew, or negative scale: fall back to raw matrix-driven
      // positioning at natural pixel size (matches ScaleMode::None behaviour
      // without the scaleMode fit, since no axis-aligned rect can express it).
      _defs->addRequiredAttribute("width", static_cast<float>(imgW));
      _defs->addRequiredAttribute("height", static_cast<float>(imgH));
      if (!pattern->matrix.isIdentity()) {
        Matrix localMatrix = pattern->matrix;
        localMatrix.tx -= shapeBounds.x;
        localMatrix.ty -= shapeBounds.y;
        _defs->addAttribute("transform", MatrixToSVGTransform(localMatrix));
      }
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

  // Inner shadow carves away the offset/blurred alpha from inside the source:
  // result = k1*S*O + k2*S = S*(1 - O), where S = SourceAlpha, O = offsetResult.
  // This keeps only the portion of the source that is NOT covered by the offset
  // blur, producing the shadow along the inner edge opposite the offset direction.
  _defs->openElement("feComposite");
  _defs->addAttribute("in", "SourceAlpha");
  _defs->addAttribute("in2", offsetResult);
  _defs->addAttribute("operator", "arithmetic");
  _defs->addAttribute("k1", "-1");
  _defs->addAttribute("k2", "1");
  _defs->addAttribute("result", compositeResult);
  _defs->closeElementSelfClosing();

  writeShadowColorMatrix(color, compositeResult, shadowResult);
  return shadowResult;
}

void SVGWriter::writeColorMatrixFilter(const ColorMatrixFilter* cm, int& colorMatrixIndex,
                                       std::string& currentSource) {
  std::string idx = std::to_string(colorMatrixIndex++);
  std::string resultName = "colorMatrix" + idx;
  _defs->openElement("feColorMatrix");
  _defs->addAttribute("in", currentSource);
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
  _defs->addAttribute("result", resultName);
  _defs->closeElementSelfClosing();
  currentSource = resultName;
}

void SVGWriter::writeBlendFilter(const BlendFilter* blend, int& shadowIndex,
                                 std::string& currentSource) {
  std::string idx = std::to_string(shadowIndex++);
  std::string floodResult = "blendFlood" + idx;
  _defs->openElement("feFlood");
  _defs->addAttribute("flood-color", ColorToSVGString(blend->color));
  if (blend->color.alpha < 1.0f) {
    _defs->addAttribute("flood-opacity", FloatToString(blend->color.alpha));
  }
  _defs->addAttribute("result", floodResult);
  _defs->closeElementSelfClosing();

  // PAGX's BlendFilter treats the stored color as the source and the layer
  // content as the destination (backdrop) — see
  // tgfx::ColorFilter::Blend / ModeColorFilter::asFragmentProcessor. SVG's
  // feBlend expects `in` to be the top/source and `in2` to be the bottom/dst,
  // so the flood color is the `in` and the current chain result is the `in2`.
  std::string blendResult = "blendResult" + idx;
  _defs->openElement("feBlend");
  _defs->addAttribute("in", floodResult);
  _defs->addAttribute("in2", currentSource);
  // Use the feBlend-specific lookup — modes without an feBlend equivalent
  // (plus-lighter / plus-darker) intentionally fall back to the default mode.
  auto modeStr = BlendModeToFEBlendString(blend->blendMode);
  if (modeStr) {
    _defs->addAttribute("mode", modeStr);
  }
  _defs->addAttribute("result", blendResult);
  _defs->closeElementSelfClosing();

  // Clip the blend output to the current chain's alpha so the solid flood
  // rectangle is confined to the visible layer silhouette. Using
  // `currentSource` as the mask keeps the blend correctly chained after
  // upstream filters (e.g. Blur widens alpha → the blend follows).
  std::string blendOut = "blendOut" + idx;
  _defs->openElement("feComposite");
  _defs->addAttribute("in", blendResult);
  _defs->addAttribute("in2", currentSource);
  _defs->addAttribute("operator", "in");
  _defs->addAttribute("result", blendOut);
  _defs->closeElementSelfClosing();
  currentSource = blendOut;
}

std::string SVGWriter::writeNoiseTurbulence(const NoiseFilter* noise, const std::string& resultName,
                                            const Rect&) {
  auto freq = noise->size > 0.0f ? 1.0f / noise->size : 0.25f;
  _defs->openElement("feTurbulence");
  _defs->addAttribute("type", "fractalNoise");
  _defs->addAttribute("baseFrequency", FloatToString(freq));
  _defs->addAttribute("stitchTiles", "stitch");
  _defs->addAttribute("numOctaves", "3");
  _defs->addAttribute("seed", FloatToString(noise->seed));
  _defs->addAttribute("result", resultName);
  _defs->closeElementSelfClosing();
  return resultName;
}

std::string SVGWriter::writeNoiseBand(const NoiseFilter* noise, bool isDark,
                                      const std::string& label, const Rect& contentBounds) {
  auto turbResult = writeNoiseTurbulence(noise, "turb" + label, contentBounds);

  _defs->openElement("feColorMatrix");
  _defs->addAttribute("in", turbResult);
  _defs->addAttribute("type", "luminanceToAlpha");
  _defs->addAttribute("result", "luma" + label);
  _defs->closeElementSelfClosing();

  auto d = std::clamp(noise->density, 0.0f, 1.0f);
  int lower = 0;
  int upper = 0;
  if (isDark) {
    lower = std::clamp(static_cast<int>(std::lround(-25.0f * d + 25.0f)), 0, 99);
    upper = std::clamp(static_cast<int>(std::lround(24.0f * d + 25.0f)), 0, 99);
  } else {
    lower = std::clamp(static_cast<int>(std::lround(-24.0f * d + 74.0f)), 0, 99);
    upper = std::clamp(static_cast<int>(std::lround(25.0f * d + 74.0f)), 0, 99);
  }
  std::string table;
  table.reserve(300);
  for (int i = 0; i < 100; i++) {
    table += (i >= lower && i <= upper) ? "1 " : "0 ";
  }
  table.pop_back();

  _defs->openElement("feComponentTransfer");
  _defs->addAttribute("in", "luma" + label);
  _defs->addAttribute("result", "band" + label);
  _defs->closeElementStart();
  _defs->openElement("feFuncA");
  _defs->addAttribute("type", "discrete");
  _defs->addAttribute("tableValues", table);
  _defs->closeElementSelfClosing();
  _defs->closeElement();
  return "band" + label;
}

std::string SVGWriter::writeNoiseTurbulence(const NoiseStyle* noise, const std::string& resultName,
                                            const Rect&) {
  auto freq = noise->size > 0.0f ? 1.0f / noise->size : 0.25f;
  _defs->openElement("feTurbulence");
  _defs->addAttribute("type", "fractalNoise");
  _defs->addAttribute("baseFrequency", FloatToString(freq));
  _defs->addAttribute("stitchTiles", "stitch");
  _defs->addAttribute("numOctaves", "3");
  _defs->addAttribute("seed", FloatToString(noise->seed));
  _defs->addAttribute("result", resultName);
  _defs->closeElementSelfClosing();
  return resultName;
}

std::string SVGWriter::writeNoiseBand(const NoiseStyle* noise, bool isDark,
                                      const std::string& label, const Rect& contentBounds) {
  auto turbResult = writeNoiseTurbulence(noise, "turb" + label, contentBounds);

  _defs->openElement("feColorMatrix");
  _defs->addAttribute("in", turbResult);
  _defs->addAttribute("type", "luminanceToAlpha");
  _defs->addAttribute("result", "luma" + label);
  _defs->closeElementSelfClosing();

  auto d = std::clamp(noise->density, 0.0f, 1.0f);
  int lower = 0;
  int upper = 0;
  if (isDark) {
    lower = std::clamp(static_cast<int>(std::lround(-25.0f * d + 25.0f)), 0, 99);
    upper = std::clamp(static_cast<int>(std::lround(24.0f * d + 25.0f)), 0, 99);
  } else {
    lower = std::clamp(static_cast<int>(std::lround(-24.0f * d + 74.0f)), 0, 99);
    upper = std::clamp(static_cast<int>(std::lround(25.0f * d + 74.0f)), 0, 99);
  }
  std::string table;
  table.reserve(300);
  for (int i = 0; i < 100; i++) {
    table += (i >= lower && i <= upper) ? "1 " : "0 ";
  }
  table.pop_back();

  _defs->openElement("feComponentTransfer");
  _defs->addAttribute("in", "luma" + label);
  _defs->addAttribute("result", "band" + label);
  _defs->closeElementStart();
  _defs->openElement("feFuncA");
  _defs->addAttribute("type", "discrete");
  _defs->addAttribute("tableValues", table);
  _defs->closeElementSelfClosing();
  _defs->closeElement();
  return "band" + label;
}

std::string SVGWriter::writeNoiseFilter(const NoiseFilter* noise, int& noiseIndex,
                                        std::string& currentSource, const Rect& contentBounds) {
  std::string filterId = "noise" + std::to_string(noiseIndex++);

  if (noise->mode == NoiseMode::Mono) {
    auto band = writeNoiseBand(noise, true, "Dark" + filterId, contentBounds);
    _defs->openElement("feFlood");
    _defs->addAttribute("flood-color", ColorToSVGString(noise->color));
    if (noise->color.alpha < 1.0f) {
      _defs->addAttribute("flood-opacity", FloatToString(noise->color.alpha));
    }
    _defs->addAttribute("result", "flood" + filterId);
    _defs->closeElementSelfClosing();

    _defs->openElement("feComposite");
    _defs->addAttribute("in", "flood" + filterId);
    _defs->addAttribute("in2", band);
    _defs->addAttribute("operator", "in");
    _defs->addAttribute("result", "colored" + filterId);
    _defs->closeElementSelfClosing();

    _defs->openElement("feComposite");
    _defs->addAttribute("in", "colored" + filterId);
    _defs->addAttribute("in2", currentSource);
    _defs->addAttribute("operator", "in");
    _defs->addAttribute("result", "clipped" + filterId);
    _defs->closeElementSelfClosing();

    auto resultName = "noiseOut" + filterId;
    _defs->openElement("feBlend");
    _defs->addAttribute("in", "clipped" + filterId);
    _defs->addAttribute("in2", currentSource);
    auto modeStr = BlendModeToFEBlendString(noise->blendMode);
    if (modeStr) {
      _defs->addAttribute("mode", modeStr);
    }
    _defs->addAttribute("result", resultName);
    _defs->closeElementSelfClosing();
    currentSource = resultName;
    return resultName;
  }

  if (noise->mode == NoiseMode::Duo) {
    auto dark = writeNoiseBand(noise, true, "Dark" + filterId, contentBounds);
    auto bright = writeNoiseBand(noise, false, "Bright" + filterId, contentBounds);

    _defs->openElement("feComposite");
    _defs->addAttribute("in", dark);
    _defs->addAttribute("in2", bright);
    _defs->addAttribute("operator", "out");
    _defs->addAttribute("result", "duoDark" + filterId);
    _defs->closeElementSelfClosing();

    _defs->openElement("feFlood");
    _defs->addAttribute("flood-color", ColorToSVGString(noise->firstColor));
    if (noise->firstColor.alpha < 1.0f) {
      _defs->addAttribute("flood-opacity", FloatToString(noise->firstColor.alpha));
    }
    _defs->addAttribute("result", "floodDark" + filterId);
    _defs->closeElementSelfClosing();

    _defs->openElement("feComposite");
    _defs->addAttribute("in", "floodDark" + filterId);
    _defs->addAttribute("in2", "duoDark" + filterId);
    _defs->addAttribute("operator", "in");
    _defs->addAttribute("result", "darkColored" + filterId);
    _defs->closeElementSelfClosing();

    _defs->openElement("feFlood");
    _defs->addAttribute("flood-color", ColorToSVGString(noise->secondColor));
    if (noise->secondColor.alpha < 1.0f) {
      _defs->addAttribute("flood-opacity", FloatToString(noise->secondColor.alpha));
    }
    _defs->addAttribute("result", "floodBright" + filterId);
    _defs->closeElementSelfClosing();

    _defs->openElement("feComposite");
    _defs->addAttribute("in", "floodBright" + filterId);
    _defs->addAttribute("in2", bright);
    _defs->addAttribute("operator", "in");
    _defs->addAttribute("result", "brightColored" + filterId);
    _defs->closeElementSelfClosing();

    _defs->openElement("feBlend");
    _defs->addAttribute("in", "darkColored" + filterId);
    _defs->addAttribute("in2", "brightColored" + filterId);
    _defs->addAttribute("mode", "screen");
    _defs->addAttribute("result", "duo" + filterId);
    _defs->closeElementSelfClosing();

    _defs->openElement("feComposite");
    _defs->addAttribute("in", "duo" + filterId);
    _defs->addAttribute("in2", currentSource);
    _defs->addAttribute("operator", "in");
    _defs->addAttribute("result", "clipped" + filterId);
    _defs->closeElementSelfClosing();

    auto resultName = "noiseOut" + filterId;
    _defs->openElement("feBlend");
    _defs->addAttribute("in", "clipped" + filterId);
    _defs->addAttribute("in2", currentSource);
    auto modeStr = BlendModeToFEBlendString(noise->blendMode);
    if (modeStr) {
      _defs->addAttribute("mode", modeStr);
    }
    _defs->addAttribute("result", resultName);
    _defs->closeElementSelfClosing();
    currentSource = resultName;
    return resultName;
  }

  auto nCtResult = writeNoiseTurbulence(noise, "nCt" + filterId, contentBounds);

  _defs->openElement("feColorMatrix");
  _defs->addAttribute("in", nCtResult);
  _defs->addAttribute("type", "matrix");
  _defs->addAttribute("values",
                      "2 0 0 0 -0.5 "
                      "0 2 0 0 -0.5 "
                      "0 0 2 0 -0.5 "
                      "0 0 0 1 0");
  _defs->addAttribute("result", "nCon" + filterId);
  _defs->closeElementSelfClosing();

  auto darkBand = writeNoiseBand(noise, true, "MultiDark" + filterId, contentBounds);
  auto brightBand = writeNoiseBand(noise, false, "MultiBright" + filterId, contentBounds);

  _defs->openElement("feComposite");
  _defs->addAttribute("in", darkBand);
  _defs->addAttribute("in2", brightBand);
  _defs->addAttribute("operator", "out");
  _defs->addAttribute("result", "mB" + filterId);
  _defs->closeElementSelfClosing();

  _defs->openElement("feComposite");
  _defs->addAttribute("in", "nCon" + filterId);
  _defs->addAttribute("in2", "mB" + filterId);
  _defs->addAttribute("operator", "in");
  _defs->addAttribute("result", "mN" + filterId);
  _defs->closeElementSelfClosing();

  _defs->openElement("feColorMatrix");
  _defs->addAttribute("in", "mN" + filterId);
  _defs->addAttribute("type", "matrix");
  std::string opacityValues = "1 0 0 0 0 0 1 0 0 0 0 0 1 0 0 0 0 0 ";
  opacityValues += FloatToString(noise->opacity);
  opacityValues += " 0";
  _defs->addAttribute("values", opacityValues);
  _defs->addAttribute("result", "mO" + filterId);
  _defs->closeElementSelfClosing();

  _defs->openElement("feComposite");
  _defs->addAttribute("in", "mO" + filterId);
  _defs->addAttribute("in2", currentSource);
  _defs->addAttribute("operator", "in");
  _defs->addAttribute("result", "mC" + filterId);
  _defs->closeElementSelfClosing();

  auto resultName = "noiseOut" + filterId;
  _defs->openElement("feBlend");
  _defs->addAttribute("in", "mC" + filterId);
  _defs->addAttribute("in2", currentSource);
  auto modeStr = BlendModeToFEBlendString(noise->blendMode);
  if (modeStr) {
    _defs->addAttribute("mode", modeStr);
  }
  _defs->addAttribute("result", resultName);
  _defs->closeElementSelfClosing();
  currentSource = resultName;
  return resultName;
}

std::string SVGWriter::writeNoiseStyle(const NoiseStyle* noise, int& noiseStyleIndex,
                                       const Rect& contentBounds) {
  std::string styleId = "noiseStyle" + std::to_string(noiseStyleIndex++);

  if (noise->mode == NoiseMode::Mono) {
    auto band = writeNoiseBand(noise, true, "Dark" + styleId, contentBounds);
    _defs->openElement("feFlood");
    _defs->addAttribute("flood-color", ColorToSVGString(noise->color));
    if (noise->color.alpha < 1.0f) {
      _defs->addAttribute("flood-opacity", FloatToString(noise->color.alpha));
    }
    _defs->addAttribute("result", "flood" + styleId);
    _defs->closeElementSelfClosing();

    _defs->openElement("feComposite");
    _defs->addAttribute("in", "flood" + styleId);
    _defs->addAttribute("in2", band);
    _defs->addAttribute("operator", "in");
    _defs->addAttribute("result", "colored" + styleId);
    _defs->closeElementSelfClosing();

    auto resultName = "noiseStyleOut" + styleId;
    _defs->openElement("feComposite");
    _defs->addAttribute("in", "colored" + styleId);
    _defs->addAttribute("in2", "SourceGraphic");
    _defs->addAttribute("operator", "in");
    _defs->addAttribute("result", resultName);
    _defs->closeElementSelfClosing();
    return resultName;
  }

  if (noise->mode == NoiseMode::Duo) {
    auto dark = writeNoiseBand(noise, true, "Dark" + styleId, contentBounds);
    auto bright = writeNoiseBand(noise, false, "Bright" + styleId, contentBounds);

    _defs->openElement("feComposite");
    _defs->addAttribute("in", dark);
    _defs->addAttribute("in2", bright);
    _defs->addAttribute("operator", "out");
    _defs->addAttribute("result", "duoDark" + styleId);
    _defs->closeElementSelfClosing();

    _defs->openElement("feFlood");
    _defs->addAttribute("flood-color", ColorToSVGString(noise->firstColor));
    if (noise->firstColor.alpha < 1.0f) {
      _defs->addAttribute("flood-opacity", FloatToString(noise->firstColor.alpha));
    }
    _defs->addAttribute("result", "floodDark" + styleId);
    _defs->closeElementSelfClosing();

    _defs->openElement("feComposite");
    _defs->addAttribute("in", "floodDark" + styleId);
    _defs->addAttribute("in2", "duoDark" + styleId);
    _defs->addAttribute("operator", "in");
    _defs->addAttribute("result", "darkColored" + styleId);
    _defs->closeElementSelfClosing();

    _defs->openElement("feFlood");
    _defs->addAttribute("flood-color", ColorToSVGString(noise->secondColor));
    if (noise->secondColor.alpha < 1.0f) {
      _defs->addAttribute("flood-opacity", FloatToString(noise->secondColor.alpha));
    }
    _defs->addAttribute("result", "floodBright" + styleId);
    _defs->closeElementSelfClosing();

    _defs->openElement("feComposite");
    _defs->addAttribute("in", "floodBright" + styleId);
    _defs->addAttribute("in2", bright);
    _defs->addAttribute("operator", "in");
    _defs->addAttribute("result", "brightColored" + styleId);
    _defs->closeElementSelfClosing();

    _defs->openElement("feBlend");
    _defs->addAttribute("in", "darkColored" + styleId);
    _defs->addAttribute("in2", "brightColored" + styleId);
    _defs->addAttribute("mode", "screen");
    _defs->addAttribute("result", "duo" + styleId);
    _defs->closeElementSelfClosing();

    auto resultName = "noiseStyleOut" + styleId;
    _defs->openElement("feComposite");
    _defs->addAttribute("in", "duo" + styleId);
    _defs->addAttribute("in2", "SourceGraphic");
    _defs->addAttribute("operator", "in");
    _defs->addAttribute("result", resultName);
    _defs->closeElementSelfClosing();
    return resultName;
  }

  // Multi mode
  auto nCtResult = writeNoiseTurbulence(noise, "nCt" + styleId, contentBounds);

  _defs->openElement("feColorMatrix");
  _defs->addAttribute("in", nCtResult);
  _defs->addAttribute("type", "matrix");
  _defs->addAttribute("values",
                      "2 0 0 0 -0.5 "
                      "0 2 0 0 -0.5 "
                      "0 0 2 0 -0.5 "
                      "0 0 0 1 0");
  _defs->addAttribute("result", "nCon" + styleId);
  _defs->closeElementSelfClosing();

  auto darkBand = writeNoiseBand(noise, true, "MultiDark" + styleId, contentBounds);
  auto brightBand = writeNoiseBand(noise, false, "MultiBright" + styleId, contentBounds);

  _defs->openElement("feComposite");
  _defs->addAttribute("in", darkBand);
  _defs->addAttribute("in2", brightBand);
  _defs->addAttribute("operator", "out");
  _defs->addAttribute("result", "mB" + styleId);
  _defs->closeElementSelfClosing();

  _defs->openElement("feComposite");
  _defs->addAttribute("in", "nCon" + styleId);
  _defs->addAttribute("in2", "mB" + styleId);
  _defs->addAttribute("operator", "in");
  _defs->addAttribute("result", "mN" + styleId);
  _defs->closeElementSelfClosing();

  _defs->openElement("feColorMatrix");
  _defs->addAttribute("in", "mN" + styleId);
  _defs->addAttribute("type", "matrix");
  std::string opacityValues = "1 0 0 0 0 0 1 0 0 0 0 0 1 0 0 0 0 0 ";
  opacityValues += FloatToString(noise->opacity);
  opacityValues += " 0";
  _defs->addAttribute("values", opacityValues);
  _defs->addAttribute("result", "mO" + styleId);
  _defs->closeElementSelfClosing();

  auto resultName = "noiseStyleOut" + styleId;
  _defs->openElement("feComposite");
  _defs->addAttribute("in", "mO" + styleId);
  _defs->addAttribute("in2", "SourceGraphic");
  _defs->addAttribute("operator", "in");
  _defs->addAttribute("result", resultName);
  _defs->closeElementSelfClosing();
  return resultName;
}

void SVGWriter::writeFilterList(const std::vector<LayerFilter*>& filters, int& shadowIndex,
                                ShadowAggregate& agg, std::string& currentSource,
                                const Rect& contentBounds) {
  int colorMatrixIndex = 0;
  int blurIndex = 0;
  int noiseIndex = 0;
  for (const auto* filter : filters) {
    switch (filter->nodeType()) {
      case NodeType::BlurFilter: {
        auto blur = static_cast<const BlurFilter*>(filter);
        std::string resultName = "blurred" + std::to_string(blurIndex++);
        _defs->openElement("feGaussianBlur");
        _defs->addAttribute("in", currentSource);
        _defs->addAttribute("stdDeviation", FormatBlurStdDev(blur->blurX, blur->blurY));
        _defs->addAttribute("result", resultName);
        _defs->closeElementSelfClosing();
        currentSource = resultName;
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
        writeColorMatrixFilter(static_cast<const ColorMatrixFilter*>(filter), colorMatrixIndex,
                               currentSource);
        break;
      case NodeType::BlendFilter:
        writeBlendFilter(static_cast<const BlendFilter*>(filter), shadowIndex, currentSource);
        break;
      case NodeType::NoiseFilter:
        writeNoiseFilter(static_cast<const NoiseFilter*>(filter), noiseIndex, currentSource,
                         contentBounds);
        break;
      default:
        break;
    }
  }
}

// LayerStyle emission mirrors the Filter pass so DropShadowStyle / InnerShadowStyle
// share the feMerge aggregation logic. BackgroundBlurStyle is silently skipped because SVG has
// no portable backdrop-blur primitive (the deprecated enable-background is not supported by
// modern renderers). NoiseStyle emits its SVG primitives and adds the result name to
// agg.aboveResults so it composites above the source in the final feMerge.
void SVGWriter::writeStyleList(const std::vector<LayerStyle*>& styles, int& shadowIndex,
                               ShadowAggregate& agg, const Rect& contentBounds) {
  int noiseStyleIndex = 0;
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
      case NodeType::NoiseStyle: {
        auto noise = static_cast<const NoiseStyle*>(style);
        auto result = writeNoiseStyle(noise, noiseStyleIndex, contentBounds);
        agg.aboveResults.push_back(result);
        agg.needSourceGraphic = true;
        break;
      }
      default:
        break;
    }
  }
}

void SVGWriter::writeShadowMerge(const ShadowAggregate& agg, const std::string& currentSource) {
  bool hasShadows = !agg.dropShadowResults.empty() || !agg.innerShadowResults.empty();
  if (!hasShadows) {
    // No shadows but upstream filters may have reshaped the source (Blur,
    // ColorMatrix, Blend). In that case the chain already terminates in
    // `currentSource`, which is the default implicit output — nothing to
    // merge. Only when no image-transforming filter ran does currentSource
    // remain "SourceGraphic", and the <filter> would have been empty anyway.
    return;
  }
  bool multipleShadows = (agg.dropShadowResults.size() + agg.innerShadowResults.size()) > 1;
  if (!agg.needSourceGraphic && !multipleShadows) {
    // Single shadow with shadowOnly / no source overlay: the shadow primitive
    // is the filter's implicit output. When upstream filters mutated the
    // source (e.g. Blur before a shadowOnly InnerShadow) the mutated source
    // is intentionally hidden — the authored shadowOnly semantics dominate.
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
    // Use the chain's current source so upstream Blur / ColorMatrix / Blend
    // appear on top of the drop shadows rather than the original untouched
    // graphic.
    _defs->addAttribute("in", currentSource);
    _defs->closeElementSelfClosing();
  }
  for (const auto& result : agg.aboveResults) {
    _defs->openElement("feMergeNode");
    _defs->addAttribute("in", result);
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
                                               const std::vector<LayerStyle*>& styles,
                                               const Rect& contentBounds) {
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
  if (!contentBounds.isEmpty()) {
    // Use filterUnits="userSpaceOnUse" with absolute pixel coordinates for filters
    // with known content bounds. This decouples the filter region from the element's
    // bounding box so effects like feTurbulence sample at absolute coordinates,
    // producing position-dependent noise phase that matches tgfx behavior.
    float filterX = contentBounds.x - marginLeft;
    float filterY = contentBounds.y - marginTop;
    float filterW = contentBounds.width + marginLeft + marginRight;
    float filterH = contentBounds.height + marginTop + marginBottom;
    _defs->addAttribute("filterUnits", "userSpaceOnUse");
    _defs->addAttribute("x", FloatToString(filterX));
    _defs->addAttribute("y", FloatToString(filterY));
    _defs->addAttribute("width", FloatToString(filterW));
    _defs->addAttribute("height", FloatToString(filterH));
  } else {
    _defs->addAttribute("x", "-" + FloatToString(pctLeft) + "%");
    _defs->addAttribute("y", "-" + FloatToString(pctTop) + "%");
    _defs->addAttribute("width", FloatToString(100.0f + pctLeft + pctRight) + "%");
    _defs->addAttribute("height", FloatToString(100.0f + pctTop + pctBottom) + "%");
  }
  _defs->addAttribute("color-interpolation-filters", "sRGB");
  _defs->closeElementStart();

  int shadowIndex = 0;
  ShadowAggregate agg;
  std::string currentSource = "SourceGraphic";
  writeFilterList(filters, shadowIndex, agg, currentSource, contentBounds);
  writeStyleList(styles, shadowIndex, agg, contentBounds);
  writeShadowMerge(agg, currentSource);

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
  // Apply layout-resolved placement (matches LayerBuilder::convertPath):
  // shapePath->setPosition(renderPosition()) + path data is scaled by renderScale().
  // Without applying both, raw path data emits at its authored origin and authored
  // size, losing any flex / centerX / layoutBounds-driven centring and sizing.
  auto renderPos = path->renderPosition();
  float scale = path->renderScale();
  out.openElement("path");
  std::string transform = MatrixToSVGTransform(m);
  std::string localTransform;
  if (renderPos.x != 0.0f || renderPos.y != 0.0f) {
    localTransform =
        "translate(" + FloatToString(renderPos.x) + "," + FloatToString(renderPos.y) + ")";
  }
  if (scale != 1.0f) {
    if (!localTransform.empty()) {
      localTransform += " ";
    }
    localTransform += "scale(" + FloatToString(scale) + ")";
  }
  if (!transform.empty() && !localTransform.empty()) {
    out.addAttribute("transform", transform + " " + localTransform);
  } else if (!transform.empty()) {
    out.addAttribute("transform", transform);
  } else if (!localTransform.empty()) {
    out.addAttribute("transform", localTransform);
  }
  out.addAttribute("d", PathDataToSVGString(*path->data));
  // shapeBounds must reflect the painted region in the parent coordinate space so
  // gradient `fitsToGeometry` and image patterns sample the right rectangle. The path
  // data sits in pre-scale space, so multiply the raw bounds by the scale factor and
  // translate them by renderPosition to match the on-canvas extent.
  Rect dataBounds = path->data->getBounds();
  Rect bounds =
      Rect::MakeXYWH(renderPos.x + dataBounds.x * scale, renderPos.y + dataBounds.y * scale,
                     dataBounds.width * scale, dataBounds.height * scale);
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

  // A gradient must span the whole glyph run, but the fill/stroke here is
  // inherited by every per-glyph <path>, and SVG re-fits both objectBoundingBox
  // and userSpaceOnUse gradients to each <path>'s own coordinate space (each
  // glyph carries its own transform). Bake the glyph transforms into the path
  // data so all glyphs share the group's coordinate space. For fitsToGeometry
  // gradients the run's combined bounds are then baked into gradientTransform
  // (see finishGradientDef); for pixel-space (fitsToGeometry=false) gradients
  // the shared coordinate space alone is enough.
  bool fitGradientToRun = IsGradientSource(fs.fill ? fs.fill->color : nullptr) ||
                          IsGradientSource(fs.stroke ? fs.stroke->color : nullptr);

  std::vector<std::string> bakedPaths;
  if (fitGradientToRun) {
    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();
    bakedPaths.reserve(glyphPaths.size());
    for (const auto& gp : glyphPaths) {
      if (!gp.pathData) {
        continue;
      }
      PathData baked = PathDataFromSVGString("");
      baked = *gp.pathData;
      baked.transform(gp.transform);
      Rect b = baked.getBounds();
      minX = std::min(minX, b.x);
      minY = std::min(minY, b.y);
      maxX = std::max(maxX, b.x + b.width);
      maxY = std::max(maxY, b.y + b.height);
      bakedPaths.push_back(PathDataToSVGString(baked));
    }
    if (maxX > minX && maxY > minY) {
      _gradientUserSpaceBounds = Rect::MakeLTRB(minX, minY, maxX, maxY);
    }
  }

  // Fill/stroke here only apply to the vector glyph <path> children; <image>
  // children have an intrinsic bitmap and ignore fill. We still emit the group
  // attributes so SVG renderers inherit them onto the glyph paths.
  applyPainters(out, fs, {}, alpha);
  // The gradient def was written by applyPainters; clear the run bounds so the
  // bitmap glyph branch and any nested writers fall back to the default mode.
  _gradientUserSpaceBounds = {};
  out.closeElementStart();

  if (fitGradientToRun) {
    for (const auto& d : bakedPaths) {
      out.openElement("path");
      out.addAttribute("d", d);
      out.closeElementSelfClosing();
    }
  } else {
    for (const auto& gp : glyphPaths) {
      out.openElement("path");
      out.addAttribute("transform", MatrixToSVGTransform(gp.transform));
      out.addAttribute("d", PathDataToSVGString(*gp.pathData));
      out.closeElementSelfClosing();
    }
  }

  // Bitmap glyphs (emoji / colour fonts). Each image is emitted as an <image>
  // element transformed into its per-glyph pixel-space origin. Mirror PPT's
  // writeTextAsPath bitmap loop (PPTExporter.cpp:1566).
  for (const auto& gi : glyphImages) {
    if (!gi.image) {
      continue;
    }
    bool mimeRecognized = true;
    std::string href = GetImageHref(gi.image, &mimeRecognized);
    if (href.empty()) {
      continue;
    }
    if (!mimeRecognized) {
      addWarning(
          "Glyph bitmap: encoded bytes do not match PNG/JPEG/WebP signature; declared as image/png "
          "fallback — viewers may fail to decode.");
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
  // Browser-first bold/italic mapping. A real Bold / Italic face (the PAGX
  // Text carries fontStyle="Bold" / "Italic" / "Bold Italic") is encoded by
  // appending the style to the family name (e.g. "Arial Bold"), and the
  // font-weight / font-style attributes are suppressed. Browsers and CoreText
  // (Safari, Chromium, macOS Preview / QuickLook) resolve the styled family
  // name directly to the real bold/italic glyph face — emitting just the
  // bare family name plus font-weight="bold" instead made them apply
  // faux-bold synthesis on top of the regular face, producing a visibly
  // thicker stroke than the PAGX renderer (which loads the real Arial Bold
  // face via MakeFromName(fontFamily, fontStyle)).
  //
  // fauxBold / fauxItalic still surface as font-weight / font-style — those
  // flags exist precisely so the renderer synthesizes the style instead of
  // locking onto a particular face. When both apply at once (a real Bold face
  // plus an additional fauxBold), the styled family picks the real Bold face
  // and font-weight="bold" layers the extra synthesis on top, mirroring
  // tgfx's setFauxBold-on-top-of-Bold-primary behaviour in PAGX's own
  // renderer.
  //
  // Trade-off: fontconfig-based viewers (librsvg, Inkscape on Linux, some
  // online viewers) match "Arial Bold" against the *family* "Arial Bold"
  // rather than family "Arial" + Bold face. No such family exists, so they
  // fall back to a substitute font and lose the bold weight. The call here
  // is that native browser / Preview fidelity matters more than fontconfig
  // fallback behaviour. Mirrors BuildRunStyle in src/pagx/ppt/PPTWriter.h.
  std::string typeface = BuildStyledTypeface(text->fontFamily, text->fontStyle);
  if (!typeface.empty()) {
    out.addAttribute("font-family", typeface);
  }
  // Use the layout-resolved font size. PAGX layout may shrink a Text internally via
  // a textScale factor to fit dual-axis constraints (e.g. `left`+`right`,
  // `width="100%"`); renderFontSize() carries that factor while fontSize still holds
  // the authored pre-scale value. Emitting the raw size here causes constrained text
  // to overflow its container in viewers. Recover the same scale to apply it to
  // letterSpacing, which the renderer also multiplies by textScale during shaping.
  float effectiveFontSize = text->renderFontSize();
  float textScale = (text->fontSize > 0.0f) ? effectiveFontSize / text->fontSize : 1.0f;
  out.addAttribute("font-size", FloatToString(effectiveFontSize));
  if (text->letterSpacing != 0.0f) {
    out.addAttribute("letter-spacing", FloatToString(text->letterSpacing * textScale));
  }
  if (anchor == TextAnchor::Center) {
    out.addAttribute("text-anchor", "middle");
  } else if (anchor == TextAnchor::End) {
    out.addAttribute("text-anchor", "end");
  }
  if (text->fauxBold) {
    out.addAttribute("font-weight", "bold");
  }
  if (text->fauxItalic) {
    out.addAttribute("font-style", "italic");
  }
}

// Emits writing-mode and text-align-last degradation notes on a <text> element.
// Called inline inside every <text> in writeTextWithLayout / writeText fallback.
static void ApplyTextBoxBodyAttrs(SVGBuilder& out, const TextBox* box) {
  if (box == nullptr) {
    return;
  }
  // CSS writing-mode "vertical-rl" matches PAGX's vertical CJK layout:
  // characters flow top-to-bottom within a column and columns advance
  // right-to-left. The legacy SVG 1.1 keyword "tb" is treated as an alias
  // by modern renderers, but "vertical-rl" is the value defined by CSS
  // Writing Modes Level 3 — browsers (including the WebKit/Blink based
  // renderers used by macOS QuickLook / Preview) follow the modern spec for
  // text-anchor interpretation in inline-axis (vertical) alignment, where
  // "tb" sometimes hits a legacy code path that ignores text-anchor.
  if (box->writingMode == WritingMode::Vertical) {
    out.addAttribute("writing-mode", "vertical-rl");
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

// Logical-to-physical anchor / align resolution lives in ExporterUtils.h
// (ResolveLogicalAnchor / ResolveLogicalAlign), shared with the PPT exporter.

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
  float effectiveFontSize = text->renderFontSize();
  float contentWidth = textBounds.isEmpty() ? effectiveFontSize : textBounds.width;
  float innerWidth = effectiveWidth - paddingLeft - paddingRight;
  float innerHeight =
      (!std::isnan(effectiveHeight)) ? effectiveHeight - paddingTop - paddingBottom : 0;
  // Block axis (horizontal): paragraphAlign positions columns right-to-left.
  // Near = right edge, Far = left edge.
  switch (box->paragraphAlign) {
    case ParagraphAlign::Middle:
      out->anchorX = paddingLeft + (innerWidth + contentWidth) / 2 - effectiveFontSize / 2;
      break;
    case ParagraphAlign::Far:
      out->anchorX = paddingLeft + contentWidth - effectiveFontSize / 2;
      break;
    default:
      out->anchorX = effectiveWidth - paddingRight - effectiveFontSize / 2;
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

static void ResolveHorizontalAnchoring(const TextBox* box, bool rtl, TextAnchoring* out) {
  float paddingLeft = box->padding.left;
  float paddingTop = box->padding.top;
  float paddingRight = box->padding.right;
  float effectiveWidth = EffectiveTextBoxWidth(box);
  // Resolve logical Start/End to a physical edge based on paragraph base
  // direction so an RTL paragraph aligns to the right edge for Start (the
  // default) and to the left for End.
  switch (ResolveLogicalAlign(box->textAlign, rtl)) {
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
// element's own anchor / renderPosition directly. `rtl` reflects the
// paragraph base direction (UBA P2/P3) and is only meaningful for horizontal
// writing mode — vertical writing flows top-to-bottom and ignores bidi.
static TextAnchoring ResolveTextAnchorAndOffset(const Text* text, const TextBox* box,
                                                const TextLayoutResult& layoutResult, bool rtl) {
  TextAnchoring out;
  if (box == nullptr) {
    out.anchor = ResolveLogicalAnchor(text->textAnchor, rtl);
    auto renderPos = text->renderPosition();
    out.anchorX = renderPos.x;
    out.offsetY = renderPos.y;
    return out;
  }
  if (box->writingMode == WritingMode::Vertical) {
    ResolveVerticalAnchoring(text, box, layoutResult, &out);
  } else {
    ResolveHorizontalAnchoring(box, rtl, &out);
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
      lastLineIndex = li;
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
  // Paragraph base direction (UBA P2/P3). Vertical writing flows top-to-bottom
  // and ignores BiDi, so suppress the rtl axis in that case to avoid swapping
  // the column anchor for Hebrew/Arabic vertical layouts.
  bool rtl = !isVertical && HasRTLParagraphBase(text->text);
  TextAnchoring anchoring = ResolveTextAnchorAndOffset(text, fs.textBox, layoutResult, rtl);

  std::string transform = MatrixToSVGTransform(m);

  // Vertical writing mode: SVG <text> does not auto-wrap into columns —
  // writing-mode only orients glyphs along the block axis, it never breaks
  // a long string into multiple columns. The layout already computed
  // per-column geometry — one TextLayoutLineInfo per visible column, with
  // baselineY = -columnX (column left-edge X in content coordinates),
  // lineWidth = maxColumnWidth, and startX = column.height. Columns dropped
  // by overflow="hidden" are absent from the layout result and therefore
  // omitted here automatically. For each column we:
  //   * recover columnX and shift by paddingLeft + columnWidth/2 to land on
  //     the column's true horizontal centre (CSS vertical writing-mode
  //     treats this midpoint as the inline baseline);
  //   * compute the inline-axis (vertical) start y from box->textAlign and
  //     column.height instead of relying on SVG's text-anchor, because
  //     several renderers (incl. Chrome / WebKit) ignore text-anchor in
  //     vertical writing-mode and always treat the position as the inline
  //     start, leaving Center / End-aligned columns overflowing past the
  //     box's bottom edge.
  if (isVertical && lines && !lines->empty()) {
    float effectiveFontSize = text->renderFontSize();
    float paddingLeft = fs.textBox ? fs.textBox->padding.left : 0.0f;
    float paddingTop = fs.textBox ? fs.textBox->padding.top : 0.0f;
    float effectiveHeight =
        fs.textBox ? EffectiveTextBoxHeight(fs.textBox) : std::numeric_limits<float>::quiet_NaN();
    float innerHeight = (!std::isnan(effectiveHeight))
                            ? std::max(0.0f, effectiveHeight - paddingTop -
                                                 (fs.textBox ? fs.textBox->padding.bottom : 0.0f))
                            : 0.0f;
    TextAlign textAlign = fs.textBox ? fs.textBox->textAlign : TextAlign::Start;
    // For Justify, mirror the renderer's per-column behaviour: every column
    // except the last visible (non-empty) one is stretched to fill the box
    // along the inline axis; the last column degrades to Start. Pre-compute
    // that index so the per-column emission can branch on it without an extra
    // pass.
    size_t lastJustifyColumnIdx =
        (textAlign == TextAlign::Justify) ? FindLastNonEmptyLineIndex(text->text, *lines) : 0;
    for (size_t lineIdx = 0; lineIdx < lines->size(); ++lineIdx) {
      const auto& info = (*lines)[lineIdx];
      if (info.byteStart >= info.byteEnd ||
          info.byteEnd - info.byteStart > text->text.size() - info.byteStart) {
        continue;
      }
      std::string columnText = text->text.substr(info.byteStart, info.byteEnd - info.byteStart);
      if (columnText.empty()) {
        continue;
      }
      float columnX = -info.baselineY;
      // Layout fills lineWidth/startX with column geometry; fall back to
      // sensible defaults if either is missing (e.g. degenerate font metric).
      float columnWidth = info.lineWidth > 0 ? info.lineWidth : effectiveFontSize;
      float columnHeight = info.startX > 0 ? info.startX : 0.0f;
      float columnCenterX = paddingLeft + columnX + columnWidth / 2.0f;
      float columnY = paddingTop;
      bool justifyStretch = false;
      if (innerHeight > 0 && columnHeight > 0) {
        switch (textAlign) {
          case TextAlign::Center:
            columnY = paddingTop + (innerHeight - columnHeight) / 2.0f;
            break;
          case TextAlign::End:
            columnY = paddingTop + innerHeight - columnHeight;
            break;
          case TextAlign::Justify:
            // Stretch all non-last columns to fill innerHeight by widening
            // inter-glyph spacing (see textLength emission below). The last
            // visible column degrades to Start, matching the renderer.
            // SVG's lengthAdjust="spacing" distributes the surplus uniformly
            // across every glyph pair, while the renderer concentrates it at
            // UAX#14 break opportunities — visually near-identical for the
            // CJK + Latin mixes vertical justify is used for, and the only
            // portable approximation available without per-glyph <tspan>s.
            columnY = paddingTop;
            if (lineIdx != lastJustifyColumnIdx && innerHeight > columnHeight) {
              justifyStretch = true;
            }
            break;
          case TextAlign::Start:
          default:
            columnY = paddingTop;
            break;
        }
      }
      out.openElement("text");
      if (!transform.empty()) {
        out.addAttribute("transform", transform);
      }
      // Suppress the resolved text-anchor here: inline alignment is baked
      // into columnY above, and emitting text-anchor on a vertical <text>
      // either has no effect (Chrome) or shifts glyphs the wrong way
      // (older WebKit), depending on the renderer.
      WriteSharedTextAttrs(out, text, TextAnchor::Start);
      ApplyTextBoxBodyAttrs(out, fs.textBox);
      applyPainters(out, fs, {}, alpha);
      out.addRequiredAttribute("x", columnCenterX);
      out.addRequiredAttribute("y", columnY);
      if (justifyStretch) {
        out.addAttribute("textLength", FloatToString(innerHeight));
        out.addAttribute("lengthAdjust", "spacing");
      }
      out.closeElementWithText(columnText);
    }
    return;
  }

  if (isVertical || !lines || lines->empty()) {
    // Fallback for: vertical mode with no usable column info (e.g. layout
    // produced nothing), or horizontal mode with no line info. Emit the whole
    // string as one <text>; horizontal mode needs a baseline shift by font
    // size since (x, y) in SVG marks the baseline of the first glyph, not the
    // top of the line box. Vertical fallback keeps offsetY as-is because
    // writing-mode handles the block-flow baseline implicitly.
    out.openElement("text");
    if (!transform.empty()) {
      out.addAttribute("transform", transform);
    }
    WriteSharedTextAttrs(out, text, anchoring.anchor);
    ApplyTextBoxBodyAttrs(out, fs.textBox);
    applyPainters(out, fs, {}, alpha);
    out.addRequiredAttribute("x", anchoring.anchorX);
    out.addRequiredAttribute(
        "y", isVertical ? anchoring.offsetY : anchoring.offsetY + text->renderFontSize());
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
// TextBox reproduces per-run fill/stroke pairing. Walked by the shared
// CollectRichTextRuns template in ExporterUtils.h.
namespace {

struct SVGRichTextRun {
  const Text* text = nullptr;
  const Fill* fill = nullptr;
  const Stroke* stroke = nullptr;
};

struct SVGLineEntry {
  size_t runIndex = 0;
  float baselineY = 0;
  float lineWidth = 0;
  uint32_t byteStart = 0;
  uint32_t byteEnd = 0;
};

}  // namespace

void SVGWriter::writeTextBoxGroup(SVGBuilder& out, const TextBox* textBox,
                                  const std::vector<Element*>& elements, const Matrix& transform,
                                  float alpha) {
  auto topLevelFs = CollectFillStroke(elements);
  std::vector<SVGRichTextRun> runs;
  CollectRichTextRuns(elements, topLevelFs.fill, topLevelFs.stroke, runs);
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
  // gives us anchor/anchorX/offsetY/justifyWidth directly. The TextBox carries a
  // single paragraph, so concatenating run text in source order matches the
  // paragraph-level base direction rule that PAGX's ICU BiDi uses; the helper
  // walks until it hits the first strong directional character.
  std::string combinedText;
  for (const auto& run : runs) {
    combinedText.append(run.text->text);
  }
  bool rtl = HasRTLParagraphBase(combinedText);
  TextAnchoring anchoring;
  ResolveHorizontalAnchoring(textBox, rtl, &anchoring);
  std::string svgTransform = MatrixToSVGTransform(transform);

  // Stable-sort entries by baselineY so that runs from different Text
  // elements sharing the same visual line stay in source order.
  std::stable_sort(lineEntries.begin(), lineEntries.end(), LineEntryBaselineYLess<SVGLineEntry>);
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
  bool isVertical = fs.textBox && fs.textBox->writingMode == WritingMode::Vertical;
  bool rtl = !isVertical && HasRTLParagraphBase(text->text);
  std::string transform = MatrixToSVGTransform(m);
  out.openElement("text");
  if (!transform.empty()) {
    out.addAttribute("transform", transform);
  }
  WriteSharedTextAttrs(out, text, ResolveLogicalAnchor(text->textAnchor, rtl));
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
  auto tgfxLayer = buildResult.getLayer(layer);
  if (!tgfxLayer) {
    addWarning("rasterize skipped: layer '" +
               (layer->id.empty() ? std::string("(unnamed)") : layer->id) +
               "' has no entry in the LayerBuilder map; falling back to vector emission.");
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
  // Compute the on-canvas placement bounds in the same coordinate space and with the same
  // scrollRect intersection that RenderMaskedLayer applies internally, so the <image>'s x/y/w/h
  // exactly cover the rasterized PNG's pixel region. Without intersecting scrollRect here the
  // <image> would be placed at the unclipped layer bounds while the PNG carries scrollRect-clipped
  // pixels, leaking transparent borders outside the visible window.
  auto bounds = ComputeRasterizedLayerBoundsInSpace(tgfxLayer, coordinateSpace);
  if (bounds.isEmpty()) {
    addWarning("rasterize skipped: layer '" +
               (layer->id.empty() ? std::string("(unnamed)") : layer->id) +
               "' reported empty bounds; falling back to vector emission.");
    return false;
  }
  auto pixelScale = _rasterScale;
  auto pngData = RenderMaskedLayer(&_gpu, buildResult.root, tgfxLayer, coordinateSpace, pixelScale);
  if (!pngData || pngData->size() == 0) {
    addWarning("rasterize failed: PNG bake for layer '" +
               (layer->id.empty() ? std::string("(unnamed)") : layer->id) +
               "' produced no pixel data; falling back to vector emission.");
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

// Dispatch a single accumulated geometry through the appropriate per-shape
// writer with the given painter applied. Each painter that renders a geometry
// goes through this function so that multi-fill / multi-stroke produces one
// SVG element per painter (overlapping in document order). Mirrors
// PPTWriter::emitGeometryWithFs.
void SVGWriter::emitGeometryWithFs(SVGBuilder& out, const AccumulatedGeometry& entry,
                                   const FillStrokeInfo& fs, float alpha) {
  FillStrokeInfo localFs = fs;
  if (localFs.textBox == nullptr) {
    localFs.textBox = entry.textBox;
  }
  switch (entry.element->nodeType()) {
    case NodeType::Rectangle:
      writeRectangle(out, static_cast<const Rectangle*>(entry.element), localFs, entry.transform,
                     alpha);
      break;
    case NodeType::Ellipse:
      writeEllipse(out, static_cast<const Ellipse*>(entry.element), localFs, entry.transform,
                   alpha);
      break;
    case NodeType::Path:
      writePath(out, static_cast<const Path*>(entry.element), localFs, entry.transform, alpha);
      break;
    case NodeType::Text: {
      auto* text = static_cast<const Text*>(entry.element);
      // Any Text that carries GlyphRun elements is treated as the authoritative
      // geometry: SVG's native <text> can't express arbitrary glyph IDs /
      // per-glyph xOffsets / anchors / rotations / skews / scales, so going
      // through writeText would silently drop everything the GlyphRun specifies
      // and fall back to renderer-driven layout, producing visibly wrong
      // output. This also covers the common case of a Text that keeps its
      // readable `text` for accessibility alongside pre-shaped GlyphRuns.
      // The convertTextToPath flag has the same effect, but only when GlyphRun
      // data is available to walk — without glyphRuns there is no geometry to
      // emit and we must fall back to native text anyway.
      if (!text->glyphRuns.empty()) {
        writeTextAsPath(out, text, localFs, entry.transform, alpha);
      } else {
        writeText(out, text, localFs, entry.transform, alpha);
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
        // The painter's effective alpha is the alpha of THIS scope, not the
        // alpha that was in effect when each geometry was collected. A Group
        // that contains geometry but whose Painter lives outside the Group
        // must NOT multiply its own alpha into the outer painter's output —
        // Group is an isolation boundary for painters even though geometry
        // propagates upward.
        for (size_t i = scopeStart; i < accumulator.size(); ++i) {
          emitGeometryWithFs(out, accumulator[i], painterFs, alpha);
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
          const std::vector<Element*> innerWalked = resolveIfEnabled(tb->elements);
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
        const std::vector<Element*> innerWalked = resolveIfEnabled(group->elements);
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
  const std::vector<Element*> walked = resolveIfEnabled(elements);

  std::vector<AccumulatedGeometry> accumulator;
  accumulator.reserve(walked.size());
  processVectorScope(out, walked, transform, alpha, parentTextBox, accumulator, /*scopeStart=*/0);
}

void SVGWriter::writeLayerContents(SVGBuilder& out, const Layer* layer, const Matrix& transform,
                                   float alpha) {
  writeElements(out, layer->contents, transform, alpha, nullptr);
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
  if (_bakeUnsupported) {
    auto features = ProbeLayerFeaturesForSVG(layer);
    if (features.needsRasterization()) {
      if (rasterizeLayerAsImage(out, layer)) {
        return;
      }
    }
  } else {
    auto features = ProbeLayerFeaturesForSVG(layer);
    if (features.needsRasterization()) {
      addWarning("layer '" + (layer->id.empty() ? std::string("(unnamed)") : layer->id) +
                 "' uses SVG-unsupported features (TextPath / TextModifier / diamond or conic "
                 "gradient) and bakeUnsupported is false; emitting as best-effort vector with "
                 "those features dropped or downgraded to radial.");
    }
  }

  auto renderPos = layer->renderPosition();
  bool needsGroup = !layer->matrix.isIdentity() || !layer->matrix3D.isIdentity() ||
                    layer->alpha < 1.0f || !layer->id.empty() || !layer->filters.empty() ||
                    !layer->styles.empty() || layer->mask != nullptr || renderPos.x != 0.0f ||
                    renderPos.y != 0.0f || !layer->customData.empty() ||
                    layer->blendMode != BlendMode::Normal || layer->hasScrollRect;

  // When groupOpacity is false, the layer's alpha is distributed to each child
  // element individually rather than applied to the composited group. Compute
  // the per-child alpha multiplier here so both the bare-through path and the
  // needs-group path can forward it to writeLayerBody.
  float perChildAlpha = (!layer->groupOpacity && layer->alpha < 1.0f) ? layer->alpha : 1.0f;

  if (!needsGroup) {
    writeLayerBody(out, layer, perChildAlpha);
    return;
  }

  out.openElement("g");
  bool needsContourClip = layer->mask != nullptr && layer->maskType == MaskType::Contour;
  // The outer <g> already gets the contour-mask clip-path from
  // writeLayerGroupAttributes. SVG only allows one `clip-path` value per
  // element, so when both contour mask and scrollRect are present we defer the
  // scrollRect clip to a middle <g> wrapper below; otherwise the second
  // attribute would silently overwrite the first.
  bool needsScrollMiddleWrapper = layer->hasScrollRect && needsContourClip;
  writeLayerGroupAttributes(out, layer, /*emitScrollClipHere=*/!needsScrollMiddleWrapper);

  bool hasContent =
      !layer->contents.empty() || !layer->children.empty() || layer->composition != nullptr;
  if (!hasContent) {
    out.closeElementSelfClosing();
    return;
  }

  out.closeElementStart();

  // Middle <g> for scrollRect when a contour-mask clip-path occupies the outer
  // <g>'s clip-path slot. The middle <g> carries no transform — its local
  // coordinate space equals the outer's (post layer-matrix), so the scroll
  // clipPath rect (anchored at 0,0 with sr.width × sr.height) is interpreted
  // identically to the un-collided case where it sat on the outer <g>.
  if (needsScrollMiddleWrapper) {
    auto scrollClipId = writeScrollRectClipDef(layer);
    out.openElement("g");
    out.addAttribute("clip-path", "url(#" + scrollClipId + ")");
    out.closeElementStart();
  }

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

  writeLayerBody(out, layer, perChildAlpha);

  if (hasScrollOffset) {
    out.closeElement();
  }

  if (needsScrollMiddleWrapper) {
    out.closeElement();
  }

  out.closeElement();
}

void SVGWriter::writeLayerBody(SVGBuilder& out, const Layer* layer, float perChildAlpha) {
  writeLayerContents(out, layer, {}, perChildAlpha);
  if (layer->composition != nullptr) {
    for (const auto* compLayer : layer->composition->layers) {
      if (perChildAlpha < 1.0f) {
        out.openElement("g");
        out.addAttribute("opacity", FloatToString(perChildAlpha));
        out.closeElementStart();
        writeLayer(out, compLayer);
        out.closeElement();
      } else {
        writeLayer(out, compLayer);
      }
    }
  }
  for (const auto* child : layer->children) {
    if (perChildAlpha < 1.0f) {
      out.openElement("g");
      out.addAttribute("opacity", FloatToString(perChildAlpha));
      out.closeElementStart();
      writeLayer(out, child);
      out.closeElement();
    } else {
      writeLayer(out, child);
    }
  }
}

void SVGWriter::writeLayerGroupAttributes(SVGBuilder& out, const Layer* layer,
                                          bool emitScrollClipHere) {
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

  if (layer->alpha < 1.0f && layer->groupOpacity) {
    out.addAttribute("opacity", FloatToString(layer->alpha));
  }

  if (layer->blendMode != BlendMode::Normal) {
    auto blendStr = BlendModeToSVGString(layer->blendMode);
    if (blendStr) {
      out.addAttribute("style", std::string("mix-blend-mode:") + blendStr);
    }
  }

  if (!layer->filters.empty() || !layer->styles.empty()) {
    auto contentBounds = ComputeLayerBounds(layer);
    auto filterId = writeFilterAndStyleDefs(layer->filters, layer->styles, contentBounds);
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
  // SVG natively supports <clipPath> so we emit a vector clip. When the layer
  // also carries a contour mask, the contour mask owns this <g>'s `clip-path`
  // and `emitScrollClipHere` is false — the caller emits the scroll clip on a
  // deferred middle <g> wrapper instead so both clips can coexist.
  if (emitScrollClipHere && layer->hasScrollRect) {
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

std::string SVGExporter::ToSVG(PAGXDocument& doc, const Options& options,
                               std::vector<std::string>* warnings) {
  // Mirror PPTExporter: resolve renderPosition() for every layer/group so authored x/y/position
  // attributes flow into the matrix helpers (BuildLayerMatrix / BuildGroupMatrix).
  if (!doc.isLayoutApplied()) {
    doc.applyLayout(options.fontConfig);
  }
  // Clamp rasterScale to a sane range. 0 / negative would silently produce a zero-pixel surface
  // (the bake then fails and the exporter falls through to the vector path, contradicting
  // bakeUnsupported=true). The upper bound keeps `static_cast<int>(ceilf(width * rasterScale))`
  // away from float→int implementation-defined behaviour on huge canvases. Range matches
  // HTMLExportOptions::rasterScale.
  float safeRasterScale = std::clamp(options.rasterScale, 0.01f, 4.0f);
  SVGBuilder svg(true, options.indent);
  SVGBuilder defs(true, options.indent, 2);
  SVGWriterContext context;
  auto layoutContext = std::make_unique<LayoutContext>(options.fontConfig);
  SVGWriter writer(&defs, &context, options.indent, options.convertTextToPath, layoutContext.get(),
                   &doc, options.bakeUnsupported, safeRasterScale, options.resolveModifiers,
                   warnings);

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
                         const Options& options, std::vector<std::string>* warnings) {
  auto svgContent = ToSVG(document, options, warnings);
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
