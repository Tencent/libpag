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

#include "pagx/HTMLExporter.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>
#include "base/utils/MathUtil.h"
#include "pagx/PAGXDocument.h"
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
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/InnerShadowFilter.h"
#include "pagx/nodes/InnerShadowStyle.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/MergePath.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/RangeSelector.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/RoundCorner.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/nodes/TextModifier.h"
#include "pagx/nodes/TextPath.h"
#include "pagx/nodes/TrimPath.h"
#include "pagx/svg/SVGBlendMode.h"
#include "pagx/svg/SVGPathParser.h"
#include "pagx/svg/SVGTextLayout.h"
#include "pagx/types/MergePathMode.h"
#include "pagx/types/Rect.h"
#include "pagx/types/SelectorTypes.h"
#include "pagx/types/TrimType.h"
#include "pagx/utils/Base64.h"
#include "pagx/utils/StringParser.h"

namespace pagx {

using pag::DegreesToRadians;
using pag::FloatNearlyZero;

//==============================================================================
// HTMLBuilder
//==============================================================================

class HTMLBuilder {
 public:
  explicit HTMLBuilder(int indentSpaces, int initialLevel = 0, size_t reserve = 4096)
      : _level(initialLevel), _spaces(indentSpaces) {
    _buf.reserve(reserve);
  }

  void openTag(const char* tag) {
    indent();
    _buf += '<';
    _buf += tag;
    _tags.push_back(tag);
  }

  void addAttr(const char* name, const std::string& value) {
    if (value.empty()) {
      return;
    }
    _buf += ' ';
    _buf += name;
    _buf += "=\"";
    _buf += escapeAttr(value);
    _buf += '"';
  }

  void closeTagStart() {
    _buf += ">\n";
    _level++;
  }

  void closeTagSelfClosing() {
    auto& tag = _tags.back();
    if (std::strcmp(tag, "div") == 0 || std::strcmp(tag, "span") == 0) {
      _buf += "></";
      _buf += tag;
      _buf += ">\n";
    } else {
      _buf += "/>\n";
    }
    _tags.pop_back();
  }

  void closeTag() {
    _level--;
    indent();
    _buf += "</";
    _buf += _tags.back();
    _buf += ">\n";
    _tags.pop_back();
  }

  void closeTagWithText(const std::string& text) {
    _buf += '>';
    _buf += escapeHTML(text);
    _buf += "</";
    _buf += _tags.back();
    _buf += ">\n";
    _tags.pop_back();
  }

  void addRawContent(const std::string& c) {
    _buf += c;
  }

  void addTextContent(const std::string& t) {
    _buf += escapeHTML(t);
  }

  int level() const {
    return _level;
  }

  std::string release() {
    return std::move(_buf);
  }

 private:
  std::string _buf = {};
  std::vector<const char*> _tags = {};
  int _level = 0;
  int _spaces = 2;

  void indent() {
    if (_level > 0) {
      _buf.append(static_cast<size_t>(_level * _spaces), ' ');
    }
  }

  static std::string escapeAttr(const std::string& s) {
    std::string r;
    r.reserve(s.size());
    for (char c : s) {
      switch (c) {
        case '&':
          r += "&amp;";
          break;
        case '"':
          r += "&quot;";
          break;
        case '\'':
          r += "&#39;";
          break;
        case '<':
          r += "&lt;";
          break;
        case '>':
          r += "&gt;";
          break;
        default:
          r += c;
      }
    }
    return r;
  }

  static std::string escapeHTML(const std::string& s) {
    std::string r;
    r.reserve(s.size());
    for (char c : s) {
      switch (c) {
        case '&':
          r += "&amp;";
          break;
        case '<':
          r += "&lt;";
          break;
        case '>':
          r += "&gt;";
          break;
        default:
          r += c;
      }
    }
    return r;
  }
};

//==============================================================================
// Static helpers
//==============================================================================

static void ColorToRGB(const Color& color, int& r, int& g, int& b) {
  r = std::clamp(static_cast<int>(std::round(color.red * 255.0f)), 0, 255);
  g = std::clamp(static_cast<int>(std::round(color.green * 255.0f)), 0, 255);
  b = std::clamp(static_cast<int>(std::round(color.blue * 255.0f)), 0, 255);
}

static std::string ColorToHex(const Color& color) {
  if (color.colorSpace == ColorSpace::DisplayP3) {
    return "color(display-p3 " + FloatToString(color.red) + " " + FloatToString(color.green) + " " +
           FloatToString(color.blue) + ")";
  }
  int r = 0, g = 0, b = 0;
  ColorToRGB(color, r, g, b);
  char buf[8] = {};
  snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
  return buf;
}

// SVG attributes (stop-color, flood-color, fill, stroke) do not support the CSS color() function.
// For DisplayP3 colors, approximate by treating the P3 channel values as sRGB.
static std::string ColorToSVGHex(const Color& color) {
  int r = 0, g = 0, b = 0;
  ColorToRGB(color, r, g, b);
  char buf[8] = {};
  snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
  return buf;
}

static std::string ColorToRGBA(const Color& color, float extra = 1.0f) {
  float a = color.alpha * extra;
  if (color.colorSpace == ColorSpace::DisplayP3) {
    return "color(display-p3 " + FloatToString(color.red) + " " + FloatToString(color.green) + " " +
           FloatToString(color.blue) + " / " + FloatToString(a) + ")";
  }
  if (a >= 1.0f) {
    return ColorToHex(color);
  }
  int r = 0, g = 0, b = 0;
  ColorToRGB(color, r, g, b);
  return "rgba(" + std::to_string(r) + "," + std::to_string(g) + "," + std::to_string(b) + "," +
         FloatToString(a) + ")";
}

static const char* DetectImageMime(const uint8_t* bytes, size_t size) {
  if (size >= 8 && bytes[0] == 0x89 && bytes[1] == 'P' && bytes[2] == 'N' && bytes[3] == 'G') {
    return "image/png";
  }
  if (size >= 3 && bytes[0] == 0xFF && bytes[1] == 0xD8 && bytes[2] == 0xFF) {
    return "image/jpeg";
  }
  if (size >= 4 && bytes[0] == 'R' && bytes[1] == 'I' && bytes[2] == 'F' && bytes[3] == 'F') {
    return "image/webp";
  }
  if (size >= 4 && bytes[0] == 'G' && bytes[1] == 'I' && bytes[2] == 'F' && bytes[3] == '8') {
    return "image/gif";
  }
  return "application/octet-stream";
}

static std::string EscapeCSSUrl(const std::string& url) {
  std::string r;
  r.reserve(url.size());
  for (char c : url) {
    if (c == ')' || c == '(' || c == '\'' || c == '"' || c == ' ' || c == '\\') {
      r += '\\';
    }
    r += c;
  }
  return r;
}

static std::string GetImageSrc(const Image* image) {
  if (image->data) {
    auto mime = DetectImageMime(image->data->bytes(), image->data->size());
    return "data:" + std::string(mime) + ";base64," +
           Base64Encode(image->data->bytes(), image->data->size());
  }
  return EscapeCSSUrl(image->filePath);
}

static std::string CSSStops(const std::vector<ColorStop*>& stops) {
  if (stops.empty()) {
    return "transparent,transparent";
  }
  if (stops.size() == 1) {
    auto c = ColorToRGBA(stops[0]->color);
    return c + "," + c;
  }
  std::string r;
  r.reserve(stops.size() * 32);
  for (size_t i = 0; i < stops.size(); i++) {
    if (i > 0) {
      r += ',';
    }
    r += ColorToRGBA(stops[i]->color);
    r += ' ';
    r += FloatToString(stops[i]->offset * 100.0f);
    r += '%';
  }
  return r;
}

static std::string MatrixToCSS(const Matrix& m) {
  std::string r = "matrix(";
  r += FloatToString(m.a) + ',' + FloatToString(m.b) + ',' + FloatToString(m.c) + ',' +
       FloatToString(m.d) + ',' + FloatToString(m.tx) + ',' + FloatToString(m.ty) + ')';
  return r;
}

static std::string Matrix3DToCSS(const Matrix3D& m) {
  std::string r = "matrix3d(";
  for (int i = 0; i < 16; i++) {
    if (i > 0) {
      r += ',';
    }
    r += FloatToString(m.values[i]);
  }
  r += ')';
  return r;
}

static const char* BlendModeToMixBlendMode(BlendMode mode) {
  auto svgStr = BlendModeToSVGString(mode);
  if (svgStr) {
    return svgStr;
  }
  if (mode == BlendMode::PlusLighter) {
    return "plus-lighter";
  }
  if (mode == BlendMode::PlusDarker) {
    // CSS has no direct equivalent for PlusDarker; approximate with darken.
    return "darken";
  }
  return nullptr;
}

static Color LerpColor(const Color& a, const Color& b, float t) {
  Color result = {};
  result.red = a.red + (b.red - a.red) * t;
  result.green = a.green + (b.green - a.green) * t;
  result.blue = a.blue + (b.blue - a.blue) * t;
  result.alpha = a.alpha + (b.alpha - a.alpha) * t;
  result.colorSpace = a.colorSpace;
  return result;
}

static std::string LayerTransformCSS(const Layer* layer) {
  if (!layer->matrix3D.isIdentity()) {
    return Matrix3DToCSS(layer->matrix3D);
  }
  // x/y translation is combined with matrix: Translate(x,y) * matrix.
  Matrix m = layer->matrix;
  if (!FloatNearlyZero(layer->x) || !FloatNearlyZero(layer->y)) {
    m = Matrix::Translate(layer->x, layer->y) * m;
  }
  if (m.isIdentity()) {
    return {};
  }
  // Use translate() shorthand when the result is a pure translation.
  bool isPureTranslation = FloatNearlyZero(m.a - 1.0f) && FloatNearlyZero(m.b) &&
                           FloatNearlyZero(m.c) && FloatNearlyZero(m.d - 1.0f);
  if (isPureTranslation) {
    return "translate(" + FloatToString(m.tx) + "px," + FloatToString(m.ty) + "px)";
  }
  return MatrixToCSS(m);
}

static Matrix BuildGroupMatrix(const Group* group) {
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

static std::string BuildPolystarPath(const Polystar* ps) {
  if (ps->pointCount <= 0.0f) {
    return {};
  }
  bool isStar = ps->type == PolystarType::Star;
  int numPts = static_cast<int>(std::ceil(ps->pointCount));
  int total = isStar ? numPts * 2 : numPts;
  float fractional = ps->pointCount - std::floor(ps->pointCount);
  bool hasRoundness = ps->outerRoundness > 0.0f || (isStar && ps->innerRoundness > 0.0f);

  // Generate vertices
  struct Vertex {
    float x = 0;
    float y = 0;
    float angle = 0;
    float radius = 0;
    float roundness = 0;
  };
  std::vector<Vertex> vertices = {};
  vertices.reserve(static_cast<size_t>(total));
  for (int i = 0; i < total; i++) {
    Vertex v = {};
    if (isStar) {
      bool outer = (i % 2 == 0);
      int pi = i / 2;
      float frac = static_cast<float>(pi) / ps->pointCount;
      float half = 0.5f / ps->pointCount;
      v.angle = ps->rotation + (outer ? frac : frac + half) * 360.0f;
      v.radius = outer ? ps->outerRadius : ps->innerRadius;
      v.roundness = outer ? ps->outerRoundness : ps->innerRoundness;
      // Scale last outer vertex radius by fractional part
      if (fractional > 0.0f && pi == numPts - 1 && outer) {
        v.radius *= fractional;
      }
    } else {
      v.angle = ps->rotation + static_cast<float>(i) / ps->pointCount * 360.0f;
      v.radius = ps->outerRadius;
      v.roundness = ps->outerRoundness;
      // Scale last vertex radius by fractional part
      if (fractional > 0.0f && i == numPts - 1) {
        v.radius *= fractional;
      }
    }
    float rad = DegreesToRadians(v.angle);
    v.x = ps->position.x + v.radius * std::cos(rad);
    v.y = ps->position.y + v.radius * std::sin(rad);
    vertices.push_back(v);
  }

  if (ps->reversed) {
    std::reverse(vertices.begin(), vertices.end());
  }

  std::string d;
  d.reserve(static_cast<size_t>(total) * 40);
  for (int i = 0; i < total; i++) {
    auto& v = vertices[static_cast<size_t>(i)];
    if (i == 0) {
      d += "M" + FloatToString(v.x) + "," + FloatToString(v.y);
    } else if (hasRoundness) {
      auto& prev = vertices[static_cast<size_t>(i - 1)];
      float prevRound = prev.roundness;
      float curRound = v.roundness;
      if (prevRound > 0.0f || curRound > 0.0f) {
        // 0.33 is the approximate Bezier tangent factor used by After Effects for polystar
        // roundness, matching the native renderer behavior.
        float dx = v.x - prev.x;
        float dy = v.y - prev.y;
        float cp1x = prev.x + dx * prevRound * 0.33f;
        float cp1y = prev.y + dy * prevRound * 0.33f;
        float cp2x = v.x - dx * curRound * 0.33f;
        float cp2y = v.y - dy * curRound * 0.33f;
        d += "C" + FloatToString(cp1x) + "," + FloatToString(cp1y) + " " + FloatToString(cp2x) +
             "," + FloatToString(cp2y) + " " + FloatToString(v.x) + "," + FloatToString(v.y);
      } else {
        d += "L" + FloatToString(v.x) + "," + FloatToString(v.y);
      }
    } else {
      d += "L" + FloatToString(v.x) + "," + FloatToString(v.y);
    }
  }
  // Close path with roundness
  if (hasRoundness && total > 1) {
    auto& prev = vertices[static_cast<size_t>(total - 1)];
    auto& v = vertices[0];
    float prevRound = prev.roundness;
    float curRound = v.roundness;
    if (prevRound > 0.0f || curRound > 0.0f) {
      float dx = v.x - prev.x;
      float dy = v.y - prev.y;
      float cp1x = prev.x + dx * prevRound * 0.33f;
      float cp1y = prev.y + dy * prevRound * 0.33f;
      float cp2x = v.x - dx * curRound * 0.33f;
      float cp2y = v.y - dy * curRound * 0.33f;
      d += "C" + FloatToString(cp1x) + "," + FloatToString(cp1y) + " " + FloatToString(cp2x) + "," +
           FloatToString(cp2y) + " " + FloatToString(v.x) + "," + FloatToString(v.y);
    }
  }
  d += 'Z';
  return d;
}

//==============================================================================
// HTMLWriterContext
//==============================================================================

// Kappa value for 90-degree bezier arc approximation: 4 * (sqrt(2) - 1) / 3
static constexpr float kBezierKappa = 0.5522847498307936f;

// Compute signed area of a path to determine winding direction.
// Positive = clockwise, negative = counter-clockwise.
static float ComputePathSignedArea(const PathData& pathData) {
  float area = 0.0f;
  Point startPoint = {};
  Point currentPoint = {};
  pathData.forEach([&](PathVerb verb, const Point* pts) {
    switch (verb) {
      case PathVerb::Move:
        startPoint = pts[0];
        currentPoint = pts[0];
        break;
      case PathVerb::Line:
        area += (currentPoint.x * pts[0].y - pts[0].x * currentPoint.y);
        currentPoint = pts[0];
        break;
      case PathVerb::Quad:
        // Approximate quadratic bezier as line to endpoint
        area += (currentPoint.x * pts[1].y - pts[1].x * currentPoint.y);
        currentPoint = pts[1];
        break;
      case PathVerb::Cubic:
        // Approximate cubic bezier as line to endpoint
        area += (currentPoint.x * pts[2].y - pts[2].x * currentPoint.y);
        currentPoint = pts[2];
        break;
      case PathVerb::Close:
        area += (currentPoint.x * startPoint.y - startPoint.x * currentPoint.y);
        currentPoint = startPoint;
        break;
    }
  });
  return area / 2.0f;
}

// Check if path is clockwise (positive area)
static bool IsPathClockwise(const PathData& pathData) {
  return ComputePathSignedArea(pathData) > 0;
}

// Compute the arc length of a cubic bezier curve using recursive subdivision.
static float ComputeCubicBezierLength(Point p0, Point p1, Point p2, Point p3, int depth = 0) {
  if (depth > 8) {
    // Max recursion depth reached, approximate with chord length
    float dx = p3.x - p0.x;
    float dy = p3.y - p0.y;
    return std::sqrt(dx * dx + dy * dy);
  }
  // Chord length
  float chordDx = p3.x - p0.x;
  float chordDy = p3.y - p0.y;
  float chordLen = std::sqrt(chordDx * chordDx + chordDy * chordDy);
  // Control polygon length
  float d01x = p1.x - p0.x, d01y = p1.y - p0.y;
  float d12x = p2.x - p1.x, d12y = p2.y - p1.y;
  float d23x = p3.x - p2.x, d23y = p3.y - p2.y;
  float polyLen = std::sqrt(d01x * d01x + d01y * d01y) + std::sqrt(d12x * d12x + d12y * d12y) +
                  std::sqrt(d23x * d23x + d23y * d23y);
  // If close enough, use average
  if (polyLen - chordLen < 0.01f) {
    return (polyLen + chordLen) / 2.0f;
  }
  // Subdivide using de Casteljau
  Point m01 = {(p0.x + p1.x) / 2, (p0.y + p1.y) / 2};
  Point m12 = {(p1.x + p2.x) / 2, (p1.y + p2.y) / 2};
  Point m23 = {(p2.x + p3.x) / 2, (p2.y + p3.y) / 2};
  Point m012 = {(m01.x + m12.x) / 2, (m01.y + m12.y) / 2};
  Point m123 = {(m12.x + m23.x) / 2, (m12.y + m23.y) / 2};
  Point mid = {(m012.x + m123.x) / 2, (m012.y + m123.y) / 2};
  return ComputeCubicBezierLength(p0, m01, m012, mid, depth + 1) +
         ComputeCubicBezierLength(mid, m123, m23, p3, depth + 1);
}

// Compute the arc length of a quadratic bezier curve (convert to cubic)
static float ComputeQuadBezierLength(Point p0, Point p1, Point p2) {
  // Convert quadratic to cubic
  Point c1 = {p0.x + 2.0f / 3.0f * (p1.x - p0.x), p0.y + 2.0f / 3.0f * (p1.y - p0.y)};
  Point c2 = {p2.x + 2.0f / 3.0f * (p1.x - p2.x), p2.y + 2.0f / 3.0f * (p1.y - p2.y)};
  return ComputeCubicBezierLength(p0, c1, c2, p2);
}

// Compute total arc length of a PathData
static float ComputePathLength(const PathData& pathData) {
  float length = 0.0f;
  Point startPoint = {};
  Point currentPoint = {};
  pathData.forEach([&](PathVerb verb, const Point* pts) {
    switch (verb) {
      case PathVerb::Move:
        startPoint = pts[0];
        currentPoint = pts[0];
        break;
      case PathVerb::Line: {
        float dx = pts[0].x - currentPoint.x;
        float dy = pts[0].y - currentPoint.y;
        length += std::sqrt(dx * dx + dy * dy);
        currentPoint = pts[0];
        break;
      }
      case PathVerb::Quad:
        length += ComputeQuadBezierLength(currentPoint, pts[0], pts[1]);
        currentPoint = pts[1];
        break;
      case PathVerb::Cubic:
        length += ComputeCubicBezierLength(currentPoint, pts[0], pts[1], pts[2]);
        currentPoint = pts[2];
        break;
      case PathVerb::Close: {
        float dx = startPoint.x - currentPoint.x;
        float dy = startPoint.y - currentPoint.y;
        length += std::sqrt(dx * dx + dy * dy);
        currentPoint = startPoint;
        break;
      }
    }
  });
  return length;
}

// Apply RoundCorner modifier to a PathData, returning a new PathData with rounded corners.
static PathData ApplyRoundCorner(const PathData& pathData, float radius) {
  if (radius <= 0 || pathData.isEmpty()) {
    return pathData;
  }
  // Parse path into contours, preserving curve segments
  struct Segment {
    PathVerb verb = PathVerb::Line;
    Point endPoint = {};
    Point ctrl1 = {};  // Quad: control point; Cubic: first control point
    Point ctrl2 = {};  // Cubic: second control point
  };
  struct Contour {
    Point startPoint = {};
    std::vector<Segment> segments = {};
    bool closed = false;
  };
  std::vector<Contour> contours = {};
  Contour currentContour = {};
  bool hasContour = false;

  pathData.forEach([&](PathVerb verb, const Point* pts) {
    switch (verb) {
      case PathVerb::Move:
        if (hasContour) {
          contours.push_back(std::move(currentContour));
          currentContour = {};
        }
        currentContour.startPoint = pts[0];
        hasContour = true;
        break;
      case PathVerb::Line: {
        Segment seg = {};
        seg.verb = PathVerb::Line;
        seg.endPoint = pts[0];
        currentContour.segments.push_back(seg);
        break;
      }
      case PathVerb::Quad: {
        Segment seg = {};
        seg.verb = PathVerb::Quad;
        seg.ctrl1 = pts[0];
        seg.endPoint = pts[1];
        currentContour.segments.push_back(seg);
        break;
      }
      case PathVerb::Cubic: {
        Segment seg = {};
        seg.verb = PathVerb::Cubic;
        seg.ctrl1 = pts[0];
        seg.ctrl2 = pts[1];
        seg.endPoint = pts[2];
        currentContour.segments.push_back(seg);
        break;
      }
      case PathVerb::Close:
        currentContour.closed = true;
        break;
    }
  });
  if (hasContour) {
    contours.push_back(std::move(currentContour));
  }

  // Build new PathData with rounded corners.
  // Only Line-to-Line junctions are considered sharp corners and get rounded.
  // Curve segments (Quad/Cubic) are preserved as-is.
  PathData result = PathDataFromSVGString("");
  for (auto& contour : contours) {
    if (contour.segments.empty()) {
      result.moveTo(contour.startPoint.x, contour.startPoint.y);
      if (contour.closed) {
        result.close();
      }
      continue;
    }
    size_t n = contour.segments.size();
    bool firstOutput = true;

    for (size_t i = 0; i < n; i++) {
      auto& seg = contour.segments[i];
      // Get the start point of this segment
      Point segStart = (i == 0) ? contour.startPoint : contour.segments[i - 1].endPoint;

      // Only round a junction if both the incoming and outgoing segments are lines
      bool incomingIsLine = (i == 0) ? true : (contour.segments[i - 1].verb == PathVerb::Line);
      bool outgoingIsLine = (seg.verb == PathVerb::Line);

      if (!incomingIsLine || !outgoingIsLine) {
        // Not a line-to-line junction: output the segment as-is
        if (firstOutput) {
          result.moveTo(segStart.x, segStart.y);
          firstOutput = false;
        }
        if (seg.verb == PathVerb::Line) {
          result.lineTo(seg.endPoint.x, seg.endPoint.y);
        } else if (seg.verb == PathVerb::Quad) {
          result.quadTo(seg.ctrl1.x, seg.ctrl1.y, seg.endPoint.x, seg.endPoint.y);
        } else if (seg.verb == PathVerb::Cubic) {
          result.cubicTo(seg.ctrl1.x, seg.ctrl1.y, seg.ctrl2.x, seg.ctrl2.y, seg.endPoint.x,
                         seg.endPoint.y);
        }
        continue;
      }

      // Both incoming and outgoing are lines: check if we can round the start vertex
      // The "start vertex" of this segment is the junction point
      Point vertex = segStart;
      // Get previous segment's start point
      bool hasPrev = (i > 0) || contour.closed;
      if (!hasPrev) {
        // First segment, no preceding edge to round
        if (firstOutput) {
          result.moveTo(vertex.x, vertex.y);
          firstOutput = false;
        }
        result.lineTo(seg.endPoint.x, seg.endPoint.y);
        continue;
      }

      Point prevStart = {};
      if (i > 0) {
        prevStart = (i >= 2) ? contour.segments[i - 2].endPoint : contour.startPoint;
      } else {
        // i==0 and closed: the edge arriving at startPoint comes from the last segment's endpoint
        prevStart = contour.segments[n - 1].endPoint;
      }

      // Compute edge vectors
      float dx1 = vertex.x - prevStart.x;
      float dy1 = vertex.y - prevStart.y;
      float dx2 = seg.endPoint.x - vertex.x;
      float dy2 = seg.endPoint.y - vertex.y;
      float len1 = std::sqrt(dx1 * dx1 + dy1 * dy1);
      float len2 = std::sqrt(dx2 * dx2 + dy2 * dy2);

      if (len1 < 0.001f || len2 < 0.001f) {
        if (firstOutput) {
          result.moveTo(vertex.x, vertex.y);
          firstOutput = false;
        }
        result.lineTo(seg.endPoint.x, seg.endPoint.y);
        continue;
      }

      float effectiveRadius = std::min({radius, len1 / 2.0f, len2 / 2.0f});
      float t1 = effectiveRadius / len1;
      float t2 = effectiveRadius / len2;
      Point p1 = {vertex.x - dx1 * t1, vertex.y - dy1 * t1};
      Point p2 = {vertex.x + dx2 * t2, vertex.y + dy2 * t2};
      Point cp1 = {p1.x + (vertex.x - p1.x) * kBezierKappa,
                   p1.y + (vertex.y - p1.y) * kBezierKappa};
      Point cp2 = {p2.x + (vertex.x - p2.x) * kBezierKappa,
                   p2.y + (vertex.y - p2.y) * kBezierKappa};

      if (firstOutput) {
        result.moveTo(p1.x, p1.y);
        firstOutput = false;
      } else {
        result.lineTo(p1.x, p1.y);
      }
      result.cubicTo(cp1.x, cp1.y, cp2.x, cp2.y, p2.x, p2.y);
      // The line from p2 to seg.endPoint will be handled by the next iteration's lineTo
      // or we output it now if this is the last segment
      if (i == n - 1 && !contour.closed) {
        result.lineTo(seg.endPoint.x, seg.endPoint.y);
      }
    }
    if (contour.closed) {
      result.close();
    }
  }
  return result;
}

// Reverse a PathData (reverse the winding direction)
static PathData ReversePathData(const PathData& pathData) {
  if (pathData.isEmpty()) {
    return pathData;
  }
  // Split into contours
  struct Segment {
    PathVerb verb = PathVerb::Move;
    std::vector<Point> points = {};
  };
  struct Contour {
    Point startPoint = {};
    std::vector<Segment> segments = {};
    bool closed = false;
  };
  std::vector<Contour> contours = {};
  Contour currentContour = {};
  bool hasContour = false;

  pathData.forEach([&](PathVerb verb, const Point* pts) {
    switch (verb) {
      case PathVerb::Move:
        if (hasContour) {
          contours.push_back(std::move(currentContour));
          currentContour = {};
        }
        currentContour.startPoint = pts[0];
        hasContour = true;
        break;
      case PathVerb::Line: {
        Segment seg = {PathVerb::Line, {pts[0]}};
        currentContour.segments.push_back(std::move(seg));
        break;
      }
      case PathVerb::Quad: {
        Segment seg = {PathVerb::Quad, {pts[0], pts[1]}};
        currentContour.segments.push_back(std::move(seg));
        break;
      }
      case PathVerb::Cubic: {
        Segment seg = {PathVerb::Cubic, {pts[0], pts[1], pts[2]}};
        currentContour.segments.push_back(std::move(seg));
        break;
      }
      case PathVerb::Close:
        currentContour.closed = true;
        break;
    }
  });
  if (hasContour) {
    contours.push_back(std::move(currentContour));
  }

  // Build reversed PathData
  PathData result = PathDataFromSVGString("");
  for (auto& contour : contours) {
    if (contour.segments.empty()) {
      result.moveTo(contour.startPoint.x, contour.startPoint.y);
      if (contour.closed) {
        result.close();
      }
      continue;
    }
    // New start point is the last segment's endpoint
    auto& lastSeg = contour.segments.back();
    Point newStart = lastSeg.points.back();
    result.moveTo(newStart.x, newStart.y);

    // Walk segments in reverse order
    for (int i = static_cast<int>(contour.segments.size()) - 1; i >= 0; i--) {
      auto& seg = contour.segments[static_cast<size_t>(i)];
      Point segStart = (i == 0) ? contour.startPoint
                                : contour.segments[static_cast<size_t>(i - 1)].points.back();
      switch (seg.verb) {
        case PathVerb::Line:
          result.lineTo(segStart.x, segStart.y);
          break;
        case PathVerb::Quad:
          // Quadratic: control point stays, endpoint becomes previous start
          result.quadTo(seg.points[0].x, seg.points[0].y, segStart.x, segStart.y);
          break;
        case PathVerb::Cubic:
          // Cubic: swap control points
          result.cubicTo(seg.points[1].x, seg.points[1].y, seg.points[0].x, seg.points[0].y,
                         segStart.x, segStart.y);
          break;
        default:
          break;
      }
    }
    if (contour.closed) {
      result.close();
    }
  }
  return result;
}

static std::string TransformPathDataToSVG(const PathData& pathData, const Matrix& m) {
  PathData result = PathDataFromSVGString("");
  pathData.forEach([&](PathVerb verb, const Point* pts) {
    switch (verb) {
      case PathVerb::Move: {
        Point p = m.mapPoint(pts[0]);
        result.moveTo(p.x, p.y);
        break;
      }
      case PathVerb::Line: {
        Point p = m.mapPoint(pts[0]);
        result.lineTo(p.x, p.y);
        break;
      }
      case PathVerb::Quad: {
        Point c = m.mapPoint(pts[0]);
        Point p = m.mapPoint(pts[1]);
        result.quadTo(c.x, c.y, p.x, p.y);
        break;
      }
      case PathVerb::Cubic: {
        Point c1 = m.mapPoint(pts[0]);
        Point c2 = m.mapPoint(pts[1]);
        Point p = m.mapPoint(pts[2]);
        result.cubicTo(c1.x, c1.y, c2.x, c2.y, p.x, p.y);
        break;
      }
      case PathVerb::Close:
        result.close();
        break;
    }
  });
  return PathDataToSVGString(result);
}

// Convert geometry to PathData
static PathData GeoToPathData(const Element* element, NodeType type) {
  PathData pathData = PathDataFromSVGString("");
  switch (type) {
    case NodeType::Rectangle: {
      auto r = static_cast<const Rectangle*>(element);
      float x = r->position.x - r->size.width / 2;
      float y = r->position.y - r->size.height / 2;
      float w = r->size.width;
      float h = r->size.height;
      if (r->roundness <= 0) {
        pathData.moveTo(x, y);
        if (r->reversed) {
          pathData.lineTo(x, y + h);
          pathData.lineTo(x + w, y + h);
          pathData.lineTo(x + w, y);
        } else {
          pathData.lineTo(x + w, y);
          pathData.lineTo(x + w, y + h);
          pathData.lineTo(x, y + h);
        }
        pathData.close();
      } else {
        float rn = std::min(r->roundness, std::min(w / 2, h / 2));
        // Rounded rect with bezier arcs at corners
        pathData.moveTo(x + rn, y);
        if (r->reversed) {
          // Counter-clockwise
          pathData.cubicTo(x + rn * (1 - kBezierKappa), y, x, y + rn * (1 - kBezierKappa), x,
                           y + rn);
          pathData.lineTo(x, y + h - rn);
          pathData.cubicTo(x, y + h - rn * (1 - kBezierKappa), x + rn * (1 - kBezierKappa), y + h,
                           x + rn, y + h);
          pathData.lineTo(x + w - rn, y + h);
          pathData.cubicTo(x + w - rn * (1 - kBezierKappa), y + h, x + w,
                           y + h - rn * (1 - kBezierKappa), x + w, y + h - rn);
          pathData.lineTo(x + w, y + rn);
          pathData.cubicTo(x + w, y + rn * (1 - kBezierKappa), x + w - rn * (1 - kBezierKappa), y,
                           x + w - rn, y);
        } else {
          // Clockwise
          pathData.lineTo(x + w - rn, y);
          pathData.cubicTo(x + w - rn * (1 - kBezierKappa), y, x + w, y + rn * (1 - kBezierKappa),
                           x + w, y + rn);
          pathData.lineTo(x + w, y + h - rn);
          pathData.cubicTo(x + w, y + h - rn * (1 - kBezierKappa), x + w - rn * (1 - kBezierKappa),
                           y + h, x + w - rn, y + h);
          pathData.lineTo(x + rn, y + h);
          pathData.cubicTo(x + rn * (1 - kBezierKappa), y + h, x, y + h - rn * (1 - kBezierKappa),
                           x, y + h - rn);
          pathData.lineTo(x, y + rn);
          pathData.cubicTo(x, y + rn * (1 - kBezierKappa), x + rn * (1 - kBezierKappa), y, x + rn,
                           y);
        }
        pathData.close();
      }
      break;
    }
    case NodeType::Ellipse: {
      auto e = static_cast<const Ellipse*>(element);
      float cx = e->position.x;
      float cy = e->position.y;
      float rx = e->size.width / 2;
      float ry = e->size.height / 2;
      // Ellipse approximated with 4 cubic bezier curves
      float kx = rx * kBezierKappa;
      float ky = ry * kBezierKappa;
      pathData.moveTo(cx - rx, cy);
      if (e->reversed) {
        pathData.cubicTo(cx - rx, cy + ky, cx - kx, cy + ry, cx, cy + ry);
        pathData.cubicTo(cx + kx, cy + ry, cx + rx, cy + ky, cx + rx, cy);
        pathData.cubicTo(cx + rx, cy - ky, cx + kx, cy - ry, cx, cy - ry);
        pathData.cubicTo(cx - kx, cy - ry, cx - rx, cy - ky, cx - rx, cy);
      } else {
        pathData.cubicTo(cx - rx, cy - ky, cx - kx, cy - ry, cx, cy - ry);
        pathData.cubicTo(cx + kx, cy - ry, cx + rx, cy - ky, cx + rx, cy);
        pathData.cubicTo(cx + rx, cy + ky, cx + kx, cy + ry, cx, cy + ry);
        pathData.cubicTo(cx - kx, cy + ry, cx - rx, cy + ky, cx - rx, cy);
      }
      pathData.close();
      break;
    }
    case NodeType::Path: {
      auto p = static_cast<const Path*>(element);
      if (p->data) {
        if (p->reversed) {
          pathData = ReversePathData(*p->data);
        } else {
          pathData.setPathData(*p->data);
        }
      }
      break;
    }
    case NodeType::Polystar: {
      // Use existing BuildPolystarPath and parse it back
      auto ps = static_cast<const Polystar*>(element);
      std::string d = BuildPolystarPath(ps);
      if (!d.empty()) {
        pathData = PathDataFromSVGString(d);
      }
      break;
    }
    default:
      break;
  }
  return pathData;
}

// RangeSelector shape functions
static float SelectorShapeSquare(float) {
  return 1.0f;
}

static float SelectorShapeRampUp(float t) {
  return t;
}

static float SelectorShapeRampDown(float t) {
  return 1.0f - t;
}

static float SelectorShapeTriangle(float t) {
  return t < 0.5f ? 2.0f * t : 2.0f * (1.0f - t);
}

static float SelectorShapeRound(float t) {
  return std::sin(t * static_cast<float>(M_PI));
}

static float SelectorShapeSmooth(float t) {
  return 0.5f - 0.5f * std::cos(t * static_cast<float>(M_PI));
}

// Get shape function by SelectorShape enum
static float ApplySelectorShape(SelectorShape shape, float t) {
  switch (shape) {
    case SelectorShape::Square:
      return SelectorShapeSquare(t);
    case SelectorShape::RampUp:
      return SelectorShapeRampUp(t);
    case SelectorShape::RampDown:
      return SelectorShapeRampDown(t);
    case SelectorShape::Triangle:
      return SelectorShapeTriangle(t);
    case SelectorShape::Round:
      return SelectorShapeRound(t);
    case SelectorShape::Smooth:
      return SelectorShapeSmooth(t);
    default:
      return 1.0f;
  }
}

// Combine selector values using SelectorMode
static float CombineSelectorValues(SelectorMode mode, float a, float b) {
  switch (mode) {
    case SelectorMode::Add:
      return a + b;
    case SelectorMode::Subtract:
      return b >= 0 ? a * (1.0f - b) : a * (-1.0f - b);
    case SelectorMode::Intersect:
      return a * b;
    case SelectorMode::Min:
      return std::min(a, b);
    case SelectorMode::Max:
      return std::max(a, b);
    case SelectorMode::Difference:
      return std::abs(a - b);
    default:
      return a + b;
  }
}

// Compute factor for a single glyph from a RangeSelector
static float ComputeRangeSelectorFactor(const RangeSelector* selector, size_t glyphIndex,
                                        size_t totalGlyphs) {
  if (totalGlyphs == 0) {
    return 0.0f;
  }
  // Normalize glyph position to [0, 1]
  float glyphPos = 0.0f;
  if (selector->unit == SelectorUnit::Percentage) {
    float denom = totalGlyphs > 1 ? static_cast<float>(totalGlyphs - 1) : 1.0f;
    glyphPos = static_cast<float>(glyphIndex) / denom;
  } else {
    // Index mode: start/end are direct indices
    glyphPos = static_cast<float>(glyphIndex);
  }
  // Apply offset
  float offset = selector->offset;
  if (selector->unit == SelectorUnit::Percentage) {
    glyphPos -= offset;
  } else {
    glyphPos -= offset;
  }
  // Wrap around (only when offset pushes the position outside [0,1])
  if (selector->unit == SelectorUnit::Percentage) {
    if (glyphPos < 0.0f || glyphPos > 1.0f) {
      glyphPos = std::fmod(glyphPos, 1.0f);
      if (glyphPos < 0) {
        glyphPos += 1.0f;
      }
    }
  }
  // Normalize to [0,1] range for both modes
  if (selector->unit == SelectorUnit::Index) {
    // In Index mode, glyphPos is already (glyphIndex - offset). Normalize to [0,1].
    glyphPos = glyphPos / static_cast<float>(totalGlyphs);
    // Wrap around
    glyphPos = std::fmod(glyphPos, 1.0f);
    if (glyphPos < 0) {
      glyphPos += 1.0f;
    }
  }
  // Check if within range (start/end always in [0,1] after normalization)
  float start = selector->start;
  float end = selector->end;
  if (selector->unit == SelectorUnit::Index) {
    start = start / static_cast<float>(totalGlyphs);
    end = end / static_cast<float>(totalGlyphs);
  }
  if (glyphPos < start || glyphPos > end) {
    return 0.0f;
  }
  // Compute normalized position within range
  float rangeSize = end - start;
  if (rangeSize <= 0) {
    return 1.0f;
  }
  float t = (glyphPos - start) / rangeSize;
  // Apply shape
  float rawInfluence = ApplySelectorShape(selector->shape, t);
  // Apply ease in/out
  if (selector->easeIn > 0 && t < 0.5f) {
    float easeT = t * 2.0f;
    rawInfluence *= 1.0f - (1.0f - easeT) * selector->easeIn;
  }
  if (selector->easeOut > 0 && t > 0.5f) {
    float easeT = (t - 0.5f) * 2.0f;
    rawInfluence *= 1.0f - easeT * selector->easeOut;
  }
  return rawInfluence * selector->weight;
}

// Arc-length LUT for TextPath
struct ArcLengthLUT {
  std::vector<float> arcLengths = {};  // cumulative arc length at each sample
  std::vector<Point> positions = {};   // position at each sample
  std::vector<float> tangents = {};    // tangent angle at each sample (radians)
  float totalLength = 0.0f;
};

// Build arc-length LUT from PathData
static ArcLengthLUT BuildArcLengthLUT(const PathData& pathData, int samplesPerSegment = 16) {
  ArcLengthLUT lut = {};
  lut.arcLengths.push_back(0.0f);
  Point startPoint = {};
  Point currentPoint = {};
  float cumLength = 0.0f;

  pathData.forEach([&](PathVerb verb, const Point* pts) {
    switch (verb) {
      case PathVerb::Move:
        startPoint = pts[0];
        currentPoint = pts[0];
        lut.positions.push_back(currentPoint);
        lut.tangents.push_back(0.0f);
        break;
      case PathVerb::Line: {
        float dx = pts[0].x - currentPoint.x;
        float dy = pts[0].y - currentPoint.y;
        float segLen = std::sqrt(dx * dx + dy * dy);
        float angle = std::atan2(dy, dx);
        for (int i = 1; i <= samplesPerSegment; i++) {
          float t = static_cast<float>(i) / samplesPerSegment;
          Point p = {currentPoint.x + dx * t, currentPoint.y + dy * t};
          cumLength += segLen / samplesPerSegment;
          lut.arcLengths.push_back(cumLength);
          lut.positions.push_back(p);
          lut.tangents.push_back(angle);
        }
        currentPoint = pts[0];
        break;
      }
      case PathVerb::Quad: {
        Point p0 = currentPoint;
        Point p1 = pts[0];
        Point p2 = pts[1];
        for (int i = 1; i <= samplesPerSegment; i++) {
          float t = static_cast<float>(i) / samplesPerSegment;
          float u = 1.0f - t;
          Point p = {u * u * p0.x + 2 * u * t * p1.x + t * t * p2.x,
                     u * u * p0.y + 2 * u * t * p1.y + t * t * p2.y};
          // Tangent: derivative of quadratic bezier
          Point tan = {2 * (1 - t) * (p1.x - p0.x) + 2 * t * (p2.x - p1.x),
                       2 * (1 - t) * (p1.y - p0.y) + 2 * t * (p2.y - p1.y)};
          float angle = std::atan2(tan.y, tan.x);
          float dx = p.x - lut.positions.back().x;
          float dy = p.y - lut.positions.back().y;
          cumLength += std::sqrt(dx * dx + dy * dy);
          lut.arcLengths.push_back(cumLength);
          lut.positions.push_back(p);
          lut.tangents.push_back(angle);
        }
        currentPoint = pts[1];
        break;
      }
      case PathVerb::Cubic: {
        Point p0 = currentPoint;
        Point p1 = pts[0];
        Point p2 = pts[1];
        Point p3 = pts[2];
        for (int i = 1; i <= samplesPerSegment; i++) {
          float t = static_cast<float>(i) / samplesPerSegment;
          float u = 1.0f - t;
          Point p = {
              u * u * u * p0.x + 3 * u * u * t * p1.x + 3 * u * t * t * p2.x + t * t * t * p3.x,
              u * u * u * p0.y + 3 * u * u * t * p1.y + 3 * u * t * t * p2.y + t * t * t * p3.y};
          // Tangent: derivative of cubic bezier
          Point tan = {
              3 * u * u * (p1.x - p0.x) + 6 * u * t * (p2.x - p1.x) + 3 * t * t * (p3.x - p2.x),
              3 * u * u * (p1.y - p0.y) + 6 * u * t * (p2.y - p1.y) + 3 * t * t * (p3.y - p2.y)};
          float angle = std::atan2(tan.y, tan.x);
          float dx = p.x - lut.positions.back().x;
          float dy = p.y - lut.positions.back().y;
          cumLength += std::sqrt(dx * dx + dy * dy);
          lut.arcLengths.push_back(cumLength);
          lut.positions.push_back(p);
          lut.tangents.push_back(angle);
        }
        currentPoint = pts[2];
        break;
      }
      case PathVerb::Close: {
        float dx = startPoint.x - currentPoint.x;
        float dy = startPoint.y - currentPoint.y;
        float segLen = std::sqrt(dx * dx + dy * dy);
        if (segLen > 0.001f) {
          float angle = std::atan2(dy, dx);
          for (int i = 1; i <= samplesPerSegment; i++) {
            float t = static_cast<float>(i) / samplesPerSegment;
            Point p = {currentPoint.x + dx * t, currentPoint.y + dy * t};
            cumLength += segLen / samplesPerSegment;
            lut.arcLengths.push_back(cumLength);
            lut.positions.push_back(p);
            lut.tangents.push_back(angle);
          }
        }
        currentPoint = startPoint;
        break;
      }
    }
  });
  lut.totalLength = cumLength;
  return lut;
}

// Sample position and tangent at a given arc length from LUT
static void SampleArcLengthLUT(const ArcLengthLUT& lut, float arcLength, Point* outPos,
                               float* outTangent) {
  if (lut.arcLengths.empty()) {
    *outPos = {};
    *outTangent = 0;
    return;
  }
  // Clamp to valid range
  arcLength = std::max(0.0f, std::min(arcLength, lut.totalLength));
  // Binary search for the segment
  auto it = std::lower_bound(lut.arcLengths.begin(), lut.arcLengths.end(), arcLength);
  size_t idx = static_cast<size_t>(std::distance(lut.arcLengths.begin(), it));
  if (idx >= lut.positions.size()) {
    idx = lut.positions.size() - 1;
  }
  if (idx == 0) {
    *outPos = lut.positions[0];
    *outTangent = lut.tangents[0];
    return;
  }
  // Linear interpolation between samples
  float prevLen = lut.arcLengths[idx - 1];
  float nextLen = lut.arcLengths[idx];
  float segLen = nextLen - prevLen;
  float t = (segLen > 0.001f) ? (arcLength - prevLen) / segLen : 0.0f;
  Point prevPos = lut.positions[idx - 1];
  Point nextPos = lut.positions[idx];
  *outPos = {prevPos.x + (nextPos.x - prevPos.x) * t, prevPos.y + (nextPos.y - prevPos.y) * t};
  *outTangent = lut.tangents[idx];
}

static std::string RectToPathData(const Rectangle* r) {
  float x = r->position.x - r->size.width / 2;
  float y = r->position.y - r->size.height / 2;
  float w = r->size.width;
  float h = r->size.height;
  if (r->roundness <= 0) {
    if (r->reversed) {
      // Counter-clockwise: top-left → bottom-left → bottom-right → top-right
      return "M" + FloatToString(x) + "," + FloatToString(y) + "V" + FloatToString(y + h) + "H" +
             FloatToString(x + w) + "V" + FloatToString(y) + "Z";
    }
    return "M" + FloatToString(x) + "," + FloatToString(y) + "H" + FloatToString(x + w) + "V" +
           FloatToString(y + h) + "H" + FloatToString(x) + "Z";
  }
  float rn = std::min(r->roundness, std::min(w / 2, h / 2));
  std::string d;
  if (r->reversed) {
    // Counter-clockwise direction with reversed arc sweep
    d += "M" + FloatToString(x + rn) + "," + FloatToString(y);
    d += "A" + FloatToString(rn) + "," + FloatToString(rn) + " 0 0 0 " + FloatToString(x) + "," +
         FloatToString(y + rn);
    d += "V" + FloatToString(y + h - rn);
    d += "A" + FloatToString(rn) + "," + FloatToString(rn) + " 0 0 0 " + FloatToString(x + rn) +
         "," + FloatToString(y + h);
    d += "H" + FloatToString(x + w - rn);
    d += "A" + FloatToString(rn) + "," + FloatToString(rn) + " 0 0 0 " + FloatToString(x + w) +
         "," + FloatToString(y + h - rn);
    d += "V" + FloatToString(y + rn);
    d += "A" + FloatToString(rn) + "," + FloatToString(rn) + " 0 0 0 " + FloatToString(x + w - rn) +
         "," + FloatToString(y);
    d += "Z";
  } else {
    d += "M" + FloatToString(x + rn) + "," + FloatToString(y);
    d += "H" + FloatToString(x + w - rn);
    d += "A" + FloatToString(rn) + "," + FloatToString(rn) + " 0 0 1 " + FloatToString(x + w) +
         "," + FloatToString(y + rn);
    d += "V" + FloatToString(y + h - rn);
    d += "A" + FloatToString(rn) + "," + FloatToString(rn) + " 0 0 1 " + FloatToString(x + w - rn) +
         "," + FloatToString(y + h);
    d += "H" + FloatToString(x + rn);
    d += "A" + FloatToString(rn) + "," + FloatToString(rn) + " 0 0 1 " + FloatToString(x) + "," +
         FloatToString(y + h - rn);
    d += "V" + FloatToString(y + rn);
    d += "A" + FloatToString(rn) + "," + FloatToString(rn) + " 0 0 1 " + FloatToString(x + rn) +
         "," + FloatToString(y);
    d += "Z";
  }
  return d;
}

static std::string EllipseToPathData(const Ellipse* e) {
  float cx = e->position.x;
  float cy = e->position.y;
  float rx = e->size.width / 2;
  float ry = e->size.height / 2;
  // sweep-flag: 0 = counter-clockwise (reversed), 1 = clockwise (normal)
  const char* sweep = e->reversed ? "0" : "1";
  return "M" + FloatToString(cx - rx) + "," + FloatToString(cy) + "A" + FloatToString(rx) + "," +
         FloatToString(ry) + " 0 1 " + sweep + " " + FloatToString(cx + rx) + "," +
         FloatToString(cy) + "A" + FloatToString(rx) + "," + FloatToString(ry) + " 0 1 " + sweep +
         " " + FloatToString(cx - rx) + "," + FloatToString(cy) + "Z";
}

static std::string ReversePathDataToSVGString(const PathData& pathData) {
  // Split the path into contours, reverse each contour, then emit SVG string.
  struct Segment {
    PathVerb verb = PathVerb::Move;
    // For Line: 1 point (endpoint). For Quad: 2 points (ctrl, end). For Cubic: 3 points.
    std::vector<Point> points = {};
  };
  struct Contour {
    Point startPoint = {};
    std::vector<Segment> segments = {};
    bool closed = false;
  };

  std::vector<Contour> contours = {};
  Contour currentContour = {};
  bool hasContour = false;

  const auto& verbs = pathData.verbs();
  const auto& points = pathData.points();
  size_t pointIndex = 0;
  for (auto verb : verbs) {
    const Point* pts = points.data() + pointIndex;
    switch (verb) {
      case PathVerb::Move:
        if (hasContour) {
          contours.push_back(std::move(currentContour));
          currentContour = {};
        }
        currentContour.startPoint = pts[0];
        hasContour = true;
        break;
      case PathVerb::Line: {
        Segment seg = {};
        seg.verb = PathVerb::Line;
        seg.points = {pts[0]};
        currentContour.segments.push_back(std::move(seg));
        break;
      }
      case PathVerb::Quad: {
        Segment seg = {};
        seg.verb = PathVerb::Quad;
        seg.points = {pts[0], pts[1]};
        currentContour.segments.push_back(std::move(seg));
        break;
      }
      case PathVerb::Cubic: {
        Segment seg = {};
        seg.verb = PathVerb::Cubic;
        seg.points = {pts[0], pts[1], pts[2]};
        currentContour.segments.push_back(std::move(seg));
        break;
      }
      case PathVerb::Close:
        currentContour.closed = true;
        break;
    }
    pointIndex += PathData::PointsPerVerb(verb);
  }
  if (hasContour) {
    contours.push_back(std::move(currentContour));
  }

  // Build reversed SVG string
  std::string result;
  result.reserve(pathData.verbs().size() * 24);
  for (auto& contour : contours) {
    if (contour.segments.empty()) {
      result +=
          "M" + FloatToString(contour.startPoint.x) + " " + FloatToString(contour.startPoint.y);
      if (contour.closed) {
        result += "Z";
      }
      continue;
    }
    // The new start point is the last segment's endpoint
    auto& lastSeg = contour.segments.back();
    Point newStart = lastSeg.points.back();
    result += "M" + FloatToString(newStart.x) + " " + FloatToString(newStart.y);

    // Walk segments in reverse order
    for (int i = static_cast<int>(contour.segments.size()) - 1; i >= 0; i--) {
      auto& seg = contour.segments[static_cast<size_t>(i)];
      Point segStart = (i == 0) ? contour.startPoint
                                : contour.segments[static_cast<size_t>(i - 1)].points.back();
      switch (seg.verb) {
        case PathVerb::Line:
          result += "L" + FloatToString(segStart.x) + " " + FloatToString(segStart.y);
          break;
        case PathVerb::Quad:
          // Quad: reverse control point stays the same, but endpoint becomes the previous start
          result += "Q" + FloatToString(seg.points[0].x) + " " + FloatToString(seg.points[0].y) +
                    " " + FloatToString(segStart.x) + " " + FloatToString(segStart.y);
          break;
        case PathVerb::Cubic:
          // Cubic: swap control points (cp2 becomes cp1, cp1 becomes cp2)
          result += "C" + FloatToString(seg.points[1].x) + " " + FloatToString(seg.points[1].y) +
                    " " + FloatToString(seg.points[0].x) + " " + FloatToString(seg.points[0].y) +
                    " " + FloatToString(segStart.x) + " " + FloatToString(segStart.y);
          break;
        default:
          break;
      }
    }
    if (contour.closed) {
      result += "Z";
    }
  }
  return result;
}

static std::string GetPathSVGString(const Path* path) {
  if (!path->data || path->data->isEmpty()) {
    return {};
  }
  if (path->reversed) {
    return ReversePathDataToSVGString(*path->data);
  }
  return PathDataToSVGString(*path->data);
}

class HTMLWriterContext {
 public:
  float docWidth = 0;
  float docHeight = 0;
  std::unordered_set<const Composition*> visitedCompositions = {};

  std::string nextId(const std::string& prefix) {
    return prefix + std::to_string(_id++);
  }

 private:
  int _id = 0;
};

//==============================================================================
// HTMLWriter
//==============================================================================

class HTMLWriter {
 public:
  HTMLWriter(HTMLBuilder* defs, HTMLWriterContext* ctx) : _defs(defs), _ctx(ctx) {
  }

  void writeLayer(HTMLBuilder& out, const Layer* layer, float parentAlpha = 1.0f,
                  bool distributeAlpha = false);

 private:
  HTMLBuilder* _defs = nullptr;
  HTMLWriterContext* _ctx = nullptr;

  struct GeoInfo {
    NodeType type = NodeType::Rectangle;
    const Element* element = nullptr;
    std::string modifiedPathData = {};  // Used when RoundCorner modifier is applied
  };

  // Color source conversions
  std::string colorToCSS(const ColorSource* src, float* outAlpha);
  std::string colorToSVGFill(const ColorSource* src, float* outAlpha);
  void writeSVGGradientDef(const ColorSource* src, const std::string& id);

  // Rendering
  void writeLayerContents(HTMLBuilder& out, const Layer* layer, float alpha, bool distribute,
                          LayerPlacement targetPlacement);
  void writeElements(HTMLBuilder& out, const std::vector<Element*>& elements, float alpha,
                     bool distribute, LayerPlacement targetPlacement);
  void renderGeo(HTMLBuilder& out, const std::vector<GeoInfo>& geos, const Fill* fill,
                 const Stroke* stroke, float alpha, bool hasTrim, const TrimPath* trim,
                 bool hasMerge, MergePathMode mergeMode = MergePathMode::Append);
  bool canCSS(const std::vector<GeoInfo>& geos, const Fill* fill, const Stroke* stroke,
              bool hasTrim, bool hasMerge);
  void renderCSSDiv(HTMLBuilder& out, const GeoInfo& geo, const Fill* fill, float alpha,
                    BlendMode painterBlend);
  void renderSVG(HTMLBuilder& out, const std::vector<GeoInfo>& geos, const Fill* fill,
                 const Stroke* stroke, float alpha, BlendMode painterBlend,
                 const TrimPath* trim = nullptr, MergePathMode mergeMode = MergePathMode::Append);
  void writeText(HTMLBuilder& out, const Text* text, const Fill* fill, const Stroke* stroke,
                 const TextBox* tb, float alpha);
  void writeTextModifier(HTMLBuilder& out, const std::vector<GeoInfo>& geos,
                         const TextModifier* modifier, const Fill* fill, const Stroke* stroke,
                         const TextBox* tb, float alpha);
  void writeTextPath(HTMLBuilder& out, const std::vector<GeoInfo>& geos, const TextPath* textPath,
                     const Fill* fill, const Stroke* stroke, const TextBox* tb, float alpha);
  void writeGlyphRunSVG(HTMLBuilder& out, const Text* text, const Fill* fill, const Stroke* stroke,
                        float alpha);
  void writeGroup(HTMLBuilder& out, const Group* group, float alpha, bool distribute);
  void writeRepeater(HTMLBuilder& out, const Repeater* rep, const std::vector<GeoInfo>& geos,
                     const Fill* fill, const Stroke* stroke, float alpha);
  void writeComposition(HTMLBuilder& out, const Composition* comp, float alpha = 1.0f,
                        bool distribute = false);
  void paintGeos(HTMLBuilder& out, const std::vector<GeoInfo>& geos, const Fill* fill,
                 const Stroke* stroke, const TextBox* textBox, float alpha, bool hasTrim,
                 const TrimPath* curTrim, bool hasMerge, MergePathMode mergeMode);
  void applyTrimAttrs(HTMLBuilder& builder, const TrimPath* trim, bool isEllipse = false);
  void applyTrimAttrsContinuous(HTMLBuilder& builder, const TrimPath* trim,
                                const std::vector<float>& pathLengths, float totalLength,
                                size_t geoIndex, bool isEllipse = false);
  float computeGeoPathLength(const GeoInfo& geo);

  // Filter defs
  std::string writeFilterDefs(const std::vector<LayerFilter*>& filters);

  // Mask/clip defs
  std::string writeMaskCSS(const Layer* mask, MaskType type);
  std::string writeClipDef(const Layer* mask);
  void writeClipContent(HTMLBuilder& out, const Layer* layer, const Matrix& parent);

  // SVG fill/stroke attributes
  void applySVGFill(HTMLBuilder& out, const Fill* fill);
  void applySVGStroke(HTMLBuilder& out, const Stroke* stroke, float pathLength = 0.0f);
};

//==============================================================================
// HTMLWriter – color source
//==============================================================================

std::string HTMLWriter::colorToCSS(const ColorSource* src, float* outAlpha) {
  if (!src) {
    if (outAlpha) {
      *outAlpha = 1.0f;
    }
    return "transparent";
  }
  switch (src->nodeType()) {
    case NodeType::SolidColor: {
      auto sc = static_cast<const SolidColor*>(src);
      if (outAlpha) {
        *outAlpha = sc->color.alpha;
      }
      return ColorToHex(sc->color);
    }
    case NodeType::LinearGradient: {
      auto g = static_cast<const LinearGradient*>(src);
      if (outAlpha) {
        *outAlpha = 1.0f;
      }
      // CSS linear-gradient can only express angle, not start/end positions.
      // If the matrix has scale or translation, fall back to SVG.
      if (!g->matrix.isIdentity()) {
        bool hasTranslation = !FloatNearlyZero(g->matrix.tx) || !FloatNearlyZero(g->matrix.ty);
        float sx = std::sqrt(g->matrix.a * g->matrix.a + g->matrix.b * g->matrix.b);
        float sy = std::sqrt(g->matrix.c * g->matrix.c + g->matrix.d * g->matrix.d);
        bool hasScale = !FloatNearlyZero(sx - 1.0f) || !FloatNearlyZero(sy - 1.0f);
        if (hasTranslation || hasScale) {
          return {};
        }
      }
      Point s = g->matrix.mapPoint(g->startPoint);
      Point e = g->matrix.mapPoint(g->endPoint);
      float ang = std::atan2(e.y - s.y, e.x - s.x) * 180.0f / static_cast<float>(M_PI) + 90.0f;
      return "linear-gradient(" + FloatToString(ang) + "deg," + CSSStops(g->colorStops) + ")";
    }
    case NodeType::RadialGradient: {
      auto g = static_cast<const RadialGradient*>(src);
      if (outAlpha) {
        *outAlpha = 1.0f;
      }
      // Check for rotation or non-uniform scale in the matrix.
      // CSS radial-gradient(circle ...) cannot express a rotated ellipse.
      float sx = std::sqrt(g->matrix.a * g->matrix.a + g->matrix.b * g->matrix.b);
      float sy = std::sqrt(g->matrix.c * g->matrix.c + g->matrix.d * g->matrix.d);
      bool hasRotation = !FloatNearlyZero(g->matrix.b) || !FloatNearlyZero(g->matrix.c);
      bool nonUniformScale = !FloatNearlyZero(sx - sy);
      if (hasRotation && nonUniformScale) {
        // Cannot express rotated ellipse in CSS; caller should use SVG path
        return {};
      }
      Point c = g->matrix.mapPoint(g->center);
      float r = g->radius * sx;
      if (nonUniformScale) {
        float ry = g->radius * sy;
        return "radial-gradient(ellipse " + FloatToString(r) + "px " + FloatToString(ry) +
               "px at " + FloatToString(c.x) + "px " + FloatToString(c.y) + "px," +
               CSSStops(g->colorStops) + ")";
      }
      return "radial-gradient(circle " + FloatToString(r) + "px at " + FloatToString(c.x) + "px " +
             FloatToString(c.y) + "px," + CSSStops(g->colorStops) + ")";
    }
    case NodeType::ConicGradient: {
      auto g = static_cast<const ConicGradient*>(src);
      if (outAlpha) {
        *outAlpha = 1.0f;
      }
      // CSS conic-gradient supports rotation via the 'from' angle.
      // Extract rotation from the matrix and apply it to the start angle.
      float matRotation = 0.0f;
      if (!g->matrix.isIdentity()) {
        float sx = std::sqrt(g->matrix.a * g->matrix.a + g->matrix.b * g->matrix.b);
        float sy = std::sqrt(g->matrix.c * g->matrix.c + g->matrix.d * g->matrix.d);
        bool hasNonUniformScale = !FloatNearlyZero(sx - 1.0f) || !FloatNearlyZero(sy - 1.0f);
        if (hasNonUniformScale) {
          return {};  // Non-uniform scale cannot be expressed in CSS conic-gradient.
        }
        matRotation = std::atan2(g->matrix.b, g->matrix.a) * 180.0f / static_cast<float>(M_PI);
      }
      Point c = g->matrix.mapPoint(g->center);
      float cssStartAng = g->startAngle + 90.0f + matRotation;
      float sweepRange = g->endAngle - g->startAngle;
      if (FloatNearlyZero(sweepRange - 360.0f)) {
        return "conic-gradient(from " + FloatToString(cssStartAng) + "deg at " +
               FloatToString(c.x) + "px " + FloatToString(c.y) + "px," + CSSStops(g->colorStops) +
               ")";
      }
      // Partial sweep: map stop offsets to explicit angle positions
      std::string stops;
      for (size_t i = 0; i < g->colorStops.size(); i++) {
        if (i > 0) {
          stops += ',';
        }
        stops += ColorToRGBA(g->colorStops[i]->color);
        float angle = cssStartAng + g->colorStops[i]->offset * sweepRange;
        stops += ' ' + FloatToString(angle) + "deg";
      }
      return "conic-gradient(at " + FloatToString(c.x) + "px " + FloatToString(c.y) + "px," +
             stops + ")";
    }
    case NodeType::ImagePattern: {
      auto p = static_cast<const ImagePattern*>(src);
      if (outAlpha) {
        *outAlpha = 1.0f;
      }
      if (!p->image) {
        return "transparent";
      }
      std::string imgUrl = "url(" + GetImageSrc(p->image) + ")";
      // Store tileMode/filterMode/matrix info in the CSS background shorthand
      // The caller will handle background-repeat/background-size/image-rendering separately
      return imgUrl;
    }
    case NodeType::DiamondGradient: {
      // Approximate diamond gradient with a radial gradient (visual difference expected)
      auto g = static_cast<const DiamondGradient*>(src);
      if (outAlpha) {
        *outAlpha = 1.0f;
      }
      Point c = g->matrix.mapPoint(g->center);
      float sx = std::sqrt(g->matrix.a * g->matrix.a + g->matrix.b * g->matrix.b);
      float r = g->radius * sx;
      return "radial-gradient(circle " + FloatToString(r) + "px at " + FloatToString(c.x) + "px " +
             FloatToString(c.y) + "px," + CSSStops(g->colorStops) + ")";
    }
    default:
      if (outAlpha) {
        *outAlpha = 1.0f;
      }
      return "transparent";
  }
}

std::string HTMLWriter::colorToSVGFill(const ColorSource* src, float* outAlpha) {
  if (!src) {
    if (outAlpha) {
      *outAlpha = 1.0f;
    }
    return "none";
  }
  if (src->nodeType() == NodeType::SolidColor) {
    auto sc = static_cast<const SolidColor*>(src);
    if (outAlpha) {
      *outAlpha = sc->color.alpha;
    }
    return ColorToSVGHex(sc->color);
  }
  if (src->nodeType() == NodeType::LinearGradient || src->nodeType() == NodeType::RadialGradient) {
    std::string id = _ctx->nextId("grad");
    writeSVGGradientDef(src, id);
    if (outAlpha) {
      *outAlpha = 1.0f;
    }
    return "url(#" + id + ")";
  }
  if (src->nodeType() == NodeType::ConicGradient) {
    // Use SVG pattern with foreignObject to embed CSS conic-gradient.
    // Must use userSpaceOnUse because CSS gradient coordinates are in absolute px.
    auto g = static_cast<const ConicGradient*>(src);
    std::string id = _ctx->nextId("cpat");
    _defs->openTag("pattern");
    _defs->addAttr("id", id);
    _defs->addAttr("patternUnits", "userSpaceOnUse");
    _defs->addAttr("x", "0");
    _defs->addAttr("y", "0");
    std::string patternSize = FloatToString(std::max({_ctx->docWidth, _ctx->docHeight, 10000.0f}));
    _defs->addAttr("width", patternSize);
    _defs->addAttr("height", patternSize);
    _defs->closeTagStart();
    _defs->openTag("foreignObject");
    _defs->addAttr("width", patternSize);
    _defs->addAttr("height", patternSize);
    _defs->closeTagStart();
    // Extract rotation angle from the matrix and add it to the CSS start angle.
    // Use the original center (not matrix-mapped) since the conic-gradient center
    // is relative to the element's local coordinate space within the SVG pattern.
    float matRotation = std::atan2(g->matrix.b, g->matrix.a) * 180.0f / static_cast<float>(M_PI);
    Point c = g->center;
    float cssStartAng = g->startAngle + 90.0f + matRotation;
    float sweepRange = g->endAngle - g->startAngle;
    std::string cssGrad;
    if (FloatNearlyZero(sweepRange - 360.0f)) {
      cssGrad = "conic-gradient(from " + FloatToString(cssStartAng) + "deg at " +
                FloatToString(c.x) + "px " + FloatToString(c.y) + "px," + CSSStops(g->colorStops) +
                ")";
    } else {
      std::string stops;
      for (size_t i = 0; i < g->colorStops.size(); i++) {
        if (i > 0) {
          stops += ',';
        }
        stops += ColorToRGBA(g->colorStops[i]->color);
        float angle = cssStartAng + g->colorStops[i]->offset * sweepRange;
        stops += ' ' + FloatToString(angle) + "deg";
      }
      cssGrad = "conic-gradient(at " + FloatToString(c.x) + "px " + FloatToString(c.y) + "px," +
                stops + ")";
    }
    _defs->openTag("div");
    _defs->addAttr("xmlns", "http://www.w3.org/1999/xhtml");
    _defs->addAttr("style", "width:100%;height:100%;background:" + cssGrad);
    _defs->closeTagSelfClosing();
    _defs->closeTag();  // </foreignObject>
    _defs->closeTag();  // </pattern>
    if (outAlpha) {
      *outAlpha = 1.0f;
    }
    return "url(#" + id + ")";
  }
  if (src->nodeType() == NodeType::DiamondGradient) {
    // Approximate diamond gradient with SVG radialGradient
    auto g = static_cast<const DiamondGradient*>(src);
    std::string id = _ctx->nextId("grad");
    _defs->openTag("radialGradient");
    _defs->addAttr("id", id);
    _defs->addAttr("cx", FloatToString(g->center.x));
    _defs->addAttr("cy", FloatToString(g->center.y));
    _defs->addAttr("r", FloatToString(g->radius));
    _defs->addAttr("gradientUnits", "userSpaceOnUse");
    if (!g->matrix.isIdentity()) {
      _defs->addAttr("gradientTransform", MatrixToCSS(g->matrix));
    }
    _defs->closeTagStart();
    for (auto* stop : g->colorStops) {
      _defs->openTag("stop");
      _defs->addAttr("offset", FloatToString(stop->offset));
      _defs->addAttr("stop-color", ColorToSVGHex(stop->color));
      if (stop->color.alpha < 1.0f) {
        _defs->addAttr("stop-opacity", FloatToString(stop->color.alpha));
      }
      _defs->closeTagSelfClosing();
    }
    _defs->closeTag();
    if (outAlpha) {
      *outAlpha = 1.0f;
    }
    return "url(#" + id + ")";
  }
  if (src->nodeType() == NodeType::ImagePattern) {
    auto p = static_cast<const ImagePattern*>(src);
    if (!p->image) {
      if (outAlpha) {
        *outAlpha = 1.0f;
      }
      return "none";
    }
    std::string imgUrl = "url(" + GetImageSrc(p->image) + ")";
    std::string repeatX = "no-repeat";
    std::string repeatY = "no-repeat";
    if (p->tileModeX == TileMode::Repeat || p->tileModeX == TileMode::Mirror) {
      repeatX = "repeat";
    }
    if (p->tileModeY == TileMode::Repeat || p->tileModeY == TileMode::Mirror) {
      repeatY = "repeat";
    }
    std::string bgRepeat = (repeatX == repeatY) ? repeatX : (repeatX + " " + repeatY);
    bool isTilingSVG = (p->tileModeX == TileMode::Repeat || p->tileModeX == TileMode::Mirror ||
                        p->tileModeY == TileMode::Repeat || p->tileModeY == TileMode::Mirror);
    std::string bgSize = isTilingSVG ? "" : "cover";
    std::string bgPos;
    if (!p->matrix.isIdentity()) {
      float sx = std::sqrt(p->matrix.a * p->matrix.a + p->matrix.b * p->matrix.b);
      float sy = std::sqrt(p->matrix.c * p->matrix.c + p->matrix.d * p->matrix.d);
      if (!FloatNearlyZero(sx - 1.0f) || !FloatNearlyZero(sy - 1.0f)) {
        bgSize = FloatToString(sx * 100.0f) + "% " + FloatToString(sy * 100.0f) + "%";
      }
      if (!FloatNearlyZero(p->matrix.tx) || !FloatNearlyZero(p->matrix.ty)) {
        bgPos = FloatToString(p->matrix.tx) + "px " + FloatToString(p->matrix.ty) + "px";
      }
    }
    std::string cssStyle =
        "width:100%;height:100%;background-image:" + imgUrl + ";background-repeat:" + bgRepeat;
    if (!bgSize.empty()) {
      cssStyle += ";background-size:" + bgSize;
    }
    if (!bgPos.empty()) {
      cssStyle += ";background-position:" + bgPos;
    }
    if (p->filterMode == FilterMode::Nearest) {
      cssStyle += ";image-rendering:pixelated";
    }
    std::string id = _ctx->nextId("ipat");
    _defs->openTag("pattern");
    _defs->addAttr("id", id);
    _defs->addAttr("patternUnits", "userSpaceOnUse");
    _defs->addAttr("x", "0");
    _defs->addAttr("y", "0");
    std::string patternSize = FloatToString(std::max({_ctx->docWidth, _ctx->docHeight, 10000.0f}));
    _defs->addAttr("width", patternSize);
    _defs->addAttr("height", patternSize);
    _defs->closeTagStart();
    _defs->openTag("foreignObject");
    _defs->addAttr("width", patternSize);
    _defs->addAttr("height", patternSize);
    _defs->closeTagStart();
    _defs->openTag("div");
    _defs->addAttr("xmlns", "http://www.w3.org/1999/xhtml");
    _defs->addAttr("style", cssStyle);
    _defs->closeTagSelfClosing();
    _defs->closeTag();  // </foreignObject>
    _defs->closeTag();  // </pattern>
    if (outAlpha) {
      *outAlpha = 1.0f;
    }
    return "url(#" + id + ")";
  }
  if (outAlpha) {
    *outAlpha = 1.0f;
  }
  return "none";
}

void HTMLWriter::writeSVGGradientDef(const ColorSource* src, const std::string& id) {
  if (src->nodeType() == NodeType::LinearGradient) {
    auto g = static_cast<const LinearGradient*>(src);
    _defs->openTag("linearGradient");
    _defs->addAttr("id", id);
    _defs->addAttr("x1", FloatToString(g->startPoint.x));
    _defs->addAttr("y1", FloatToString(g->startPoint.y));
    _defs->addAttr("x2", FloatToString(g->endPoint.x));
    _defs->addAttr("y2", FloatToString(g->endPoint.y));
    _defs->addAttr("gradientUnits", "userSpaceOnUse");
    if (!g->matrix.isIdentity()) {
      _defs->addAttr("gradientTransform", MatrixToCSS(g->matrix));
    }
    _defs->closeTagStart();
    for (auto* stop : g->colorStops) {
      _defs->openTag("stop");
      _defs->addAttr("offset", FloatToString(stop->offset));
      _defs->addAttr("stop-color", ColorToSVGHex(stop->color));
      if (stop->color.alpha < 1.0f) {
        _defs->addAttr("stop-opacity", FloatToString(stop->color.alpha));
      }
      _defs->closeTagSelfClosing();
    }
    _defs->closeTag();
  } else if (src->nodeType() == NodeType::RadialGradient) {
    auto g = static_cast<const RadialGradient*>(src);
    _defs->openTag("radialGradient");
    _defs->addAttr("id", id);
    _defs->addAttr("cx", FloatToString(g->center.x));
    _defs->addAttr("cy", FloatToString(g->center.y));
    _defs->addAttr("r", FloatToString(g->radius));
    _defs->addAttr("gradientUnits", "userSpaceOnUse");
    if (!g->matrix.isIdentity()) {
      _defs->addAttr("gradientTransform", MatrixToCSS(g->matrix));
    }
    _defs->closeTagStart();
    for (auto* stop : g->colorStops) {
      _defs->openTag("stop");
      _defs->addAttr("offset", FloatToString(stop->offset));
      _defs->addAttr("stop-color", ColorToSVGHex(stop->color));
      if (stop->color.alpha < 1.0f) {
        _defs->addAttr("stop-opacity", FloatToString(stop->color.alpha));
      }
      _defs->closeTagSelfClosing();
    }
    _defs->closeTag();
  }
}

//==============================================================================
// HTMLWriter – filter defs
//==============================================================================

std::string HTMLWriter::writeFilterDefs(const std::vector<LayerFilter*>& filters) {
  if (filters.empty()) {
    return {};
  }
  bool needSVG = false;
  for (auto* f : filters) {
    auto t = f->nodeType();
    if (t == NodeType::BlurFilter) {
      auto b = static_cast<const BlurFilter*>(f);
      if (b->blurX != b->blurY) {
        needSVG = true;
      }
    } else if (t == NodeType::DropShadowFilter) {
      auto s = static_cast<const DropShadowFilter*>(f);
      if (s->blurX != s->blurY || s->shadowOnly) {
        needSVG = true;
      }
    } else if (t == NodeType::InnerShadowFilter || t == NodeType::ColorMatrixFilter ||
               t == NodeType::BlendFilter) {
      needSVG = true;
    }
  }
  if (!needSVG) {
    std::string css;
    for (auto* f : filters) {
      if (!css.empty()) {
        css += ' ';
      }
      if (f->nodeType() == NodeType::BlurFilter) {
        auto b = static_cast<const BlurFilter*>(f);
        css += "blur(" + FloatToString(b->blurX) + "px)";
      } else if (f->nodeType() == NodeType::DropShadowFilter) {
        auto s = static_cast<const DropShadowFilter*>(f);
        css += "drop-shadow(" + FloatToString(s->offsetX) + "px " + FloatToString(s->offsetY) +
               "px " + FloatToString(s->blurX) + "px " + ColorToRGBA(s->color) + ")";
      }
    }
    return css;
  }
  std::string fid = _ctx->nextId("filter");
  _defs->openTag("filter");
  _defs->addAttr("id", fid);
  _defs->addAttr("x", "-50%");
  _defs->addAttr("y", "-50%");
  _defs->addAttr("width", "200%");
  _defs->addAttr("height", "200%");
  _defs->addAttr("color-interpolation-filters", "sRGB");
  _defs->closeTagStart();
  int si = 0;
  std::vector<std::string> dropRes = {};
  std::vector<std::string> innerRes = {};
  bool needSrc = false;
  for (auto* f : filters) {
    switch (f->nodeType()) {
      case NodeType::BlurFilter: {
        auto b = static_cast<const BlurFilter*>(f);
        // TODO: BlurFilter.tileMode is ignored. SVG feGaussianBlur has no equivalent of
        // TileMode (Clamp/Repeat/Mirror/Decal); it always uses Decal-like behavior.
        _defs->openTag("feGaussianBlur");
        _defs->addAttr("in", "SourceGraphic");
        std::string sd = FloatToString(b->blurX);
        if (b->blurX != b->blurY) {
          sd += " " + FloatToString(b->blurY);
        }
        _defs->addAttr("stdDeviation", sd);
        _defs->closeTagSelfClosing();
        break;
      }
      case NodeType::DropShadowFilter: {
        auto s = static_cast<const DropShadowFilter*>(f);
        std::string i = std::to_string(si++);
        _defs->openTag("feGaussianBlur");
        _defs->addAttr("in", "SourceAlpha");
        std::string sd = FloatToString(s->blurX);
        if (s->blurX != s->blurY) {
          sd += " " + FloatToString(s->blurY);
        }
        _defs->addAttr("stdDeviation", sd);
        _defs->addAttr("result", "sBlur" + i);
        _defs->closeTagSelfClosing();
        _defs->openTag("feOffset");
        _defs->addAttr("in", "sBlur" + i);
        if (!FloatNearlyZero(s->offsetX)) {
          _defs->addAttr("dx", FloatToString(s->offsetX));
        }
        if (!FloatNearlyZero(s->offsetY)) {
          _defs->addAttr("dy", FloatToString(s->offsetY));
        }
        _defs->addAttr("result", "sOff" + i);
        _defs->closeTagSelfClosing();
        _defs->openTag("feColorMatrix");
        _defs->addAttr("in", "sOff" + i);
        _defs->addAttr("type", "matrix");
        _defs->addAttr("values", "0 0 0 0 " + FloatToString(s->color.red) + " 0 0 0 0 " +
                                     FloatToString(s->color.green) + " 0 0 0 0 " +
                                     FloatToString(s->color.blue) + " 0 0 0 " +
                                     FloatToString(s->color.alpha) + " 0");
        _defs->addAttr("result", "sDrop" + i);
        _defs->closeTagSelfClosing();
        dropRes.push_back("sDrop" + i);
        if (!s->shadowOnly) {
          needSrc = true;
        }
        break;
      }
      case NodeType::InnerShadowFilter: {
        auto s = static_cast<const InnerShadowFilter*>(f);
        std::string i = std::to_string(si++);
        _defs->openTag("feOffset");
        _defs->addAttr("in", "SourceAlpha");
        if (!FloatNearlyZero(s->offsetX)) {
          _defs->addAttr("dx", FloatToString(s->offsetX));
        }
        if (!FloatNearlyZero(s->offsetY)) {
          _defs->addAttr("dy", FloatToString(s->offsetY));
        }
        _defs->addAttr("result", "iOff" + i);
        _defs->closeTagSelfClosing();
        _defs->openTag("feComposite");
        _defs->addAttr("in", "SourceAlpha");
        _defs->addAttr("in2", "iOff" + i);
        _defs->addAttr("operator", "arithmetic");
        _defs->addAttr("k2", "-1");
        _defs->addAttr("k3", "1");
        _defs->addAttr("result", "iComp" + i);
        _defs->closeTagSelfClosing();
        std::string sd = FloatToString(s->blurX);
        if (s->blurX != s->blurY) {
          sd += " " + FloatToString(s->blurY);
        }
        _defs->openTag("feGaussianBlur");
        _defs->addAttr("in", "iComp" + i);
        _defs->addAttr("stdDeviation", sd);
        _defs->addAttr("result", "iBlur" + i);
        _defs->closeTagSelfClosing();
        _defs->openTag("feColorMatrix");
        _defs->addAttr("in", "iBlur" + i);
        _defs->addAttr("type", "matrix");
        _defs->addAttr("values", "0 0 0 0 " + FloatToString(s->color.red) + " 0 0 0 0 " +
                                     FloatToString(s->color.green) + " 0 0 0 0 " +
                                     FloatToString(s->color.blue) + " 0 0 0 " +
                                     FloatToString(s->color.alpha) + " 0");
        _defs->addAttr("result", "iShadow" + i);
        _defs->closeTagSelfClosing();
        innerRes.push_back("iShadow" + i);
        if (!s->shadowOnly) {
          needSrc = true;
        }
        break;
      }
      case NodeType::ColorMatrixFilter: {
        auto cm = static_cast<const ColorMatrixFilter*>(f);
        _defs->openTag("feColorMatrix");
        _defs->addAttr("in", "SourceGraphic");
        _defs->addAttr("type", "matrix");
        std::string v;
        for (size_t j = 0; j < cm->matrix.size(); j++) {
          if (j > 0) {
            v += ' ';
          }
          v += FloatToString(cm->matrix[j]);
        }
        _defs->addAttr("values", v);
        _defs->closeTagSelfClosing();
        break;
      }
      case NodeType::BlendFilter: {
        auto bf = static_cast<const BlendFilter*>(f);
        std::string i = std::to_string(si++);
        _defs->openTag("feFlood");
        _defs->addAttr("flood-color", ColorToSVGHex(bf->color));
        if (bf->color.alpha < 1.0f) {
          _defs->addAttr("flood-opacity", FloatToString(bf->color.alpha));
        }
        _defs->addAttr("result", "bFlood" + i);
        _defs->closeTagSelfClosing();
        _defs->openTag("feBlend");
        _defs->addAttr("in", "bFlood" + i);
        _defs->addAttr("in2", "SourceGraphic");
        auto ms = BlendModeToMixBlendMode(bf->blendMode);
        if (ms) {
          _defs->addAttr("mode", ms);
        }
        _defs->addAttr("result", "bBlend" + i);
        _defs->closeTagSelfClosing();
        _defs->openTag("feComposite");
        _defs->addAttr("in", "bBlend" + i);
        _defs->addAttr("in2", "SourceAlpha");
        _defs->addAttr("operator", "in");
        _defs->closeTagSelfClosing();
        break;
      }
      default:
        break;
    }
  }
  if (!dropRes.empty() || !innerRes.empty()) {
    if (needSrc || (dropRes.size() + innerRes.size()) > 1) {
      _defs->openTag("feMerge");
      _defs->closeTagStart();
      for (auto& r : dropRes) {
        _defs->openTag("feMergeNode");
        _defs->addAttr("in", r);
        _defs->closeTagSelfClosing();
      }
      if (needSrc) {
        _defs->openTag("feMergeNode");
        _defs->addAttr("in", "SourceGraphic");
        _defs->closeTagSelfClosing();
      }
      for (auto& r : innerRes) {
        _defs->openTag("feMergeNode");
        _defs->addAttr("in", r);
        _defs->closeTagSelfClosing();
      }
      _defs->closeTag();
    }
  }
  _defs->closeTag();
  return "url(#" + fid + ")";
}

//==============================================================================
// HTMLWriter – mask / clip defs
//==============================================================================

// Build an inline SVG data URI for use as CSS mask-image. This avoids the Chrome
// limitation where CSS mask:url(#id) cannot reference SVG <mask> elements from
// HTML div elements.
std::string HTMLWriter::writeMaskCSS(const Layer* mask, MaskType type) {
  // Scan for Fill painters to get the color source for luminance masks.
  const Fill* maskFill = nullptr;
  for (auto* e : mask->contents) {
    if (e->nodeType() == NodeType::Fill) {
      maskFill = static_cast<const Fill*>(e);
      break;
    }
  }

  bool useFillColor = (type == MaskType::Luminance && maskFill != nullptr && maskFill->color);
  std::string fillAttr = "white";
  float fillOpacity = 1.0f;

  // Build the inline SVG content. For luminance masks with gradients, the gradient
  // definition must be included inside this self-contained SVG.
  HTMLBuilder svg(0);
  svg.openTag("svg");
  svg.addAttr("xmlns", "http://www.w3.org/2000/svg");
  svg.closeTagStart();

  if (useFillColor) {
    // Write gradient defs directly into the inline SVG.
    auto* src = maskFill->color;
    if (src->nodeType() == NodeType::LinearGradient ||
        src->nodeType() == NodeType::RadialGradient) {
      std::string gradId = "g";
      svg.openTag("defs");
      svg.closeTagStart();
      if (src->nodeType() == NodeType::LinearGradient) {
        auto g = static_cast<const LinearGradient*>(src);
        svg.openTag("linearGradient");
        svg.addAttr("id", gradId);
        svg.addAttr("x1", FloatToString(g->startPoint.x));
        svg.addAttr("y1", FloatToString(g->startPoint.y));
        svg.addAttr("x2", FloatToString(g->endPoint.x));
        svg.addAttr("y2", FloatToString(g->endPoint.y));
        svg.addAttr("gradientUnits", "userSpaceOnUse");
        if (!g->matrix.isIdentity()) {
          svg.addAttr("gradientTransform", MatrixToCSS(g->matrix));
        }
        svg.closeTagStart();
        for (auto* stop : g->colorStops) {
          svg.openTag("stop");
          svg.addAttr("offset", FloatToString(stop->offset));
          svg.addAttr("stop-color", ColorToSVGHex(stop->color));
          if (stop->color.alpha < 1.0f) {
            svg.addAttr("stop-opacity", FloatToString(stop->color.alpha));
          }
          svg.closeTagSelfClosing();
        }
        svg.closeTag();
      } else {
        auto g = static_cast<const RadialGradient*>(src);
        svg.openTag("radialGradient");
        svg.addAttr("id", gradId);
        svg.addAttr("cx", FloatToString(g->center.x));
        svg.addAttr("cy", FloatToString(g->center.y));
        svg.addAttr("r", FloatToString(g->radius));
        svg.addAttr("gradientUnits", "userSpaceOnUse");
        if (!g->matrix.isIdentity()) {
          svg.addAttr("gradientTransform", MatrixToCSS(g->matrix));
        }
        svg.closeTagStart();
        for (auto* stop : g->colorStops) {
          svg.openTag("stop");
          svg.addAttr("offset", FloatToString(stop->offset));
          svg.addAttr("stop-color", ColorToSVGHex(stop->color));
          if (stop->color.alpha < 1.0f) {
            svg.addAttr("stop-opacity", FloatToString(stop->color.alpha));
          }
          svg.closeTagSelfClosing();
        }
        svg.closeTag();
      }
      svg.closeTag();  // </defs>
      fillAttr = "url(#g)";
      fillOpacity = maskFill->alpha;
    } else if (src->nodeType() == NodeType::SolidColor) {
      auto sc = static_cast<const SolidColor*>(src);
      fillAttr = ColorToSVGHex(sc->color);
      fillOpacity = sc->color.alpha * maskFill->alpha;
    }
  }

  for (auto* e : mask->contents) {
    if (e->nodeType() == NodeType::Rectangle) {
      auto rect = static_cast<const Rectangle*>(e);
      svg.openTag("rect");
      float x = rect->position.x - rect->size.width / 2;
      float y = rect->position.y - rect->size.height / 2;
      if (!FloatNearlyZero(x)) {
        svg.addAttr("x", FloatToString(x));
      }
      if (!FloatNearlyZero(y)) {
        svg.addAttr("y", FloatToString(y));
      }
      svg.addAttr("width", FloatToString(rect->size.width));
      svg.addAttr("height", FloatToString(rect->size.height));
      if (rect->roundness > 0) {
        svg.addAttr("rx", FloatToString(rect->roundness));
      }
      svg.addAttr("fill", fillAttr);
      if (fillOpacity < 1.0f) {
        svg.addAttr("fill-opacity", FloatToString(fillOpacity));
      }
      svg.closeTagSelfClosing();
    } else if (e->nodeType() == NodeType::Ellipse) {
      auto el = static_cast<const Ellipse*>(e);
      svg.openTag("ellipse");
      svg.addAttr("cx", FloatToString(el->position.x));
      svg.addAttr("cy", FloatToString(el->position.y));
      svg.addAttr("rx", FloatToString(el->size.width / 2));
      svg.addAttr("ry", FloatToString(el->size.height / 2));
      svg.addAttr("fill", fillAttr);
      if (fillOpacity < 1.0f) {
        svg.addAttr("fill-opacity", FloatToString(fillOpacity));
      }
      svg.closeTagSelfClosing();
    } else if (e->nodeType() == NodeType::Path) {
      auto p = static_cast<const Path*>(e);
      std::string d = GetPathSVGString(p);
      if (!d.empty()) {
        svg.openTag("path");
        svg.addAttr("d", d);
        svg.addAttr("fill", fillAttr);
        if (fillOpacity < 1.0f) {
          svg.addAttr("fill-opacity", FloatToString(fillOpacity));
        }
        svg.closeTagSelfClosing();
      }
    } else if (e->nodeType() == NodeType::Polystar) {
      auto ps = static_cast<const Polystar*>(e);
      std::string d = BuildPolystarPath(ps);
      if (!d.empty()) {
        svg.openTag("path");
        svg.addAttr("d", d);
        svg.addAttr("fill", fillAttr);
        if (fillOpacity < 1.0f) {
          svg.addAttr("fill-opacity", FloatToString(fillOpacity));
        }
        svg.closeTagSelfClosing();
      }
    }
  }
  svg.closeTag();  // </svg>

  // Build the CSS properties for mask-image with an inline SVG data URI.
  std::string svgContent = svg.release();
  // Remove newlines for a compact single-line data URI.
  std::string compactSVG;
  compactSVG.reserve(svgContent.size());
  for (char c : svgContent) {
    if (c != '\n') {
      compactSVG += c;
    }
  }
  // Percent-encode characters that are problematic in CSS url() data URIs.
  std::string encoded;
  encoded.reserve(compactSVG.size() * 2);
  for (char c : compactSVG) {
    switch (c) {
      case '<':
        encoded += "%3C";
        break;
      case '>':
        encoded += "%3E";
        break;
      case '#':
        encoded += "%23";
        break;
      case '"':
        encoded += "%22";
        break;
      default:
        encoded += c;
        break;
    }
  }
  std::string dataURI = "url('data:image/svg+xml," + encoded + "')";
  std::string css = ";-webkit-mask-image:" + dataURI;
  css += ";mask-image:" + dataURI;
  css += ";-webkit-mask-size:100% 100%;mask-size:100% 100%";
  if (type == MaskType::Luminance) {
    css += ";-webkit-mask-mode:luminance;mask-mode:luminance";
  } else {
    css += ";-webkit-mask-mode:alpha;mask-mode:alpha";
  }
  return css;
}

std::string HTMLWriter::writeClipDef(const Layer* mask) {
  std::string id = _ctx->nextId("clip");
  _defs->openTag("clipPath");
  _defs->addAttr("id", id);
  _defs->closeTagStart();
  writeClipContent(*_defs, mask, {});
  _defs->closeTag();
  return id;
}

void HTMLWriter::writeClipContent(HTMLBuilder& out, const Layer* layer, const Matrix& parent) {
  Matrix lm = layer->matrix;
  if (layer->x != 0 || layer->y != 0) {
    lm = Matrix::Translate(layer->x, layer->y) * lm;
  }
  Matrix combined = parent * lm;
  std::string tr = combined.isIdentity() ? "" : MatrixToCSS(combined);
  for (auto* e : layer->contents) {
    if (e->nodeType() == NodeType::Path) {
      auto p = static_cast<const Path*>(e);
      std::string d = GetPathSVGString(p);
      if (!d.empty()) {
        out.openTag("path");
        if (!tr.empty()) {
          out.addAttr("transform", tr);
        }
        out.addAttr("d", d);
        out.closeTagSelfClosing();
      }
    } else if (e->nodeType() == NodeType::Rectangle) {
      auto r = static_cast<const Rectangle*>(e);
      out.openTag("rect");
      if (!tr.empty()) {
        out.addAttr("transform", tr);
      }
      float x = r->position.x - r->size.width / 2;
      float y = r->position.y - r->size.height / 2;
      if (!FloatNearlyZero(x)) {
        out.addAttr("x", FloatToString(x));
      }
      if (!FloatNearlyZero(y)) {
        out.addAttr("y", FloatToString(y));
      }
      out.addAttr("width", FloatToString(r->size.width));
      out.addAttr("height", FloatToString(r->size.height));
      if (r->roundness > 0) {
        out.addAttr("rx", FloatToString(r->roundness));
      }
      out.closeTagSelfClosing();
    } else if (e->nodeType() == NodeType::Ellipse) {
      auto el = static_cast<const Ellipse*>(e);
      out.openTag("ellipse");
      if (!tr.empty()) {
        out.addAttr("transform", tr);
      }
      out.addAttr("cx", FloatToString(el->position.x));
      out.addAttr("cy", FloatToString(el->position.y));
      out.addAttr("rx", FloatToString(el->size.width / 2));
      out.addAttr("ry", FloatToString(el->size.height / 2));
      out.closeTagSelfClosing();
    } else if (e->nodeType() == NodeType::Polystar) {
      auto ps = static_cast<const Polystar*>(e);
      std::string d = BuildPolystarPath(ps);
      if (!d.empty()) {
        out.openTag("path");
        if (!tr.empty()) {
          out.addAttr("transform", tr);
        }
        out.addAttr("d", d);
        out.closeTagSelfClosing();
      }
    }
  }
  for (auto* child : layer->children) {
    writeClipContent(out, child, combined);
  }
}

//==============================================================================
// HTMLWriter – SVG fill/stroke helpers
//==============================================================================

void HTMLWriter::applySVGFill(HTMLBuilder& out, const Fill* fill) {
  if (!fill) {
    out.addAttr("fill", "none");
    return;
  }
  float alpha = 1.0f;
  std::string f = colorToSVGFill(fill->color, &alpha);
  out.addAttr("fill", f);
  float ea = alpha * fill->alpha;
  if (ea < 1.0f) {
    out.addAttr("fill-opacity", FloatToString(ea));
  }
  if (fill->fillRule == FillRule::EvenOdd) {
    out.addAttr("fill-rule", "evenodd");
  }
}

void HTMLWriter::applySVGStroke(HTMLBuilder& out, const Stroke* stroke, float pathLength) {
  if (!stroke) {
    return;
  }
  float alpha = 1.0f;
  std::string s = colorToSVGFill(stroke->color, &alpha);
  out.addAttr("stroke", s);
  float ea = alpha * stroke->alpha;
  if (ea < 1.0f) {
    out.addAttr("stroke-opacity", FloatToString(ea));
  }
  // Double stroke width for Inside/Outside alignment (used with clip-path)
  float effectiveWidth = stroke->width;
  if (stroke->align != StrokeAlign::Center) {
    effectiveWidth *= 2.0f;
  }
  if (effectiveWidth != 1.0f) {
    out.addAttr("stroke-width", FloatToString(effectiveWidth));
  }
  if (stroke->cap == LineCap::Round) {
    out.addAttr("stroke-linecap", "round");
  } else if (stroke->cap == LineCap::Square) {
    out.addAttr("stroke-linecap", "square");
  }
  if (stroke->join == LineJoin::Round) {
    out.addAttr("stroke-linejoin", "round");
  } else if (stroke->join == LineJoin::Bevel) {
    out.addAttr("stroke-linejoin", "bevel");
  }
  if (stroke->join == LineJoin::Miter && stroke->miterLimit != 4.0f) {
    out.addAttr("stroke-miterlimit", FloatToString(stroke->miterLimit));
  }
  if (!stroke->dashes.empty()) {
    // When dashAdaptive is true, scale dash intervals to fit whole number of pattern cycles.
    std::vector<float> dashValues = stroke->dashes;
    if (stroke->dashAdaptive && pathLength > 0) {
      float patternLength = 0.0f;
      for (float dv : stroke->dashes) {
        patternLength += dv;
      }
      if (patternLength > 0) {
        int n = static_cast<int>(std::round(pathLength / patternLength));
        if (n > 0) {
          float scaleFactor = pathLength / (static_cast<float>(n) * patternLength);
          for (size_t j = 0; j < dashValues.size(); j++) {
            dashValues[j] = stroke->dashes[j] * scaleFactor;
          }
        }
      }
    }
    std::string d;
    for (size_t i = 0; i < dashValues.size(); i++) {
      if (i > 0) {
        d += ',';
      }
      d += FloatToString(dashValues[i]);
    }
    out.addAttr("stroke-dasharray", d);
  }
  if (stroke->dashOffset != 0.0f) {
    out.addAttr("stroke-dashoffset", FloatToString(stroke->dashOffset));
  }
}

//==============================================================================
// HTMLWriter – geometry rendering
//==============================================================================

bool HTMLWriter::canCSS(const std::vector<GeoInfo>& geos, const Fill* fill, const Stroke* stroke,
                        bool hasTrim, bool hasMerge) {
  if (hasTrim || hasMerge || geos.size() != 1 || stroke) {
    return false;
  }
  if (!geos[0].modifiedPathData.empty()) {
    return false;
  }
  auto t = geos[0].type;
  if (t != NodeType::Rectangle && t != NodeType::Ellipse) {
    return false;
  }
  if (fill) {
    // CSS background-color cannot express fill-rule: evenodd
    if (fill->fillRule == FillRule::EvenOdd) {
      return false;
    }
    // LinearGradient: CSS linear-gradient only expresses angle, not start/end positions.
    // Fall back to SVG when the matrix has scale or translation that changes gradient geometry.
    if (fill->color && fill->color->nodeType() == NodeType::LinearGradient) {
      auto g = static_cast<const LinearGradient*>(fill->color);
      if (!FloatNearlyZero(g->matrix.tx) || !FloatNearlyZero(g->matrix.ty)) {
        return false;
      }
      float sx = std::sqrt(g->matrix.a * g->matrix.a + g->matrix.b * g->matrix.b);
      float sy = std::sqrt(g->matrix.c * g->matrix.c + g->matrix.d * g->matrix.d);
      if (!FloatNearlyZero(sx - 1.0f) || !FloatNearlyZero(sy - 1.0f)) {
        return false;
      }
    }
    // RadialGradient with rotation + non-uniform scale cannot be expressed in CSS
    if (fill->color && fill->color->nodeType() == NodeType::RadialGradient) {
      auto g = static_cast<const RadialGradient*>(fill->color);
      float sx = std::sqrt(g->matrix.a * g->matrix.a + g->matrix.b * g->matrix.b);
      float sy = std::sqrt(g->matrix.c * g->matrix.c + g->matrix.d * g->matrix.d);
      bool hasRotation = !FloatNearlyZero(g->matrix.b) || !FloatNearlyZero(g->matrix.c);
      bool nonUniformScale = !FloatNearlyZero(sx - sy);
      if (hasRotation && nonUniformScale) {
        return false;
      }
    }
    // DiamondGradient has no CSS equivalent; SVG radialGradient with gradientTransform is closer.
    if (fill->color && fill->color->nodeType() == NodeType::DiamondGradient) {
      return false;
    }
    // ConicGradient with non-uniform scale cannot be expressed in CSS conic-gradient.
    if (fill->color && fill->color->nodeType() == NodeType::ConicGradient) {
      auto g = static_cast<const ConicGradient*>(fill->color);
      float sx = std::sqrt(g->matrix.a * g->matrix.a + g->matrix.b * g->matrix.b);
      float sy = std::sqrt(g->matrix.c * g->matrix.c + g->matrix.d * g->matrix.d);
      if (!FloatNearlyZero(sx - 1.0f) || !FloatNearlyZero(sy - 1.0f)) {
        return false;
      }
    }
  }
  return true;
}

void HTMLWriter::renderCSSDiv(HTMLBuilder& out, const GeoInfo& geo, const Fill* fill, float alpha,
                              BlendMode painterBlend) {
  bool isRect = geo.type == NodeType::Rectangle;
  float left = 0, top = 0, w = 0, h = 0;
  float roundness = 0;
  bool isEllipse = false;

  if (isRect) {
    auto rect = static_cast<const Rectangle*>(geo.element);
    left = rect->position.x - rect->size.width / 2;
    top = rect->position.y - rect->size.height / 2;
    w = rect->size.width;
    h = rect->size.height;
    roundness = std::min(rect->roundness, std::min(w / 2, h / 2));
  } else {
    auto el = static_cast<const Ellipse*>(geo.element);
    left = el->position.x - el->size.width / 2;
    top = el->position.y - el->size.height / 2;
    w = el->size.width;
    h = el->size.height;
    isEllipse = true;
  }

  std::string style = "position:absolute;left:" + FloatToString(left) +
                      "px;top:" + FloatToString(top) + "px;width:" + FloatToString(w) +
                      "px;height:" + FloatToString(h) + "px";

  if (isEllipse) {
    style += ";border-radius:50%";
  } else if (roundness > 0) {
    style += ";border-radius:" + FloatToString(roundness) + "px";
  }

  if (fill && fill->color) {
    float ca = 1.0f;
    std::string css = colorToCSS(fill->color, &ca);
    auto ct = fill->color->nodeType();
    if (ct == NodeType::LinearGradient || ct == NodeType::RadialGradient ||
        ct == NodeType::ConicGradient) {
      style += ";background:" + css;
      float fillOpacity = ca * fill->alpha;
      if (fillOpacity < 1.0f) {
        alpha *= fillOpacity;
      }
    } else if (ct == NodeType::ImagePattern) {
      auto p = static_cast<const ImagePattern*>(fill->color);
      style += ";background-image:" + css;
      // Tile modes
      std::string repeatX = "no-repeat";
      std::string repeatY = "no-repeat";
      if (p->tileModeX == TileMode::Repeat || p->tileModeX == TileMode::Mirror) {
        repeatX = "repeat";
      }
      if (p->tileModeY == TileMode::Repeat || p->tileModeY == TileMode::Mirror) {
        repeatY = "repeat";
      }
      if (repeatX == repeatY) {
        style += ";background-repeat:" + repeatX;
      } else {
        style += ";background-repeat:" + repeatX + " " + repeatY;
      }
      // Matrix transform → background-size and background-position
      if (!p->matrix.isIdentity()) {
        float sx = std::sqrt(p->matrix.a * p->matrix.a + p->matrix.b * p->matrix.b);
        float sy = std::sqrt(p->matrix.c * p->matrix.c + p->matrix.d * p->matrix.d);
        if (!FloatNearlyZero(sx - 1.0f) || !FloatNearlyZero(sy - 1.0f)) {
          style += ";background-size:" + FloatToString(sx * 100.0f) + "% " +
                   FloatToString(sy * 100.0f) + "%";
        }
        if (!FloatNearlyZero(p->matrix.tx) || !FloatNearlyZero(p->matrix.ty)) {
          style += ";background-position:" + FloatToString(p->matrix.tx) + "px " +
                   FloatToString(p->matrix.ty) + "px";
        }
      } else {
        // Only use 'cover' for non-tiling patterns; tiling modes need natural image size.
        bool isTiling = (p->tileModeX == TileMode::Repeat || p->tileModeX == TileMode::Mirror ||
                         p->tileModeY == TileMode::Repeat || p->tileModeY == TileMode::Mirror);
        if (!isTiling) {
          style += ";background-size:cover";
        }
      }
      // Filter mode
      // TODO: ImagePattern.mipmapMode is ignored. CSS/SVG has no direct control over mipmap
      // sampling; browsers handle mipmap generation internally.
      if (p->filterMode == FilterMode::Nearest) {
        style += ";image-rendering:pixelated";
      }
      if (fill->alpha < 1.0f) {
        alpha *= fill->alpha;
      }
    } else if (ct == NodeType::SolidColor) {
      float ea = ca * fill->alpha;
      if (ea < 1.0f) {
        auto sc = static_cast<const SolidColor*>(fill->color);
        style += ";background-color:" + ColorToRGBA(sc->color, fill->alpha);
      } else {
        style += ";background-color:" + css;
      }
    } else {
      style += ";background-color:" + css;
    }
  }
  if (painterBlend != BlendMode::Normal) {
    auto blendStr = BlendModeToMixBlendMode(painterBlend);
    if (blendStr) {
      style += ";mix-blend-mode:";
      style += blendStr;
    }
  }
  if (alpha < 1.0f) {
    style += ";opacity:" + FloatToString(alpha);
  }

  out.openTag("div");
  out.addAttr("style", style);
  out.closeTagSelfClosing();
}

void HTMLWriter::renderSVG(HTMLBuilder& out, const std::vector<GeoInfo>& geos, const Fill* fill,
                           const Stroke* stroke, float alpha, BlendMode painterBlend,
                           const TrimPath* trim, MergePathMode) {
  float pad = 0.0f;
  if (stroke) {
    pad = (stroke->align != StrokeAlign::Center) ? stroke->width * 2.0f : stroke->width;
  }
  float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
  for (auto& g : geos) {
    float gx = 0, gy = 0, gw = 0, gh = 0;
    if (!g.modifiedPathData.empty()) {
      PathData pd = PathDataFromSVGString(g.modifiedPathData);
      if (!pd.isEmpty()) {
        Rect b = pd.getBounds();
        gx = b.x;
        gy = b.y;
        gw = b.width;
        gh = b.height;
      }
    } else {
      switch (g.type) {
        case NodeType::Rectangle: {
          auto r = static_cast<const Rectangle*>(g.element);
          gx = r->position.x - r->size.width / 2;
          gy = r->position.y - r->size.height / 2;
          gw = r->size.width;
          gh = r->size.height;
          break;
        }
        case NodeType::Ellipse: {
          auto e = static_cast<const Ellipse*>(g.element);
          gx = e->position.x - e->size.width / 2;
          gy = e->position.y - e->size.height / 2;
          gw = e->size.width;
          gh = e->size.height;
          break;
        }
        case NodeType::Path: {
          auto p = static_cast<const Path*>(g.element);
          if (p->data && !p->data->isEmpty()) {
            Rect b = p->data->getBounds();
            gx = b.x;
            gy = b.y;
            gw = b.width;
            gh = b.height;
          }
          break;
        }
        case NodeType::Polystar: {
          auto ps = static_cast<const Polystar*>(g.element);
          float r = std::max(ps->outerRadius, ps->innerRadius);
          gx = ps->position.x - r;
          gy = ps->position.y - r;
          gw = r * 2;
          gh = r * 2;
          break;
        }
        default:
          break;
      }
    }
    x0 = std::min(x0, gx - pad);
    y0 = std::min(y0, gy - pad);
    x1 = std::max(x1, gx + gw + pad);
    y1 = std::max(y1, gy + gh + pad);
  }
  float sw = x1 - x0;
  float sh = y1 - y0;
  if (sw <= 0 || sh <= 0) {
    return;
  }

  std::string svgStyle =
      "position:absolute;left:" + FloatToString(x0) + "px;top:" + FloatToString(y0) + "px";
  if (painterBlend != BlendMode::Normal) {
    auto blendStr = BlendModeToMixBlendMode(painterBlend);
    if (blendStr) {
      svgStyle += ";mix-blend-mode:";
      svgStyle += blendStr;
    }
  }
  if (alpha < 1.0f) {
    svgStyle += ";opacity:" + FloatToString(alpha);
  }

  out.openTag("svg");
  out.addAttr("xmlns", "http://www.w3.org/2000/svg");
  out.addAttr("width", FloatToString(sw));
  out.addAttr("height", FloatToString(sh));
  out.addAttr("viewBox", FloatToString(x0) + " " + FloatToString(y0) + " " + FloatToString(sw) +
                             " " + FloatToString(sh));
  out.addAttr("style", svgStyle);
  out.closeTagStart();

  // Handle stroke align Inside/Outside: double width + clipPath
  bool needStrokeClip = stroke && stroke->align != StrokeAlign::Center;
  std::string strokeClipId;
  if (needStrokeClip) {
    strokeClipId = _ctx->nextId("sclip");
    out.openTag("defs");
    out.closeTagStart();
    out.openTag("clipPath");
    out.addAttr("id", strokeClipId);
    if (stroke->align == StrokeAlign::Outside) {
      out.addAttr("clip-rule", "evenodd");
    }
    out.closeTagStart();
    // For Outside stroke: add a large covering rect first, then the shape holes (evenodd)
    if (stroke->align == StrokeAlign::Outside) {
      out.openTag("rect");
      out.addAttr("x", FloatToString(x0));
      out.addAttr("y", FloatToString(y0));
      out.addAttr("width", FloatToString(sw));
      out.addAttr("height", FloatToString(sh));
      out.closeTagSelfClosing();
    }
    // Write shape geometries as clip content
    for (auto& g : geos) {
      switch (g.type) {
        case NodeType::Rectangle: {
          auto r = static_cast<const Rectangle*>(g.element);
          float cx = r->position.x - r->size.width / 2;
          float cy = r->position.y - r->size.height / 2;
          out.openTag("rect");
          if (!FloatNearlyZero(cx)) {
            out.addAttr("x", FloatToString(cx));
          }
          if (!FloatNearlyZero(cy)) {
            out.addAttr("y", FloatToString(cy));
          }
          out.addAttr("width", FloatToString(r->size.width));
          out.addAttr("height", FloatToString(r->size.height));
          if (r->roundness > 0) {
            out.addAttr("rx", FloatToString(r->roundness));
          }
          out.closeTagSelfClosing();
          break;
        }
        case NodeType::Ellipse: {
          auto e = static_cast<const Ellipse*>(g.element);
          out.openTag("ellipse");
          out.addAttr("cx", FloatToString(e->position.x));
          out.addAttr("cy", FloatToString(e->position.y));
          out.addAttr("rx", FloatToString(e->size.width / 2));
          out.addAttr("ry", FloatToString(e->size.height / 2));
          out.closeTagSelfClosing();
          break;
        }
        case NodeType::Path: {
          auto p = static_cast<const Path*>(g.element);
          std::string clipD = GetPathSVGString(p);
          if (!clipD.empty()) {
            out.openTag("path");
            out.addAttr("d", clipD);
            out.closeTagSelfClosing();
          }
          break;
        }
        case NodeType::Polystar: {
          auto ps = static_cast<const Polystar*>(g.element);
          std::string clipD = BuildPolystarPath(ps);
          if (!clipD.empty()) {
            out.openTag("path");
            out.addAttr("d", clipD);
            out.closeTagSelfClosing();
          }
          break;
        }
        default:
          break;
      }
    }
    out.closeTag();  // </clipPath>
    out.closeTag();  // </defs>
  }

  // For Outside stroke, wrap in a group with inverse clip (clip the fill area out).
  // For Inside stroke, wrap in a group with clip-path (clip to shape interior).
  if (needStrokeClip) {
    out.openTag("g");
    out.addAttr("clip-path", "url(#" + strokeClipId + ")");
    out.closeTagStart();
  }

  // Compute TrimPath parameters
  bool isContinuousTrim = trim && trim->type == TrimType::Continuous;
  std::vector<float> pathLengths = {};
  float totalPathLength = 0.0f;
  if (isContinuousTrim) {
    // Compute path lengths for all geometries
    for (auto& g : geos) {
      float len = computeGeoPathLength(g);
      pathLengths.push_back(len);
      totalPathLength += len;
    }
  }

  for (size_t geoIdx = 0; geoIdx < geos.size(); geoIdx++) {
    auto& g = geos[geoIdx];
    // Use modifiedPathData if available (set by RoundCorner modifier)
    if (!g.modifiedPathData.empty()) {
      out.openTag("path");
      out.addAttr("d", g.modifiedPathData);
      applySVGFill(out, trim ? nullptr : fill);
      if (isContinuousTrim) {
        applySVGStroke(out, stroke, computeGeoPathLength(g));
        applyTrimAttrsContinuous(out, trim, pathLengths, totalPathLength, geoIdx);
      } else {
        applySVGStroke(out, stroke);
        applyTrimAttrs(out, trim);
      }
      out.closeTagSelfClosing();
      continue;
    }
    switch (g.type) {
      case NodeType::Rectangle: {
        auto r = static_cast<const Rectangle*>(g.element);
        float x = r->position.x - r->size.width / 2;
        float y = r->position.y - r->size.height / 2;
        out.openTag("rect");
        if (!FloatNearlyZero(x)) {
          out.addAttr("x", FloatToString(x));
        }
        if (!FloatNearlyZero(y)) {
          out.addAttr("y", FloatToString(y));
        }
        out.addAttr("width", FloatToString(r->size.width));
        out.addAttr("height", FloatToString(r->size.height));
        if (r->roundness > 0) {
          out.addAttr("rx", FloatToString(r->roundness));
          out.addAttr("ry", FloatToString(r->roundness));
        }
        applySVGFill(out, trim ? nullptr : fill);
        if (isContinuousTrim) {
          applySVGStroke(out, stroke, computeGeoPathLength(g));
          applyTrimAttrsContinuous(out, trim, pathLengths, totalPathLength, geoIdx);
        } else {
          applySVGStroke(out, stroke);
          applyTrimAttrs(out, trim);
        }
        out.closeTagSelfClosing();
        break;
      }
      case NodeType::Ellipse: {
        auto e = static_cast<const Ellipse*>(g.element);
        out.openTag("ellipse");
        out.addAttr("cx", FloatToString(e->position.x));
        out.addAttr("cy", FloatToString(e->position.y));
        out.addAttr("rx", FloatToString(e->size.width / 2));
        out.addAttr("ry", FloatToString(e->size.height / 2));
        applySVGFill(out, trim ? nullptr : fill);
        if (isContinuousTrim) {
          applySVGStroke(out, stroke, computeGeoPathLength(g));
          applyTrimAttrsContinuous(out, trim, pathLengths, totalPathLength, geoIdx, true);
        } else {
          applySVGStroke(out, stroke);
          applyTrimAttrs(out, trim, true);
        }
        out.closeTagSelfClosing();
        break;
      }
      case NodeType::Path: {
        auto p = static_cast<const Path*>(g.element);
        std::string d = GetPathSVGString(p);
        if (!d.empty()) {
          out.openTag("path");
          out.addAttr("d", d);
          applySVGFill(out, trim ? nullptr : fill);
          if (isContinuousTrim) {
            applySVGStroke(out, stroke, computeGeoPathLength(g));
            applyTrimAttrsContinuous(out, trim, pathLengths, totalPathLength, geoIdx);
          } else {
            applySVGStroke(out, stroke);
            applyTrimAttrs(out, trim);
          }
          out.closeTagSelfClosing();
        }
        break;
      }
      case NodeType::Polystar: {
        auto ps = static_cast<const Polystar*>(g.element);
        std::string d = BuildPolystarPath(ps);
        if (!d.empty()) {
          out.openTag("path");
          out.addAttr("d", d);
          applySVGFill(out, trim ? nullptr : fill);
          if (isContinuousTrim) {
            applySVGStroke(out, stroke, computeGeoPathLength(g));
            applyTrimAttrsContinuous(out, trim, pathLengths, totalPathLength, geoIdx);
          } else {
            applySVGStroke(out, stroke);
            applyTrimAttrs(out, trim);
          }
          out.closeTagSelfClosing();
        }
        break;
      }
      default:
        break;
    }
  }
  if (needStrokeClip) {
    out.closeTag();  // </g> stroke clip group
  }
  out.closeTag();  // </svg>
}

void HTMLWriter::renderGeo(HTMLBuilder& out, const std::vector<GeoInfo>& geos, const Fill* fill,
                           const Stroke* stroke, float alpha, bool hasTrim, const TrimPath* trim,
                           bool hasMerge, MergePathMode mergeMode) {
  if (geos.empty()) {
    return;
  }
  // Determine the painter blend mode (fill takes precedence, then stroke)
  BlendMode painterBlend = BlendMode::Normal;
  if (fill && fill->blendMode != BlendMode::Normal) {
    painterBlend = fill->blendMode;
  } else if (stroke && stroke->blendMode != BlendMode::Normal) {
    painterBlend = stroke->blendMode;
  }
  if (hasMerge) {
    // Handle different MergePath modes
    std::string mergedPath;
    std::string fillRule = "nonzero";
    bool needClipPath = (mergeMode == MergePathMode::Intersect);

    if (needClipPath) {
      // Intersect mode: use nested clipPaths
      // The first path is the base shape, subsequent paths are used as clip-paths
      float pad = 0.0f;
      if (stroke) {
        pad = (stroke->align != StrokeAlign::Center) ? stroke->width * 2.0f : stroke->width;
      }
      float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
      for (auto& g : geos) {
        float gx = 0, gy = 0, gw = 0, gh = 0;
        switch (g.type) {
          case NodeType::Rectangle: {
            auto r = static_cast<const Rectangle*>(g.element);
            gx = r->position.x - r->size.width / 2;
            gy = r->position.y - r->size.height / 2;
            gw = r->size.width;
            gh = r->size.height;
            break;
          }
          case NodeType::Ellipse: {
            auto e = static_cast<const Ellipse*>(g.element);
            gx = e->position.x - e->size.width / 2;
            gy = e->position.y - e->size.height / 2;
            gw = e->size.width;
            gh = e->size.height;
            break;
          }
          case NodeType::Path: {
            auto p = static_cast<const Path*>(g.element);
            if (p->data && !p->data->isEmpty()) {
              Rect b = p->data->getBounds();
              gx = b.x;
              gy = b.y;
              gw = b.width;
              gh = b.height;
            }
            break;
          }
          case NodeType::Polystar: {
            auto ps = static_cast<const Polystar*>(g.element);
            float r = std::max(ps->outerRadius, ps->innerRadius);
            gx = ps->position.x - r;
            gy = ps->position.y - r;
            gw = r * 2;
            gh = r * 2;
            break;
          }
          default:
            break;
        }
        x0 = std::min(x0, gx - pad);
        y0 = std::min(y0, gy - pad);
        x1 = std::max(x1, gx + gw + pad);
        y1 = std::max(y1, gy + gh + pad);
      }
      float sw = x1 - x0;
      float sh = y1 - y0;
      if (sw <= 0 || sh <= 0) {
        return;
      }
      std::string svgStyle =
          "position:absolute;left:" + FloatToString(x0) + "px;top:" + FloatToString(y0) + "px";
      if (painterBlend != BlendMode::Normal) {
        auto blendStr = BlendModeToMixBlendMode(painterBlend);
        if (blendStr) {
          svgStyle += ";mix-blend-mode:";
          svgStyle += blendStr;
        }
      }
      if (alpha < 1.0f) {
        svgStyle += ";opacity:" + FloatToString(alpha);
      }
      out.openTag("svg");
      out.addAttr("xmlns", "http://www.w3.org/2000/svg");
      out.addAttr("width", FloatToString(sw));
      out.addAttr("height", FloatToString(sh));
      out.addAttr("viewBox", FloatToString(x0) + " " + FloatToString(y0) + " " + FloatToString(sw) +
                                 " " + FloatToString(sh));
      out.addAttr("style", svgStyle);
      out.closeTagStart();

      // Create clip-paths for each subsequent geometry, storing IDs for later reference
      std::vector<std::string> clipIds = {};
      if (geos.size() > 1) {
        out.openTag("defs");
        out.closeTagStart();
        for (size_t i = 1; i < geos.size(); i++) {
          std::string clipId = _ctx->nextId("mpclip");
          clipIds.push_back(clipId);
          out.openTag("clipPath");
          out.addAttr("id", clipId);
          out.closeTagStart();
          auto& g = geos[i];
          std::string pathD;
          if (!g.modifiedPathData.empty()) {
            pathD = g.modifiedPathData;
          } else {
            switch (g.type) {
              case NodeType::Rectangle:
                pathD = RectToPathData(static_cast<const Rectangle*>(g.element));
                break;
              case NodeType::Ellipse:
                pathD = EllipseToPathData(static_cast<const Ellipse*>(g.element));
                break;
              case NodeType::Path:
                pathD = GetPathSVGString(static_cast<const Path*>(g.element));
                break;
              case NodeType::Polystar:
                pathD = BuildPolystarPath(static_cast<const Polystar*>(g.element));
                break;
              default:
                break;
            }
          }
          if (!pathD.empty()) {
            out.openTag("path");
            out.addAttr("d", pathD);
            out.closeTagSelfClosing();
          }
          out.closeTag();  // </clipPath>
        }
        out.closeTag();  // </defs>
      }

      // Render the first path with nested clip-paths
      auto& firstGeo = geos[0];
      std::string firstPathD;
      if (!firstGeo.modifiedPathData.empty()) {
        firstPathD = firstGeo.modifiedPathData;
      } else {
        switch (firstGeo.type) {
          case NodeType::Rectangle:
            firstPathD = RectToPathData(static_cast<const Rectangle*>(firstGeo.element));
            break;
          case NodeType::Ellipse:
            firstPathD = EllipseToPathData(static_cast<const Ellipse*>(firstGeo.element));
            break;
          case NodeType::Path:
            firstPathD = GetPathSVGString(static_cast<const Path*>(firstGeo.element));
            break;
          case NodeType::Polystar:
            firstPathD = BuildPolystarPath(static_cast<const Polystar*>(firstGeo.element));
            break;
          default:
            break;
        }
      }
      if (!firstPathD.empty()) {
        // Open groups with clip-paths (in reverse order for proper nesting)
        for (size_t i = clipIds.size(); i >= 1; i--) {
          out.openTag("g");
          out.addAttr("clip-path", "url(#" + clipIds[i - 1] + ")");
          out.closeTagStart();
        }
        out.openTag("path");
        out.addAttr("d", firstPathD);
        applySVGFill(out, fill);
        applySVGStroke(out, stroke);
        out.closeTagSelfClosing();
        // Close all the clip groups
        for (size_t i = 1; i < geos.size(); i++) {
          out.closeTag();
        }
      }
      out.closeTag();  // </svg>
      return;
    }

    // Handle Union, Xor, Difference, and Append modes
    for (size_t i = 0; i < geos.size(); i++) {
      auto& g = geos[i];
      PathData pathData = PathDataFromSVGString("");
      if (!g.modifiedPathData.empty()) {
        pathData = PathDataFromSVGString(g.modifiedPathData);
      } else {
        pathData = GeoToPathData(g.element, g.type);
      }
      bool needReverse = false;
      switch (mergeMode) {
        case MergePathMode::Union:
          // All paths should be clockwise for union with nonzero fill-rule
          if (!IsPathClockwise(pathData)) {
            needReverse = true;
          }
          fillRule = "nonzero";
          break;
        case MergePathMode::Xor:
          // Just concatenate paths, evenodd rule produces XOR effect
          fillRule = "evenodd";
          break;
        case MergePathMode::Difference:
          // First path clockwise, rest counter-clockwise
          if (i == 0) {
            if (!IsPathClockwise(pathData)) {
              needReverse = true;
            }
          } else {
            if (IsPathClockwise(pathData)) {
              needReverse = true;
            }
          }
          fillRule = "nonzero";
          break;
        case MergePathMode::Append:
        default:
          // Just concatenate, no direction change
          break;
      }
      if (needReverse) {
        pathData = ReversePathData(pathData);
      }
      mergedPath += PathDataToSVGString(pathData);
    }
    if (mergedPath.empty()) {
      return;
    }
    // Compute bounding box
    float pad = 0.0f;
    if (stroke) {
      pad = (stroke->align != StrokeAlign::Center) ? stroke->width * 2.0f : stroke->width;
    }
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (auto& g : geos) {
      float gx = 0, gy = 0, gw = 0, gh = 0;
      switch (g.type) {
        case NodeType::Rectangle: {
          auto r = static_cast<const Rectangle*>(g.element);
          gx = r->position.x - r->size.width / 2;
          gy = r->position.y - r->size.height / 2;
          gw = r->size.width;
          gh = r->size.height;
          break;
        }
        case NodeType::Ellipse: {
          auto e = static_cast<const Ellipse*>(g.element);
          gx = e->position.x - e->size.width / 2;
          gy = e->position.y - e->size.height / 2;
          gw = e->size.width;
          gh = e->size.height;
          break;
        }
        case NodeType::Path: {
          auto p = static_cast<const Path*>(g.element);
          if (p->data && !p->data->isEmpty()) {
            Rect b = p->data->getBounds();
            gx = b.x;
            gy = b.y;
            gw = b.width;
            gh = b.height;
          }
          break;
        }
        case NodeType::Polystar: {
          auto ps = static_cast<const Polystar*>(g.element);
          float r = std::max(ps->outerRadius, ps->innerRadius);
          gx = ps->position.x - r;
          gy = ps->position.y - r;
          gw = r * 2;
          gh = r * 2;
          break;
        }
        default:
          break;
      }
      x0 = std::min(x0, gx - pad);
      y0 = std::min(y0, gy - pad);
      x1 = std::max(x1, gx + gw + pad);
      y1 = std::max(y1, gy + gh + pad);
    }
    float sw = x1 - x0;
    float sh = y1 - y0;
    if (sw <= 0 || sh <= 0) {
      return;
    }
    std::string svgStyle =
        "position:absolute;left:" + FloatToString(x0) + "px;top:" + FloatToString(y0) + "px";
    if (painterBlend != BlendMode::Normal) {
      auto blendStr = BlendModeToMixBlendMode(painterBlend);
      if (blendStr) {
        svgStyle += ";mix-blend-mode:";
        svgStyle += blendStr;
      }
    }
    if (alpha < 1.0f) {
      svgStyle += ";opacity:" + FloatToString(alpha);
    }
    out.openTag("svg");
    out.addAttr("xmlns", "http://www.w3.org/2000/svg");
    out.addAttr("width", FloatToString(sw));
    out.addAttr("height", FloatToString(sh));
    out.addAttr("viewBox", FloatToString(x0) + " " + FloatToString(y0) + " " + FloatToString(sw) +
                               " " + FloatToString(sh));
    out.addAttr("style", svgStyle);
    out.closeTagStart();
    out.openTag("path");
    out.addAttr("d", mergedPath);
    if (fillRule != "nonzero") {
      out.addAttr("fill-rule", fillRule);
    }
    applySVGFill(out, fill);
    applySVGStroke(out, stroke);
    out.closeTagSelfClosing();
    out.closeTag();
    return;
  }
  if (canCSS(geos, fill, stroke, hasTrim, false)) {
    renderCSSDiv(out, geos[0], fill, alpha, painterBlend);
  } else {
    renderSVG(out, geos, fill, stroke, alpha, painterBlend, trim, mergeMode);
  }
}

//==============================================================================
// HTMLWriter – text
//==============================================================================

void HTMLWriter::writeText(HTMLBuilder& out, const Text* text, const Fill* fill,
                           const Stroke* stroke, const TextBox* tb, float alpha) {
  if (!text->glyphRuns.empty()) {
    writeGlyphRunSVG(out, text, fill, stroke, alpha);
    return;
  }
  if (text->text.empty()) {
    return;
  }
  std::string style;
  style.reserve(300);
  if (tb) {
    style += "position:absolute;left:" + FloatToString(tb->position.x) +
             "px;top:" + FloatToString(tb->position.y) + "px";
    if (tb->size.width > 0) {
      style += ";width:" + FloatToString(tb->size.width) + "px";
    }
    if (tb->size.height > 0) {
      style += ";height:" + FloatToString(tb->size.height) + "px";
    }
    if (tb->textAlign == TextAlign::Center) {
      style += ";text-align:center";
    } else if (tb->textAlign == TextAlign::End) {
      style += ";text-align:end";
    } else if (tb->textAlign == TextAlign::Justify) {
      style += ";text-align:justify";
    }
    if (tb->paragraphAlign != ParagraphAlign::Near) {
      style += ";display:flex;flex-direction:column";
      if (tb->paragraphAlign == ParagraphAlign::Middle) {
        style += ";justify-content:center";
      } else if (tb->paragraphAlign == ParagraphAlign::Far) {
        style += ";justify-content:flex-end";
      }
    }
    if (tb->writingMode == WritingMode::Vertical) {
      style += ";writing-mode:vertical-rl";
    }
    if (tb->lineHeight > 0) {
      style += ";line-height:" + FloatToString(tb->lineHeight) + "px";
    }
    if (tb->wordWrap) {
      style += ";word-wrap:break-word";
    } else {
      style += ";white-space:nowrap";
    }
    if (tb->overflow == Overflow::Hidden) {
      style += ";overflow:hidden";
    }
  } else {
    float ty = text->position.y - text->fontSize * 0.8f;
    style += "position:absolute;left:" + FloatToString(text->position.x) +
             "px;top:" + FloatToString(ty) + "px;white-space:pre";
  }
  // Build a combined transform string for textAnchor and fauxItalic
  std::string textTransform;
  if (!tb) {
    if (text->textAnchor == TextAnchor::Center) {
      textTransform += "translateX(-50%)";
    } else if (text->textAnchor == TextAnchor::End) {
      textTransform += "translateX(-100%)";
    }
  }
  if (text->fauxItalic) {
    if (!textTransform.empty()) {
      textTransform += ' ';
    }
    textTransform += "skewX(-12deg)";
  }
  if (!textTransform.empty()) {
    style += ";transform:" + textTransform;
  }
  if (!text->fontFamily.empty()) {
    std::string escapedFamily = text->fontFamily;
    for (size_t pos = 0; (pos = escapedFamily.find('\'', pos)) != std::string::npos; pos += 2) {
      escapedFamily.replace(pos, 1, "\\'");
    }
    style += ";font-family:'" + escapedFamily + "'";
  }
  style += ";font-size:" + FloatToString(text->fontSize) + "px";
  if (text->letterSpacing != 0.0f) {
    style += ";letter-spacing:" + FloatToString(text->letterSpacing) + "px";
  }
  if (!text->fontStyle.empty()) {
    if (text->fontStyle.find("Bold") != std::string::npos) {
      style += ";font-weight:bold";
    }
    if (text->fontStyle.find("Italic") != std::string::npos) {
      style += ";font-style:italic";
    }
  }
  if (text->fauxBold && !stroke) {
    style += ";-webkit-text-stroke:0.02em currentColor";
  }
  if (fill && fill->color) {
    auto ct = fill->color->nodeType();
    if (ct == NodeType::SolidColor) {
      auto sc = static_cast<const SolidColor*>(fill->color);
      style += ";color:" + ColorToRGBA(sc->color, fill->alpha);
    } else {
      float ca = 1.0f;
      std::string css = colorToCSS(fill->color, &ca);
      if (!css.empty()) {
        style += ";background:" + css;
        style += ";-webkit-background-clip:text;background-clip:text";
        style += ";-webkit-text-fill-color:transparent";
        if (fill->alpha < 1.0f) {
          alpha *= fill->alpha;
        }
      }
    }
  }
  if (stroke && stroke->color && stroke->color->nodeType() == NodeType::SolidColor) {
    auto sc = static_cast<const SolidColor*>(stroke->color);
    style += ";-webkit-text-stroke:" + FloatToString(stroke->width) + "px " +
             ColorToRGBA(sc->color, stroke->alpha);
    style += ";paint-order:stroke fill";
  }
  if (alpha < 1.0f) {
    style += ";opacity:" + FloatToString(alpha);
  }
  out.openTag("span");
  out.addAttr("style", style);
  out.closeTagWithText(text->text);
}

void HTMLWriter::writeGlyphRunSVG(HTMLBuilder& out, const Text* text, const Fill* fill,
                                  const Stroke* stroke, float alpha) {
  auto paths = ComputeGlyphPaths(*text, text->position.x, text->position.y);
  // Check for bitmap glyphs that ComputeGlyphPaths skips
  bool hasBitmapGlyphs = false;
  for (auto* run : text->glyphRuns) {
    if (!run->font) {
      continue;
    }
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
      if (glyph && glyph->image) {
        hasBitmapGlyphs = true;
        break;
      }
    }
    if (hasBitmapGlyphs) {
      break;
    }
  }
  if (paths.empty() && !hasBitmapGlyphs) {
    return;
  }
  std::string svgStyle = "position:absolute;left:0;top:0;overflow:visible";
  if (alpha < 1.0f) {
    svgStyle += ";opacity:" + FloatToString(alpha);
  }
  out.openTag("svg");
  out.addAttr("xmlns", "http://www.w3.org/2000/svg");
  out.addAttr("style", svgStyle);
  out.closeTagStart();
  if (!paths.empty()) {
    out.openTag("g");
    applySVGFill(out, fill);
    applySVGStroke(out, stroke);
    out.closeTagStart();
    for (auto& gp : paths) {
      out.openTag("path");
      out.addAttr("transform", MatrixToCSS(gp.transform));
      out.addAttr("d", PathDataToSVGString(*gp.pathData));
      out.closeTagSelfClosing();
    }
    out.closeTag();  // </g>
  }
  // Render bitmap glyphs as <image> elements
  if (hasBitmapGlyphs) {
    float textPosX = text->position.x;
    float textPosY = text->position.y;
    for (auto* run : text->glyphRuns) {
      if (!run->font) {
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
        if (!glyph) {
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
        if (!glyph->image) {
          continue;
        }
        // Bitmap glyph: render as <image> with transform
        float ox = glyph->offset.x;
        float oy = glyph->offset.y;
        Matrix glyphMatrix =
            Matrix::Translate(posX + ox * scale, posY + oy * scale) * Matrix::Scale(scale, scale);
        std::string src = GetImageSrc(glyph->image);
        out.openTag("image");
        out.addAttr("href", src);
        out.addAttr("transform", MatrixToCSS(glyphMatrix));
        out.closeTagSelfClosing();
      }
    }
  }
  out.closeTag();  // </svg>
}

void HTMLWriter::writeTextModifier(HTMLBuilder& out, const std::vector<GeoInfo>& geos,
                                   const TextModifier* modifier, const Fill* fill,
                                   const Stroke* stroke, const TextBox* /*tb*/, float alpha) {
  // Find Text elements in geos
  for (auto& g : geos) {
    if (g.type != NodeType::Text) {
      continue;
    }
    auto text = static_cast<const Text*>(g.element);
    // Compute factor for each glyph using the TextModifier's selectors
    size_t totalGlyphs = 0;
    if (!text->glyphRuns.empty()) {
      for (auto* run : text->glyphRuns) {
        totalGlyphs += run->glyphs.size();
      }
    } else {
      // Runtime text: count UTF-8 characters
      const char* p = text->text.c_str();
      while (*p) {
        int32_t ch = 0;
        size_t len = SVGDecodeUTF8Char(p, text->text.size() - (p - text->text.c_str()), &ch);
        if (len == 0) {
          break;
        }
        totalGlyphs++;
        p += len;
      }
    }
    if (totalGlyphs == 0) {
      continue;
    }
    // Compute factors for all glyphs
    std::vector<float> factors(totalGlyphs, 0.0f);
    for (size_t i = 0; i < totalGlyphs; i++) {
      float combinedFactor = 0.0f;
      bool firstSelector = true;
      for (auto* selector : modifier->selectors) {
        if (selector->nodeType() == NodeType::RangeSelector) {
          auto rs = static_cast<const RangeSelector*>(selector);
          float selectorFactor = ComputeRangeSelectorFactor(rs, i, totalGlyphs);
          if (firstSelector) {
            combinedFactor = selectorFactor;
            firstSelector = false;
          } else {
            combinedFactor = CombineSelectorValues(rs->mode, combinedFactor, selectorFactor);
          }
        }
      }
      factors[i] = std::clamp(combinedFactor, -1.0f, 1.0f);
    }

    if (!text->glyphRuns.empty()) {
      // GlyphRun: render each glyph with modified transforms
      std::string svgStyle = "position:absolute;left:0;top:0;overflow:visible";
      if (alpha < 1.0f) {
        svgStyle += ";opacity:" + FloatToString(alpha);
      }
      out.openTag("svg");
      out.addAttr("xmlns", "http://www.w3.org/2000/svg");
      out.addAttr("style", svgStyle);
      out.closeTagStart();
      size_t glyphIdx = 0;
      for (auto* run : text->glyphRuns) {
        if (!run->font) {
          continue;
        }
        float scale = run->fontSize / run->font->unitsPerEm;
        for (size_t i = 0; i < run->glyphs.size(); i++) {
          uint16_t glyphId = run->glyphs[i];
          if (glyphId == 0) {
            glyphIdx++;
            continue;
          }
          auto* glyph = (glyphId > 0 && glyphId <= run->font->glyphs.size())
                            ? run->font->glyphs[glyphId - 1]
                            : nullptr;
          if (!glyph || !glyph->path) {
            glyphIdx++;
            continue;
          }
          float f = (glyphIdx < factors.size()) ? factors[glyphIdx] : 0.0f;
          float absF = std::abs(f);
          // Compute glyph position
          float gx = run->x;
          float gy = run->y;
          if (i < run->xOffsets.size()) {
            gx += run->xOffsets[i];
          }
          if (i < run->positions.size()) {
            gx += run->positions[i].x;
            gy += run->positions[i].y;
          }
          // Default anchor is (advance * 0.5, 0)
          float anchorX = glyph->advance * 0.5f * scale;
          float anchorY = 0;
          if (i < run->anchors.size()) {
            anchorX += run->anchors[i].x * scale;
            anchorY += run->anchors[i].y * scale;
          }
          // Build transform with modifier applied
          // Order: translate(-anchor*f) → scale(1+(s-1)*f) → skew → rotate(rot*f) → translate(anchor*f) → translate(pos*f)
          Matrix m = Matrix::Scale(scale, scale);
          // Apply modifier transforms scaled by factor
          // 1. Translate to anchor
          float modAnchorX = anchorX + modifier->anchor.x * absF;
          float modAnchorY = anchorY + modifier->anchor.y * absF;
          m = Matrix::Translate(-modAnchorX, -modAnchorY) * m;
          // 2. Scale
          float sx = 1.0f + (modifier->scale.x - 1.0f) * absF;
          float sy = 1.0f + (modifier->scale.y - 1.0f) * absF;
          m = Matrix::Scale(sx, sy) * m;
          // 3. Skew
          if (!FloatNearlyZero(modifier->skew)) {
            m = Matrix::Rotate(modifier->skewAxis * absF) * m;
            Matrix shear = {};
            shear.c = std::tan(DegreesToRadians(modifier->skew * absF));
            m = shear * m;
            m = Matrix::Rotate(-modifier->skewAxis * absF) * m;
          }
          // 4. Rotate
          if (!FloatNearlyZero(modifier->rotation)) {
            m = Matrix::Rotate(modifier->rotation * f) * m;
          }
          // 5. Translate back from anchor
          m = Matrix::Translate(modAnchorX, modAnchorY) * m;
          // 6. Position offset
          m = Matrix::Translate(modifier->position.x * f, modifier->position.y * f) * m;
          // 7. Glyph position
          m = Matrix::Translate(gx, gy) * m;
          // Compute alpha
          float glyphAlpha = 1.0f;
          if (modifier->alpha < 1.0f) {
            glyphAlpha = std::max(0.0f, 1.0f + (modifier->alpha - 1.0f) * absF);
          }
          out.openTag("g");
          out.addAttr("transform", MatrixToCSS(m));
          if (glyphAlpha < 1.0f) {
            out.addAttr("opacity", FloatToString(glyphAlpha));
          }
          out.closeTagStart();
          out.openTag("path");
          out.addAttr("d", PathDataToSVGString(*glyph->path));
          // Apply fill/stroke with potential color override
          if (fill) {
            if (modifier->fillColor.has_value() && absF > 0.0f && fill->color &&
                fill->color->nodeType() == NodeType::SolidColor) {
              auto baseSC = static_cast<const SolidColor*>(fill->color);
              Color blended = LerpColor(baseSC->color, modifier->fillColor.value(), absF);
              out.addAttr("fill", ColorToSVGHex(blended));
              float fa = blended.alpha * fill->alpha;
              if (fa < 1.0f) {
                out.addAttr("fill-opacity", FloatToString(fa));
              }
            } else {
              applySVGFill(out, fill);
            }
          } else {
            out.addAttr("fill", "none");
          }
          if (stroke) {
            applySVGStroke(out, stroke);
            // Apply strokeColor override
            if (modifier->strokeColor.has_value() && absF > 0) {
              Color sc = modifier->strokeColor.value();
              float blendFactor = sc.alpha * absF;
              if (blendFactor > 0) {
                out.addAttr("stroke", ColorToSVGHex(sc));
                out.addAttr("stroke-opacity", FloatToString(blendFactor));
              }
            }
            // Apply strokeWidth override
            if (modifier->strokeWidth.has_value() && absF > 0) {
              float origWidth = stroke->width;
              float modWidth = modifier->strokeWidth.value();
              float finalWidth = origWidth + (modWidth - origWidth) * absF;
              out.addAttr("stroke-width", FloatToString(finalWidth));
            }
          }
          out.closeTagSelfClosing();
          out.closeTag();  // </g>
          glyphIdx++;
        }
      }
      out.closeTag();  // </svg>
    } else {
      // Runtime text: split into per-character spans
      float ty = text->position.y - text->fontSize * 0.8f;
      std::string containerStyle =
          "position:absolute;white-space:nowrap;left:" + FloatToString(text->position.x) +
          "px;top:" + FloatToString(ty) + "px";
      // Apply textAnchor
      if (text->textAnchor == TextAnchor::Center) {
        containerStyle += ";transform:translateX(-50%)";
      } else if (text->textAnchor == TextAnchor::End) {
        containerStyle += ";transform:translateX(-100%)";
      }
      if (!text->fontFamily.empty()) {
        std::string escapedFamilyM = text->fontFamily;
        for (size_t p = 0; p < escapedFamilyM.size(); p++) {
          if (escapedFamilyM[p] == '\'') {
            escapedFamilyM.replace(p, 1, "\\'");
            p++;
          }
        }
        containerStyle += ";font-family:'" + escapedFamilyM + "'";
      }
      containerStyle += ";font-size:" + FloatToString(text->fontSize) + "px";
      if (alpha < 1.0f) {
        containerStyle += ";opacity:" + FloatToString(alpha);
      }
      out.openTag("div");
      out.addAttr("style", containerStyle);
      out.closeTagStart();
      const char* p = text->text.c_str();
      size_t charIdx = 0;
      while (*p) {
        int32_t ch = 0;
        size_t len = SVGDecodeUTF8Char(p, text->text.size() - (p - text->text.c_str()), &ch);
        if (len == 0) {
          break;
        }
        float f = (charIdx < factors.size()) ? factors[charIdx] : 0.0f;
        float absF = std::abs(f);
        std::string charStr(p, len);
        std::string charStyle = "display:inline-block";
        // Build transform (CSS reads right-to-left):
        // translate(position*f) translate(anchor*f) rotate(rot*f) skew scale translate(-anchor*f)
        std::string transform;
        // 6. Position offset
        if (!FloatNearlyZero(modifier->position.x * f) ||
            !FloatNearlyZero(modifier->position.y * f)) {
          transform += "translate(" + FloatToString(modifier->position.x * f) + "px," +
                       FloatToString(modifier->position.y * f) + "px) ";
        }
        // 5. Translate back to anchor
        float anchorX = modifier->anchor.x * f;
        float anchorY = modifier->anchor.y * f;
        if (!FloatNearlyZero(anchorX) || !FloatNearlyZero(anchorY)) {
          transform +=
              "translate(" + FloatToString(anchorX) + "px," + FloatToString(anchorY) + "px) ";
        }
        // 4. Rotation
        if (!FloatNearlyZero(modifier->rotation * f)) {
          transform += "rotate(" + FloatToString(modifier->rotation * f) + "deg) ";
        }
        // 2. Scale
        float sx = 1.0f + (modifier->scale.x - 1.0f) * absF;
        float sy = 1.0f + (modifier->scale.y - 1.0f) * absF;
        if (!FloatNearlyZero(sx - 1.0f) || !FloatNearlyZero(sy - 1.0f)) {
          transform += "scale(" + FloatToString(sx) + "," + FloatToString(sy) + ") ";
        }
        // 3. Skew
        if (!FloatNearlyZero(modifier->skew * absF)) {
          transform += "skewX(" + FloatToString(modifier->skew * absF) + "deg) ";
        }
        // 1. Translate to anchor origin
        if (!FloatNearlyZero(anchorX) || !FloatNearlyZero(anchorY)) {
          transform +=
              "translate(" + FloatToString(-anchorX) + "px," + FloatToString(-anchorY) + "px) ";
        }
        if (!transform.empty()) {
          charStyle += ";transform:" + transform;
        }
        // Alpha
        if (modifier->alpha < 1.0f) {
          float charAlpha = std::max(0.0f, 1.0f + (modifier->alpha - 1.0f) * absF);
          charStyle += ";opacity:" + FloatToString(charAlpha);
        }
        // Color
        if (fill && fill->color && fill->color->nodeType() == NodeType::SolidColor) {
          auto sc = static_cast<const SolidColor*>(fill->color);
          if (modifier->fillColor.has_value() && absF > 0.0f) {
            Color blended = LerpColor(sc->color, modifier->fillColor.value(), absF);
            charStyle += ";color:" + ColorToRGBA(blended, fill->alpha);
          } else {
            charStyle += ";color:" + ColorToRGBA(sc->color, fill->alpha);
          }
        }
        // Stroke
        if (stroke) {
          Color strokeColor = {};
          float strokeWidth = stroke->width;
          bool hasStroke = false;
          if (modifier->strokeColor.has_value() && absF > 0.0f) {
            strokeColor = modifier->strokeColor.value();
            strokeColor.alpha *= absF;
            hasStroke = true;
          } else if (stroke->color && stroke->color->nodeType() == NodeType::SolidColor) {
            strokeColor = static_cast<const SolidColor*>(stroke->color)->color;
            hasStroke = true;
          }
          if (modifier->strokeWidth.has_value() && absF > 0.0f) {
            strokeWidth = stroke->width + (modifier->strokeWidth.value() - stroke->width) * absF;
          }
          if (hasStroke && strokeWidth > 0 && strokeColor.alpha > 0) {
            charStyle += ";-webkit-text-stroke:" + FloatToString(strokeWidth) + "px " +
                         ColorToRGBA(strokeColor, stroke->alpha);
            charStyle += ";paint-order:stroke fill";
          }
        }
        out.openTag("span");
        out.addAttr("style", charStyle);
        out.closeTagWithText(charStr);
        p += len;
        charIdx++;
      }
      out.closeTag();  // </div>
    }
  }
}

void HTMLWriter::writeTextPath(HTMLBuilder& out, const std::vector<GeoInfo>& geos,
                               const TextPath* textPath, const Fill* fill, const Stroke* stroke,
                               const TextBox*, float alpha) {
  if (!textPath->path || textPath->path->isEmpty()) {
    return;
  }
  // Build arc-length LUT
  PathData pathData = *textPath->path;
  if (textPath->reversed) {
    pathData = ReversePathData(pathData);
  }
  ArcLengthLUT lut = BuildArcLengthLUT(pathData);
  if (lut.totalLength <= 0) {
    return;
  }
  float effectiveLength = lut.totalLength - textPath->firstMargin - textPath->lastMargin;
  if (effectiveLength <= 0) {
    return;
  }

  for (auto& g : geos) {
    if (g.type != NodeType::Text) {
      continue;
    }
    auto text = static_cast<const Text*>(g.element);
    if (!text->glyphRuns.empty()) {
      // GlyphRun: position each glyph along the path
      std::string svgStyle = "position:absolute;left:0;top:0;overflow:visible";
      if (alpha < 1.0f) {
        svgStyle += ";opacity:" + FloatToString(alpha);
      }
      out.openTag("svg");
      out.addAttr("xmlns", "http://www.w3.org/2000/svg");
      out.addAttr("style", svgStyle);
      out.closeTagStart();
      out.openTag("g");
      applySVGFill(out, fill);
      applySVGStroke(out, stroke);
      out.closeTagStart();
      // Compute total text width for forceAlignment
      float totalAdvance = 0.0f;
      for (auto* run : text->glyphRuns) {
        if (!run->font) {
          continue;
        }
        float scale = run->fontSize / run->font->unitsPerEm;
        for (size_t i = 0; i < run->glyphs.size(); i++) {
          uint16_t glyphId = run->glyphs[i];
          if (glyphId == 0) {
            continue;
          }
          auto* glyph = (glyphId > 0 && glyphId <= run->font->glyphs.size())
                            ? run->font->glyphs[glyphId - 1]
                            : nullptr;
          if (glyph) {
            totalAdvance += glyph->advance * scale;
          }
        }
      }
      float spacingScale =
          textPath->forceAlignment && totalAdvance > 0 ? effectiveLength / totalAdvance : 1.0f;
      float currentArcPos = textPath->firstMargin;
      for (auto* run : text->glyphRuns) {
        if (!run->font) {
          continue;
        }
        float scale = run->fontSize / run->font->unitsPerEm;
        for (size_t i = 0; i < run->glyphs.size(); i++) {
          uint16_t glyphId = run->glyphs[i];
          if (glyphId == 0) {
            continue;
          }
          auto* glyph = (glyphId > 0 && glyphId <= run->font->glyphs.size())
                            ? run->font->glyphs[glyphId - 1]
                            : nullptr;
          if (!glyph || !glyph->path) {
            continue;
          }
          float glyphAdvance = glyph->advance * scale * spacingScale;
          // Position at center of glyph
          float glyphCenterArc = currentArcPos + glyphAdvance / 2.0f;
          Point pos = {};
          float tangent = 0;
          SampleArcLengthLUT(lut, glyphCenterArc, &pos, &tangent);
          // Get original glyph y offset
          float yOffset = 0;
          if (i < run->positions.size()) {
            yOffset = run->positions[i].y;
          }
          // Apply perpendicular offset
          float normalAngle = tangent + static_cast<float>(M_PI) / 2.0f;
          pos.x += yOffset * std::cos(normalAngle);
          pos.y += yOffset * std::sin(normalAngle);
          // Build transform
          Matrix m = Matrix::Scale(scale, scale);
          // Center the glyph
          m = Matrix::Translate(-glyph->advance / 2.0f, 0) * m;
          // Rotate to align with path tangent
          if (textPath->perpendicular) {
            float angleDeg = tangent * 180.0f / static_cast<float>(M_PI);
            m = Matrix::Rotate(angleDeg) * m;
          }
          // Move to position on path
          m = Matrix::Translate(pos.x, pos.y) * m;
          out.openTag("path");
          out.addAttr("transform", MatrixToCSS(m));
          out.addAttr("d", PathDataToSVGString(*glyph->path));
          out.closeTagSelfClosing();
          currentArcPos += glyphAdvance;
        }
      }
      out.closeTag();  // </g>
      out.closeTag();  // </svg>
    } else {
      // Runtime text: position each character along the path
      // Estimate total text width
      float totalWidth = 0.0f;
      const char* p = text->text.c_str();
      while (*p) {
        int32_t ch = 0;
        size_t len = SVGDecodeUTF8Char(p, text->text.size() - (p - text->text.c_str()), &ch);
        if (len == 0) {
          break;
        }
        totalWidth += EstimateCharAdvance(ch, text->fontSize);
        p += len;
      }
      float spacingScale =
          textPath->forceAlignment && totalWidth > 0 ? effectiveLength / totalWidth : 1.0f;
      // Render each character
      float currentArcPos = textPath->firstMargin;
      p = text->text.c_str();
      while (*p) {
        int32_t ch = 0;
        size_t len = SVGDecodeUTF8Char(p, text->text.size() - (p - text->text.c_str()), &ch);
        if (len == 0) {
          break;
        }
        float charWidth = EstimateCharAdvance(ch, text->fontSize) * spacingScale;
        float charCenterArc = currentArcPos + charWidth / 2.0f;
        Point pos = {};
        float tangent = 0;
        SampleArcLengthLUT(lut, charCenterArc, &pos, &tangent);
        std::string charStr(p, len);
        std::string charStyle = "position:absolute;left:" + FloatToString(pos.x) +
                                "px;top:" + FloatToString(pos.y - text->fontSize * 0.8f) + "px";
        charStyle += ";display:inline-block";
        if (!text->fontFamily.empty()) {
          std::string escapedFamilyTP = text->fontFamily;
          for (size_t p = 0; p < escapedFamilyTP.size(); p++) {
            if (escapedFamilyTP[p] == '\'') {
              escapedFamilyTP.replace(p, 1, "\\'");
              p++;
            }
          }
          charStyle += ";font-family:'" + escapedFamilyTP + "'";
        }
        charStyle += ";font-size:" + FloatToString(text->fontSize) + "px";
        // Transform for rotation and centering
        std::string transform;
        transform += "translateX(-50%)";
        if (textPath->perpendicular) {
          float angleDeg = tangent * 180.0f / static_cast<float>(M_PI);
          transform += " rotate(" + FloatToString(angleDeg) + "deg)";
        }
        charStyle += ";transform:" + transform;
        charStyle += ";transform-origin:center center";
        if (fill && fill->color && fill->color->nodeType() == NodeType::SolidColor) {
          auto sc = static_cast<const SolidColor*>(fill->color);
          charStyle += ";color:" + ColorToRGBA(sc->color, fill->alpha);
        }
        if (alpha < 1.0f) {
          charStyle += ";opacity:" + FloatToString(alpha);
        }
        out.openTag("span");
        out.addAttr("style", charStyle);
        out.closeTagWithText(charStr);
        currentArcPos += charWidth;
        p += len;
      }
    }
  }
}

//==============================================================================
// HTMLWriter – group & repeater
//==============================================================================

void HTMLWriter::writeGroup(HTMLBuilder& out, const Group* group, float alpha, bool distribute) {
  Matrix gm = BuildGroupMatrix(group);
  std::string style = "position:relative";
  if (!gm.isIdentity()) {
    style += ";transform:" + MatrixToCSS(gm);
  }
  if (group->alpha < 1.0f) {
    float ea = distribute ? (group->alpha * alpha) : group->alpha;
    style += ";opacity:" + FloatToString(ea);
  }
  out.openTag("div");
  out.addAttr("class", "pagx-group");
  out.addAttr("style", style);
  out.closeTagStart();
  writeElements(out, group->elements, 1.0f, false, LayerPlacement::Background);
  // Only run the Foreground pass if any Fill/Stroke has foreground placement
  bool hasForegroundPainter = false;
  for (auto* e : group->elements) {
    if (e->nodeType() == NodeType::Fill) {
      if (static_cast<const Fill*>(e)->placement == LayerPlacement::Foreground) {
        hasForegroundPainter = true;
        break;
      }
    } else if (e->nodeType() == NodeType::Stroke) {
      if (static_cast<const Stroke*>(e)->placement == LayerPlacement::Foreground) {
        hasForegroundPainter = true;
        break;
      }
    }
  }
  if (hasForegroundPainter) {
    writeElements(out, group->elements, 1.0f, false, LayerPlacement::Foreground);
  }
  out.closeTag();
}

void HTMLWriter::writeRepeater(HTMLBuilder& out, const Repeater* rep,
                               const std::vector<GeoInfo>& geos, const Fill* fill,
                               const Stroke* stroke, float alpha) {
  if (rep->copies <= 0) {
    return;
  }
  int n = static_cast<int>(std::ceil(rep->copies));
  float frac = rep->copies - std::floor(rep->copies);
  for (int i = 0; i < n; i++) {
    int idx = (rep->order == RepeaterOrder::AboveOriginal) ? i : (n - 1 - i);
    float prog = static_cast<float>(idx) + rep->offset;
    Matrix m = {};
    if (!FloatNearlyZero(rep->anchor.x) || !FloatNearlyZero(rep->anchor.y)) {
      m = Matrix::Translate(-rep->anchor.x, -rep->anchor.y);
    }
    float sx = std::pow(rep->scale.x, prog);
    float sy = std::pow(rep->scale.y, prog);
    if (!FloatNearlyZero(sx - 1.0f) || !FloatNearlyZero(sy - 1.0f)) {
      m = Matrix::Scale(sx, sy) * m;
    }
    float rot = rep->rotation * prog;
    if (!FloatNearlyZero(rot)) {
      m = Matrix::Rotate(rot) * m;
    }
    float px = rep->position.x * prog;
    float py = rep->position.y * prog;
    if (!FloatNearlyZero(px) || !FloatNearlyZero(py)) {
      m = Matrix::Translate(px, py) * m;
    }
    if (!FloatNearlyZero(rep->anchor.x) || !FloatNearlyZero(rep->anchor.y)) {
      m = Matrix::Translate(rep->anchor.x, rep->anchor.y) * m;
    }
    float denom = std::max(std::ceil(rep->copies) - 1.0f, 1.0f);
    float np = std::clamp((static_cast<float>(idx) + rep->offset) / denom, 0.0f, 1.0f);
    float ca = rep->startAlpha + (rep->endAlpha - rep->startAlpha) * np;
    if (idx == n - 1 && frac > 0 && frac < 1.0f) {
      ca *= frac;
    }
    float ea = ca * alpha;
    if (!m.isIdentity() || ea < 1.0f) {
      out.openTag("div");
      std::string s = "position:absolute";
      if (!m.isIdentity()) {
        s += ";transform:" + MatrixToCSS(m);
      }
      if (ea < 1.0f) {
        s += ";opacity:" + FloatToString(ea);
      }
      out.addAttr("style", s);
      out.closeTagStart();
      renderGeo(out, geos, fill, stroke, 1.0f, false, nullptr, false);
      out.closeTag();
    } else {
      renderGeo(out, geos, fill, stroke, 1.0f, false, nullptr, false);
    }
  }
}

void HTMLWriter::writeComposition(HTMLBuilder& out, const Composition* comp, float alpha,
                                  bool distribute) {
  if (_ctx->visitedCompositions.count(comp)) {
    return;
  }
  _ctx->visitedCompositions.insert(comp);
  bool needContainer = comp->width > 0 && comp->height > 0;
  if (needContainer) {
    out.openTag("div");
    out.addAttr("style", "position:relative;width:" + FloatToString(comp->width) +
                             "px;height:" + FloatToString(comp->height) + "px;overflow:hidden");
    out.closeTagStart();
  }
  for (auto* layer : comp->layers) {
    writeLayer(out, layer, alpha, distribute);
  }
  if (needContainer) {
    out.closeTag();
  }
  _ctx->visitedCompositions.erase(comp);
}

//==============================================================================
// HTMLWriter – element processing
//==============================================================================

void HTMLWriter::applyTrimAttrs(HTMLBuilder& builder, const TrimPath* trim, bool isEllipse) {
  if (!trim) {
    return;
  }
  if (trim->start == 0.0f && trim->end == 1.0f && FloatNearlyZero(trim->offset)) {
    return;
  }
  builder.addAttr("pathLength", "1");
  // SVG ellipse paths start at 3 o'clock (rightmost point), while PAGX
  // starts at 12 o'clock (top). Apply +0.25 offset for ellipse geometries.
  float ellipseAdj = isEllipse ? 0.25f : 0.0f;
  float offsetFrac = trim->offset / 360.0f + ellipseAdj;
  float s = std::fmod(trim->start + offsetFrac, 1.0f);
  float e = std::fmod(trim->end + offsetFrac, 1.0f);
  if (s < 0.0f) {
    s += 1.0f;
  }
  if (e < 0.0f) {
    e += 1.0f;
  }
  if (s <= e) {
    float visible = e - s;
    float gap = 1.0f - visible;
    builder.addAttr("stroke-dasharray", FloatToString(visible) + " " + FloatToString(gap));
    builder.addAttr("stroke-dashoffset", FloatToString(-s));
  } else {
    // Wrapping case: visible = [s,1] + [0,e] → two segments
    float seg1 = 1.0f - s;
    float seg2 = e;
    float gap = s - e;
    builder.addAttr("stroke-dasharray",
                    FloatToString(seg2) + " " + FloatToString(gap) + " " + FloatToString(seg1));
  }
}

float HTMLWriter::computeGeoPathLength(const GeoInfo& geo) {
  if (!geo.modifiedPathData.empty()) {
    PathData pathData = PathDataFromSVGString(geo.modifiedPathData);
    return ComputePathLength(pathData);
  }
  switch (geo.type) {
    case NodeType::Rectangle: {
      auto r = static_cast<const Rectangle*>(geo.element);
      if (r->roundness <= 0) {
        return 2 * (r->size.width + r->size.height);
      }
      // Approximate with arc length
      float rn = std::min(r->roundness, std::min(r->size.width / 2, r->size.height / 2));
      float straightLen = 2 * (r->size.width + r->size.height - 4 * rn);
      float arcLen = 2 * static_cast<float>(M_PI) * rn;
      return straightLen + arcLen;
    }
    case NodeType::Ellipse: {
      auto e = static_cast<const Ellipse*>(geo.element);
      float rx = e->size.width / 2;
      float ry = e->size.height / 2;
      // Ramanujan's ellipse perimeter approximation
      float h = ((rx - ry) * (rx - ry)) / ((rx + ry) * (rx + ry));
      return static_cast<float>(M_PI) * (rx + ry) * (1 + 3 * h / (10 + std::sqrt(4 - 3 * h)));
    }
    case NodeType::Path: {
      auto p = static_cast<const Path*>(geo.element);
      if (p->data) {
        return ComputePathLength(*p->data);
      }
      return 0;
    }
    case NodeType::Polystar: {
      std::string d = BuildPolystarPath(static_cast<const Polystar*>(geo.element));
      if (!d.empty()) {
        PathData pathData = PathDataFromSVGString(d);
        return ComputePathLength(pathData);
      }
      return 0;
    }
    default:
      return 0;
  }
}

void HTMLWriter::applyTrimAttrsContinuous(HTMLBuilder& builder, const TrimPath* trim,
                                          const std::vector<float>& pathLengths, float totalLength,
                                          size_t geoIndex, bool isEllipse) {
  if (!trim || trim->type != TrimType::Continuous) {
    applyTrimAttrs(builder, trim);
    return;
  }
  if (totalLength <= 0.0f || geoIndex >= pathLengths.size()) {
    return;
  }
  // Compute cumulative start position for this geometry
  float cumStart = 0.0f;
  for (size_t i = 0; i < geoIndex; i++) {
    cumStart += pathLengths[i];
  }
  float thisLength = pathLengths[geoIndex];
  float cumEnd = cumStart + thisLength;
  // Normalize to [0, 1]
  float normStart = cumStart / totalLength;
  float normEnd = cumEnd / totalLength;
  // Apply global trim with offset
  // SVG ellipse paths start at 3 o'clock (rightmost point), while PAGX
  // starts at 12 o'clock (top). Apply +0.25 offset for ellipse geometries.
  float ellipseAdj = isEllipse ? 0.25f : 0.0f;
  float offsetFrac = trim->offset / 360.0f + ellipseAdj;
  float globalStart = std::fmod(trim->start + offsetFrac, 1.0f);
  float globalEnd = std::fmod(trim->end + offsetFrac, 1.0f);
  if (globalStart < 0.0f) {
    globalStart += 1.0f;
  }
  if (globalEnd < 0.0f) {
    globalEnd += 1.0f;
  }
  // Intersect global trim range with this geometry's range
  float localStart = std::max(globalStart, normStart);
  float localEnd = std::min(globalEnd, normEnd);
  if (localStart >= localEnd) {
    // No visible portion for this geometry
    builder.addAttr("stroke-dasharray", "0");
    return;
  }
  // Map back to local [0, 1] range for this geometry
  float localNormStart = (localStart - normStart) / (normEnd - normStart);
  float localNormEnd = (localEnd - normStart) / (normEnd - normStart);
  builder.addAttr("pathLength", "1");
  float visible = localNormEnd - localNormStart;
  float gap = 1.0f - visible;
  builder.addAttr("stroke-dasharray", FloatToString(visible) + " " + FloatToString(gap));
  builder.addAttr("stroke-dashoffset", FloatToString(-localNormStart));
}

void HTMLWriter::paintGeos(HTMLBuilder& out, const std::vector<GeoInfo>& geos, const Fill* fill,
                           const Stroke* stroke, const TextBox* textBox, float alpha, bool hasTrim,
                           const TrimPath* curTrim, bool hasMerge, MergePathMode mergeMode) {
  if (geos.empty()) {
    return;
  }
  bool hasText = false;
  for (auto& g : geos) {
    if (g.type == NodeType::Text) {
      hasText = true;
      break;
    }
  }
  if (hasText) {
    for (auto& g : geos) {
      if (g.type == NodeType::Text) {
        writeText(out, static_cast<const Text*>(g.element), fill, stroke, textBox, alpha);
      } else {
        std::vector<GeoInfo> single = {g};
        renderGeo(out, single, fill, stroke, alpha, hasTrim, curTrim, hasMerge, mergeMode);
      }
    }
  } else {
    renderGeo(out, geos, fill, stroke, alpha, hasTrim, curTrim, hasMerge, mergeMode);
  }
}

void HTMLWriter::writeElements(HTMLBuilder& out, const std::vector<Element*>& elements, float alpha,
                               bool distribute, LayerPlacement targetPlacement) {
  std::vector<GeoInfo> geos = {};
  const Fill* curFill = nullptr;
  const Stroke* curStroke = nullptr;
  const TextBox* curTextBox = nullptr;
  bool hasTrim = false;
  const TrimPath* curTrim = nullptr;
  bool hasMerge = false;
  MergePathMode mergeMode = MergePathMode::Append;
  float roundCornerRadius = 0.0f;
  const TextModifier* curTextModifier = nullptr;
  const TextPath* curTextPath = nullptr;

  // Pre-scan: check if a Repeater exists among the elements.
  // When a Repeater is present, Fill/Stroke before it should not render independently.
  bool hasUpcomingRepeater = false;
  // Pre-scan for TextBox and rich-text Groups (Groups containing Text + Fill/Stroke).
  // When a TextBox coexists with such Groups, render them as a single rich text container.
  const TextBox* preScannedTextBox = nullptr;
  struct RichTextSpan {
    const Text* text = nullptr;
    const Fill* fill = nullptr;
    const Stroke* stroke = nullptr;
  };
  std::vector<RichTextSpan> richTextSpans = {};
  int richTextGroupCount = 0;
  for (auto* e : elements) {
    if (e->nodeType() == NodeType::Repeater) {
      hasUpcomingRepeater = true;
    } else if (e->nodeType() == NodeType::TextBox) {
      preScannedTextBox = static_cast<const TextBox*>(e);
    } else if (e->nodeType() == NodeType::Group && preScannedTextBox == nullptr) {
      // Count Groups that look like rich text spans (Text + Fill/Stroke, no geometry)
      auto grp = static_cast<const Group*>(e);
      const Text* spanText = nullptr;
      const Fill* spanFill = nullptr;
      const Stroke* spanStroke = nullptr;
      bool isTextSpan = true;
      for (auto* ge : grp->elements) {
        auto gt = ge->nodeType();
        if (gt == NodeType::Text) {
          spanText = static_cast<const Text*>(ge);
        } else if (gt == NodeType::Fill) {
          spanFill = static_cast<const Fill*>(ge);
        } else if (gt == NodeType::Stroke) {
          spanStroke = static_cast<const Stroke*>(ge);
        } else {
          isTextSpan = false;
          break;
        }
      }
      if (isTextSpan && spanText && (spanFill || spanStroke)) {
        richTextGroupCount++;
        richTextSpans.push_back({spanText, spanFill, spanStroke});
      }
    }
  }
  // Only use rich text rendering when TextBox exists with multiple text-span Groups
  bool useRichText = preScannedTextBox != nullptr && richTextGroupCount >= 2;

  for (auto* element : elements) {
    auto type = element->nodeType();
    switch (type) {
      case NodeType::Rectangle:
      case NodeType::Ellipse:
      case NodeType::Path:
      case NodeType::Polystar:
      case NodeType::Text:
        geos.push_back({type, element, {}});
        break;
      case NodeType::Fill: {
        auto fill = static_cast<const Fill*>(element);
        curFill = fill;
        // Skip rendering if a Repeater follows (it will handle rendering)
        if (fill->placement == targetPlacement && !hasUpcomingRepeater) {
          float a = distribute ? alpha : 1.0f;
          // Handle TextModifier or TextPath if present
          if (curTextModifier && !geos.empty()) {
            writeTextModifier(out, geos, curTextModifier, curFill, nullptr, curTextBox, a);
          } else if (curTextPath && !geos.empty()) {
            writeTextPath(out, geos, curTextPath, curFill, nullptr, curTextBox, a);
          } else {
            paintGeos(out, geos, curFill, nullptr, curTextBox, a, hasTrim, curTrim, hasMerge,
                      mergeMode);
          }
        }
        break;
      }
      case NodeType::Stroke: {
        auto stroke = static_cast<const Stroke*>(element);
        curStroke = stroke;
        // Skip rendering if a Repeater follows (it will handle rendering)
        if (stroke->placement == targetPlacement && !hasUpcomingRepeater) {
          float a = distribute ? alpha : 1.0f;
          // Handle TextModifier or TextPath if present
          if (curTextModifier && !geos.empty()) {
            writeTextModifier(out, geos, curTextModifier, nullptr, curStroke, curTextBox, a);
          } else if (curTextPath && !geos.empty()) {
            writeTextPath(out, geos, curTextPath, nullptr, curStroke, curTextBox, a);
          } else {
            paintGeos(out, geos, nullptr, curStroke, curTextBox, a, hasTrim, curTrim, hasMerge,
                      mergeMode);
          }
        }
        break;
      }
      case NodeType::TextBox: {
        curTextBox = static_cast<const TextBox*>(element);
        if (useRichText && !richTextSpans.empty()) {
          auto tb = curTextBox;
          // Render all rich text spans inside a single TextBox container.
          std::string style = "position:absolute;left:" + FloatToString(tb->position.x) +
                              "px;top:" + FloatToString(tb->position.y) + "px";
          if (tb->size.width > 0) {
            style += ";width:" + FloatToString(tb->size.width) + "px";
          }
          if (tb->size.height > 0) {
            style += ";height:" + FloatToString(tb->size.height) + "px";
          }
          if (tb->paragraphAlign != ParagraphAlign::Near) {
            style += ";display:flex;flex-direction:column";
            if (tb->paragraphAlign == ParagraphAlign::Middle) {
              style += ";justify-content:center";
            } else if (tb->paragraphAlign == ParagraphAlign::Far) {
              style += ";justify-content:flex-end";
            }
          }
          if (tb->textAlign == TextAlign::Center) {
            style += ";text-align:center";
          } else if (tb->textAlign == TextAlign::End) {
            style += ";text-align:end";
          } else if (tb->textAlign == TextAlign::Justify) {
            style += ";text-align:justify";
          }
          if (tb->wordWrap) {
            style += ";word-wrap:break-word";
          }
          if (tb->overflow == Overflow::Hidden) {
            style += ";overflow:hidden";
          }
          out.openTag("div");
          out.addAttr("style", style);
          out.closeTagStart();
          for (auto& span : richTextSpans) {
            std::string spanStyle = "white-space:pre";
            if (!span.text->fontFamily.empty()) {
              spanStyle += ";font-family:'" + span.text->fontFamily + "'";
            }
            spanStyle += ";font-size:" + FloatToString(span.text->fontSize) + "px";
            if (!span.text->fontStyle.empty()) {
              if (span.text->fontStyle.find("Bold") != std::string::npos) {
                spanStyle += ";font-weight:bold";
              }
              if (span.text->fontStyle.find("Italic") != std::string::npos) {
                spanStyle += ";font-style:italic";
              }
            }
            if (span.text->letterSpacing != 0.0f) {
              spanStyle += ";letter-spacing:" + FloatToString(span.text->letterSpacing) + "px";
            }
            if (span.fill && span.fill->color &&
                span.fill->color->nodeType() == NodeType::SolidColor) {
              auto sc = static_cast<const SolidColor*>(span.fill->color);
              spanStyle += ";color:" + ColorToRGBA(sc->color, span.fill->alpha);
            }
            if (span.stroke && span.stroke->color &&
                span.stroke->color->nodeType() == NodeType::SolidColor) {
              auto sc = static_cast<const SolidColor*>(span.stroke->color);
              spanStyle += ";-webkit-text-stroke:" + FloatToString(span.stroke->width) + "px " +
                           ColorToRGBA(sc->color, span.stroke->alpha);
              spanStyle += ";paint-order:stroke fill";
            }
            out.openTag("span");
            out.addAttr("style", spanStyle);
            out.closeTagWithText(span.text->text);
          }
          out.closeTag();
        }
        break;
      }
      case NodeType::TrimPath:
        hasTrim = true;
        curTrim = static_cast<const TrimPath*>(element);
        break;
      case NodeType::RoundCorner: {
        auto rc = static_cast<const RoundCorner*>(element);
        roundCornerRadius = rc->radius;
        // Apply RoundCorner to all accumulated geometries
        if (roundCornerRadius > 0) {
          for (auto& g : geos) {
            if (g.type == NodeType::Rectangle || g.type == NodeType::Ellipse ||
                g.type == NodeType::Path || g.type == NodeType::Polystar) {
              PathData pathData = GeoToPathData(g.element, g.type);
              PathData rounded = ApplyRoundCorner(pathData, roundCornerRadius);
              g.modifiedPathData = PathDataToSVGString(rounded);
            }
          }
        }
        break;
      }
      case NodeType::MergePath: {
        auto mp = static_cast<const MergePath*>(element);
        hasMerge = true;
        mergeMode = mp->mode;
        // Per spec §9.3, MergePath clears previously accumulated Fill/Stroke
        curFill = nullptr;
        curStroke = nullptr;
        break;
      }
      case NodeType::TextModifier:
        curTextModifier = static_cast<const TextModifier*>(element);
        break;
      case NodeType::TextPath:
        curTextPath = static_cast<const TextPath*>(element);
        break;
      case NodeType::Group: {
        auto group = static_cast<const Group*>(element);
        // When useRichText is active, skip Groups that are part of the rich text spans.
        // They will be rendered collectively when the TextBox element is encountered.
        if (useRichText) {
          bool isRichSpan = false;
          for (auto& span : richTextSpans) {
            if (span.text != nullptr) {
              for (auto* ge : group->elements) {
                if (ge == span.text) {
                  isRichSpan = true;
                  break;
                }
              }
            }
            if (isRichSpan) {
              break;
            }
          }
          if (isRichSpan) {
            break;
          }
        }
        // Check if the Group has its own Fill/Stroke (scope-isolated).
        // If not, propagate its geometry to the parent geos vector.
        bool hasPainter = false;
        for (auto* ge : group->elements) {
          auto gt = ge->nodeType();
          if (gt == NodeType::Fill || gt == NodeType::Stroke) {
            hasPainter = true;
            break;
          }
        }
        if (hasPainter) {
          writeGroup(out, group, alpha, distribute);
        } else {
          // Geometry propagation: add child geometries with Group transform applied.
          Matrix gm = BuildGroupMatrix(group);
          for (auto* ge : group->elements) {
            auto gt = ge->nodeType();
            if (gt == NodeType::Rectangle || gt == NodeType::Ellipse || gt == NodeType::Path ||
                gt == NodeType::Polystar) {
              // Convert geometry to path data with group transform baked in
              PathData pathData = GeoToPathData(ge, gt);
              if (!pathData.isEmpty()) {
                std::string svgPath = gm.isIdentity() ? PathDataToSVGString(pathData)
                                                      : TransformPathDataToSVG(pathData, gm);
                geos.push_back({gt, ge, svgPath});
              }
            } else if (gt == NodeType::Text) {
              geos.push_back({gt, ge, {}});
            } else if (gt == NodeType::Group) {
              // Nested groups without painters also propagate
              writeGroup(out, static_cast<const Group*>(ge), alpha, distribute);
            }
          }
        }
        break;
      }
      case NodeType::Repeater: {
        auto rep = static_cast<const Repeater*>(element);
        // Look ahead for Fill/Stroke that follow the Repeater in the element list.
        const Fill* repFill = curFill;
        const Stroke* repStroke = curStroke;
        if (!repFill || !repStroke) {
          for (auto* e : elements) {
            if (e == element) {
              continue;  // Skip elements before the Repeater (already processed).
            }
            if (e->nodeType() == NodeType::Fill && !repFill) {
              auto f = static_cast<const Fill*>(e);
              if (f->placement == targetPlacement) {
                repFill = f;
              }
            } else if (e->nodeType() == NodeType::Stroke && !repStroke) {
              auto s = static_cast<const Stroke*>(e);
              if (s->placement == targetPlacement) {
                repStroke = s;
              }
            }
          }
        }
        writeRepeater(out, rep, geos, repFill, repStroke, distribute ? alpha : 1.0f);
        geos.clear();
        curFill = nullptr;
        curStroke = nullptr;
        hasTrim = false;
        curTrim = nullptr;
        hasMerge = false;
        mergeMode = MergePathMode::Append;
        roundCornerRadius = 0.0f;
        curTextModifier = nullptr;
        curTextPath = nullptr;
        break;
      }
      default:
        break;
    }
  }
}

void HTMLWriter::writeLayerContents(HTMLBuilder& out, const Layer* layer, float alpha,
                                    bool distribute, LayerPlacement targetPlacement) {
  writeElements(out, layer->contents, alpha, distribute, targetPlacement);
}

//==============================================================================
// HTMLWriter – layer
//==============================================================================

void HTMLWriter::writeLayer(HTMLBuilder& out, const Layer* layer, float parentAlpha,
                            bool distributeAlpha) {
  if (!layer->visible) {
    out.openTag("div");
    out.addAttr("class", "pagx-layer");
    out.addAttr("style", "display:none");
    if (!layer->id.empty()) {
      out.addAttr("id", layer->id);
    }
    out.closeTagSelfClosing();
    return;
  }

  std::string style;
  style.reserve(200);
  style += "position:absolute";

  std::string transform = LayerTransformCSS(layer);
  if (!transform.empty()) {
    style += ";transform:" + transform;
  }

  if (layer->preserve3D) {
    style += ";transform-style:preserve-3d";
  }

  bool groupOp = layer->groupOpacity;
  float layerAlpha = layer->alpha;
  if (distributeAlpha) {
    layerAlpha *= parentAlpha;
  }
  bool childDistribute = !groupOp && layerAlpha < 1.0f;

  if (groupOp && layerAlpha < 1.0f) {
    style += ";opacity:" + FloatToString(layerAlpha);
  }

  if (layer->blendMode != BlendMode::Normal) {
    auto blendStr = BlendModeToMixBlendMode(layer->blendMode);
    if (blendStr) {
      style += ";mix-blend-mode:";
      style += blendStr;
    }
  }

  if (!layer->passThroughBackground) {
    style += ";isolation:isolate";
  }

  if (!layer->antiAlias) {
    style += ";shape-rendering:crispEdges;image-rendering:pixelated";
  }

  // Collect all filter values into a single CSS filter property to avoid override
  std::string filterValues;

  // Layer filters
  if (!layer->filters.empty()) {
    std::string filterCSS = writeFilterDefs(layer->filters);
    if (!filterCSS.empty()) {
      filterValues += filterCSS;
    }
  }

  // Track styles that need to be rendered as independent divs (when they have blendMode != Normal)
  std::vector<std::pair<NodeType, const LayerStyle*>> belowStyles = {};
  std::vector<std::pair<NodeType, const LayerStyle*>> aboveStyles = {};

  // Layer styles that produce filter effects
  for (auto* ls : layer->styles) {
    bool hasBlendMode = ls->blendMode != BlendMode::Normal;

    if (ls->nodeType() == NodeType::DropShadowStyle) {
      auto ds = static_cast<const DropShadowStyle*>(ls);
      if (hasBlendMode) {
        // Store for separate rendering with mix-blend-mode
        belowStyles.push_back({NodeType::DropShadowStyle, ls});
      } else if (ds->blurX == ds->blurY && ds->showBehindLayer) {
        if (!filterValues.empty()) {
          filterValues += ' ';
        }
        filterValues += "drop-shadow(" + FloatToString(ds->offsetX) + "px " +
                        FloatToString(ds->offsetY) + "px " + FloatToString(ds->blurX) + "px " +
                        ColorToRGBA(ds->color) + ")";
      } else {
        // Asymmetric blur or showBehindLayer=false: use SVG filter
        std::string fid = _ctx->nextId("filter");
        _defs->openTag("filter");
        _defs->addAttr("id", fid);
        _defs->addAttr("x", "-50%");
        _defs->addAttr("y", "-50%");
        _defs->addAttr("width", "200%");
        _defs->addAttr("height", "200%");
        _defs->addAttr("color-interpolation-filters", "sRGB");
        _defs->closeTagStart();
        _defs->openTag("feGaussianBlur");
        _defs->addAttr("in", "SourceAlpha");
        _defs->addAttr("stdDeviation", FloatToString(ds->blurX) + " " + FloatToString(ds->blurY));
        _defs->addAttr("result", "blur");
        _defs->closeTagSelfClosing();
        _defs->openTag("feOffset");
        _defs->addAttr("in", "blur");
        if (!FloatNearlyZero(ds->offsetX)) {
          _defs->addAttr("dx", FloatToString(ds->offsetX));
        }
        if (!FloatNearlyZero(ds->offsetY)) {
          _defs->addAttr("dy", FloatToString(ds->offsetY));
        }
        _defs->addAttr("result", "off");
        _defs->closeTagSelfClosing();
        _defs->openTag("feColorMatrix");
        _defs->addAttr("in", "off");
        _defs->addAttr("type", "matrix");
        _defs->addAttr("values", "0 0 0 0 " + FloatToString(ds->color.red) + " 0 0 0 0 " +
                                     FloatToString(ds->color.green) + " 0 0 0 0 " +
                                     FloatToString(ds->color.blue) + " 0 0 0 " +
                                     FloatToString(ds->color.alpha) + " 0");
        _defs->addAttr("result", "shadow");
        _defs->closeTagSelfClosing();
        if (!ds->showBehindLayer) {
          // Erase the part of the shadow that is occluded by the layer outline
          _defs->openTag("feComposite");
          _defs->addAttr("in", "shadow");
          _defs->addAttr("in2", "SourceAlpha");
          _defs->addAttr("operator", "out");
          _defs->addAttr("result", "shadowClipped");
          _defs->closeTagSelfClosing();
          _defs->openTag("feMerge");
          _defs->closeTagStart();
          _defs->openTag("feMergeNode");
          _defs->addAttr("in", "shadowClipped");
          _defs->closeTagSelfClosing();
          _defs->openTag("feMergeNode");
          _defs->addAttr("in", "SourceGraphic");
          _defs->closeTagSelfClosing();
          _defs->closeTag();
        } else {
          _defs->openTag("feMerge");
          _defs->closeTagStart();
          _defs->openTag("feMergeNode");
          _defs->addAttr("in", "shadow");
          _defs->closeTagSelfClosing();
          _defs->openTag("feMergeNode");
          _defs->addAttr("in", "SourceGraphic");
          _defs->closeTagSelfClosing();
          _defs->closeTag();
        }
        _defs->closeTag();
        if (!filterValues.empty()) {
          filterValues += ' ';
        }
        filterValues += "url(#" + fid + ")";
      }
    } else if (ls->nodeType() == NodeType::InnerShadowStyle) {
      auto is = static_cast<const InnerShadowStyle*>(ls);
      if (hasBlendMode) {
        // Store for separate rendering with mix-blend-mode
        aboveStyles.push_back({NodeType::InnerShadowStyle, ls});
      } else {
        // InnerShadowStyle: SVG filter (offset → invert → blur → clip to interior → colorize)
        std::string fid = _ctx->nextId("filter");
        _defs->openTag("filter");
        _defs->addAttr("id", fid);
        _defs->addAttr("x", "-50%");
        _defs->addAttr("y", "-50%");
        _defs->addAttr("width", "200%");
        _defs->addAttr("height", "200%");
        _defs->addAttr("color-interpolation-filters", "sRGB");
        _defs->closeTagStart();
        _defs->openTag("feOffset");
        _defs->addAttr("in", "SourceAlpha");
        if (!FloatNearlyZero(is->offsetX)) {
          _defs->addAttr("dx", FloatToString(is->offsetX));
        }
        if (!FloatNearlyZero(is->offsetY)) {
          _defs->addAttr("dy", FloatToString(is->offsetY));
        }
        _defs->addAttr("result", "iOff");
        _defs->closeTagSelfClosing();
        _defs->openTag("feComposite");
        _defs->addAttr("in", "SourceAlpha");
        _defs->addAttr("in2", "iOff");
        _defs->addAttr("operator", "arithmetic");
        _defs->addAttr("k2", "-1");
        _defs->addAttr("k3", "1");
        _defs->addAttr("result", "iComp");
        _defs->closeTagSelfClosing();
        std::string sd = FloatToString(is->blurX);
        if (is->blurX != is->blurY) {
          sd += " " + FloatToString(is->blurY);
        }
        _defs->openTag("feGaussianBlur");
        _defs->addAttr("in", "iComp");
        _defs->addAttr("stdDeviation", sd);
        _defs->addAttr("result", "iBlur");
        _defs->closeTagSelfClosing();
        _defs->openTag("feColorMatrix");
        _defs->addAttr("in", "iBlur");
        _defs->addAttr("type", "matrix");
        _defs->addAttr("values", "0 0 0 0 " + FloatToString(is->color.red) + " 0 0 0 0 " +
                                     FloatToString(is->color.green) + " 0 0 0 0 " +
                                     FloatToString(is->color.blue) + " 0 0 0 " +
                                     FloatToString(is->color.alpha) + " 0");
        _defs->addAttr("result", "iShadow");
        _defs->closeTagSelfClosing();
        _defs->openTag("feMerge");
        _defs->closeTagStart();
        _defs->openTag("feMergeNode");
        _defs->addAttr("in", "SourceGraphic");
        _defs->closeTagSelfClosing();
        _defs->openTag("feMergeNode");
        _defs->addAttr("in", "iShadow");
        _defs->closeTagSelfClosing();
        _defs->closeTag();
        _defs->closeTag();
        if (!filterValues.empty()) {
          filterValues += ' ';
        }
        filterValues += "url(#" + fid + ")";
      }
    } else if (ls->nodeType() == NodeType::BackgroundBlurStyle) {
      if (hasBlendMode) {
        belowStyles.push_back({NodeType::BackgroundBlurStyle, ls});
      }
      // BackgroundBlurStyle without blendMode is handled later in the rendering order
    }
  }

  if (!filterValues.empty()) {
    style += ";filter:" + filterValues;
  }

  // scrollRect: wrap content in a clipping container
  bool needScrollRectWrapper = layer->hasScrollRect;

  // Mask
  if (layer->mask != nullptr) {
    if (layer->maskType == MaskType::Contour) {
      auto clipId = writeClipDef(layer->mask);
      style += ";clip-path:url(#" + clipId + ")";
    } else {
      style += writeMaskCSS(layer->mask, layer->maskType);
    }
  }

  out.openTag("div");
  out.addAttr("class", "pagx-layer");
  if (!layer->id.empty()) {
    out.addAttr("id", layer->id);
  }
  if (!layer->name.empty()) {
    out.addAttr("data-name", layer->name);
  }
  for (auto& [key, value] : layer->customData) {
    out.addAttr(("data-" + key).c_str(), value);
  }
  out.addAttr("style", style);

  bool hasContent = !layer->contents.empty() || !layer->children.empty();
  if (layer->composition) {
    hasContent = true;
  }
  if (!hasContent) {
    out.closeTagSelfClosing();
    return;
  }

  out.closeTagStart();

  // Render styles with blendMode that need to be below content (DropShadow, BackgroundBlur)
  for (auto& [styleType, ls] : belowStyles) {
    auto blendStr = BlendModeToMixBlendMode(ls->blendMode);
    if (styleType == NodeType::DropShadowStyle) {
      auto ds = static_cast<const DropShadowStyle*>(ls);
      std::string shadowStyle = "position:absolute;inset:0";
      if (blendStr) {
        shadowStyle += ";mix-blend-mode:";
        shadowStyle += blendStr;
      }
      bool isUniformBlur = FloatNearlyZero(ds->blurX - ds->blurY);
      if (isUniformBlur && ds->showBehindLayer) {
        // Simple case: CSS drop-shadow
        shadowStyle += ";filter:drop-shadow(" + FloatToString(ds->offsetX) + "px " +
                       FloatToString(ds->offsetY) + "px " + FloatToString(ds->blurX) + "px " +
                       ColorToRGBA(ds->color) + ")";
        out.openTag("div");
        out.addAttr("style", shadowStyle);
        out.closeTagSelfClosing();
      } else {
        // Complex case: SVG filter for anisotropic blur or showBehindLayer=false
        std::string filterId = _ctx->nextId("dsblend");
        _defs->openTag("filter");
        _defs->addAttr("id", filterId);
        _defs->addAttr("x", "-50%");
        _defs->addAttr("y", "-50%");
        _defs->addAttr("width", "200%");
        _defs->addAttr("height", "200%");
        _defs->closeTagStart();
        _defs->openTag("feGaussianBlur");
        _defs->addAttr("in", "SourceGraphic");
        _defs->addAttr("stdDeviation",
                       FloatToString(ds->blurX / 2) + " " + FloatToString(ds->blurY / 2));
        _defs->addAttr("result", "blur");
        _defs->closeTagSelfClosing();
        _defs->openTag("feOffset");
        _defs->addAttr("in", "blur");
        _defs->addAttr("dx", FloatToString(ds->offsetX));
        _defs->addAttr("dy", FloatToString(ds->offsetY));
        _defs->addAttr("result", "offsetBlur");
        _defs->closeTagSelfClosing();
        _defs->openTag("feFlood");
        _defs->addAttr("flood-color", ColorToSVGHex(ds->color));
        if (ds->color.alpha < 1.0f) {
          _defs->addAttr("flood-opacity", FloatToString(ds->color.alpha));
        }
        _defs->addAttr("result", "color");
        _defs->closeTagSelfClosing();
        _defs->openTag("feComposite");
        _defs->addAttr("in", "color");
        _defs->addAttr("in2", "offsetBlur");
        _defs->addAttr("operator", "in");
        _defs->addAttr("result", "shadow");
        _defs->closeTagSelfClosing();
        if (!ds->showBehindLayer) {
          _defs->openTag("feComposite");
          _defs->addAttr("in", "shadow");
          _defs->addAttr("in2", "SourceGraphic");
          _defs->addAttr("operator", "out");
          _defs->addAttr("result", "shadow");
          _defs->closeTagSelfClosing();
        }
        _defs->closeTag();  // </filter>
        shadowStyle += ";filter:url(#" + filterId + ")";
        out.openTag("div");
        out.addAttr("style", shadowStyle);
        out.closeTagSelfClosing();
      }
    } else if (styleType == NodeType::BackgroundBlurStyle) {
      auto blur = static_cast<const BackgroundBlurStyle*>(ls);
      float avg = (blur->blurX + blur->blurY) / 2.0f;
      if (avg > 0) {
        std::string blurStyle = "position:absolute;inset:0;backdrop-filter:blur(" +
                                FloatToString(avg) + "px);-webkit-backdrop-filter:blur(" +
                                FloatToString(avg) + "px)";
        if (blendStr) {
          blurStyle += ";mix-blend-mode:";
          blurStyle += blendStr;
        }
        out.openTag("div");
        out.addAttr("style", blurStyle);
        out.closeTagSelfClosing();
      }
    }
  }

  // Background blur styles (without blendMode)
  for (auto* ls : layer->styles) {
    if (ls->nodeType() == NodeType::BackgroundBlurStyle && ls->blendMode == BlendMode::Normal) {
      auto blur = static_cast<const BackgroundBlurStyle*>(ls);
      float avg = (blur->blurX + blur->blurY) / 2.0f;
      if (avg > 0) {
        out.openTag("div");
        out.addAttr("style", "position:absolute;inset:0;backdrop-filter:blur(" +
                                 FloatToString(avg) + "px);-webkit-backdrop-filter:blur(" +
                                 FloatToString(avg) + "px)");
        out.closeTagSelfClosing();
      }
    }
  }

  // scrollRect: create a clipping wrapper div with the visible area's size
  if (needScrollRectWrapper) {
    auto& sr = layer->scrollRect;
    out.openTag("div");
    out.addAttr("style", "position:absolute;left:0px;top:0px;width:" + FloatToString(sr.width) +
                             "px;height:" + FloatToString(sr.height) + "px;overflow:hidden");
    out.closeTagStart();
    out.openTag("div");
    out.addAttr("style", "position:relative;left:" + FloatToString(-sr.x) +
                             "px;top:" + FloatToString(-sr.y) + "px");
    out.closeTagStart();
  }

  float contentAlpha = childDistribute ? layerAlpha : 1.0f;

  // Check if any Fill/Stroke has Foreground placement
  bool hasForeground = false;
  for (auto* e : layer->contents) {
    if (e->nodeType() == NodeType::Fill) {
      if (static_cast<const Fill*>(e)->placement == LayerPlacement::Foreground) {
        hasForeground = true;
        break;
      }
    } else if (e->nodeType() == NodeType::Stroke) {
      if (static_cast<const Stroke*>(e)->placement == LayerPlacement::Foreground) {
        hasForeground = true;
        break;
      }
    }
  }

  // Render order: Background Content → Children → Foreground Content
  writeLayerContents(out, layer, contentAlpha, childDistribute, LayerPlacement::Background);

  if (layer->composition) {
    writeComposition(out, layer->composition, contentAlpha, childDistribute);
  }

  for (auto* child : layer->children) {
    writeLayer(out, child, contentAlpha, childDistribute);
  }

  if (hasForeground) {
    writeLayerContents(out, layer, contentAlpha, childDistribute, LayerPlacement::Foreground);
  }

  // Render styles with blendMode that need to be above content (InnerShadow)
  for (auto& [styleType, ls] : aboveStyles) {
    auto blendStr = BlendModeToMixBlendMode(ls->blendMode);
    if (styleType == NodeType::InnerShadowStyle) {
      auto is = static_cast<const InnerShadowStyle*>(ls);
      // Create an SVG filter for inner shadow and apply it in a div with mix-blend-mode
      std::string fid = _ctx->nextId("isf");
      _defs->openTag("filter");
      _defs->addAttr("id", fid);
      _defs->addAttr("x", "-50%");
      _defs->addAttr("y", "-50%");
      _defs->addAttr("width", "200%");
      _defs->addAttr("height", "200%");
      _defs->addAttr("color-interpolation-filters", "sRGB");
      _defs->closeTagStart();
      _defs->openTag("feOffset");
      _defs->addAttr("in", "SourceAlpha");
      if (!FloatNearlyZero(is->offsetX)) {
        _defs->addAttr("dx", FloatToString(is->offsetX));
      }
      if (!FloatNearlyZero(is->offsetY)) {
        _defs->addAttr("dy", FloatToString(is->offsetY));
      }
      _defs->addAttr("result", "iOff");
      _defs->closeTagSelfClosing();
      _defs->openTag("feComposite");
      _defs->addAttr("in", "SourceAlpha");
      _defs->addAttr("in2", "iOff");
      _defs->addAttr("operator", "arithmetic");
      _defs->addAttr("k2", "-1");
      _defs->addAttr("k3", "1");
      _defs->addAttr("result", "iComp");
      _defs->closeTagSelfClosing();
      std::string sd = FloatToString(is->blurX);
      if (is->blurX != is->blurY) {
        sd += " " + FloatToString(is->blurY);
      }
      _defs->openTag("feGaussianBlur");
      _defs->addAttr("in", "iComp");
      _defs->addAttr("stdDeviation", sd);
      _defs->addAttr("result", "iBlur");
      _defs->closeTagSelfClosing();
      _defs->openTag("feColorMatrix");
      _defs->addAttr("in", "iBlur");
      _defs->addAttr("type", "matrix");
      _defs->addAttr("values", "0 0 0 0 " + FloatToString(is->color.red) + " 0 0 0 0 " +
                                   FloatToString(is->color.green) + " 0 0 0 0 " +
                                   FloatToString(is->color.blue) + " 0 0 0 " +
                                   FloatToString(is->color.alpha) + " 0");
      _defs->closeTagSelfClosing();
      _defs->closeTag();
      std::string shadowStyle = "position:absolute;inset:0;filter:url(#" + fid + ")";
      if (blendStr) {
        shadowStyle += ";mix-blend-mode:";
        shadowStyle += blendStr;
      }
      out.openTag("div");
      out.addAttr("style", shadowStyle);
      out.closeTagSelfClosing();
    }
  }

  if (needScrollRectWrapper) {
    out.closeTag();  // inner offset div
    out.closeTag();  // clip wrapper div
  }

  out.closeTag();
}

//==============================================================================
// Main export function
//==============================================================================

// Forward declarations for serializers (implementations below)
static std::string SerializeToReactJSX(const std::string& nativeHTML,
                                       const HTMLExportOptions& options);
static std::string SerializeToVueSFC(const std::string& nativeHTML,
                                     const HTMLExportOptions& options);

std::string HTMLExporter::ToHTML(const PAGXDocument& doc, const Options& options) {
  HTMLBuilder html(options.indent);
  HTMLBuilder defs(options.indent, 2);
  HTMLWriterContext ctx;
  ctx.docWidth = doc.width;
  ctx.docHeight = doc.height;
  HTMLWriter writer(&defs, &ctx);

  // Root div
  std::string rootStyle = "position:relative;width:" + FloatToString(doc.width) +
                          "px;height:" + FloatToString(doc.height) + "px;overflow:hidden";
  html.openTag("div");
  html.addAttr("class", "pagx-root");
  html.addAttr("data-pagx-version", doc.version);
  html.addAttr("style", rootStyle);
  html.closeTagStart();

  // Render layers into body content
  HTMLBuilder body(options.indent, 1);
  for (auto* layer : doc.layers) {
    writer.writeLayer(body, layer);
  }

  // Insert SVG defs if any
  std::string defsStr = defs.release();
  if (!defsStr.empty()) {
    html.openTag("svg");
    html.addAttr("style", "position:absolute;width:0;height:0;overflow:hidden");
    html.closeTagStart();
    html.openTag("defs");
    html.closeTagStart();
    html.addRawContent(defsStr);
    html.closeTag();  // </defs>
    html.closeTag();  // </svg>
  }

  html.addRawContent(body.release());
  html.closeTag();  // </div>

  std::string nativeHTML = html.release();

  // Apply framework-specific transformation
  switch (options.framework) {
    case HTMLFramework::React:
      return SerializeToReactJSX(nativeHTML, options);
    case HTMLFramework::Vue:
      return SerializeToVueSFC(nativeHTML, options);
    case HTMLFramework::Native:
    default:
      return nativeHTML;
  }
}

bool HTMLExporter::ToFile(const PAGXDocument& document, const std::string& filePath,
                          const Options& options) {
  auto html = ToHTML(document, options);
  if (html.empty()) {
    return false;
  }
  auto dirPath = std::filesystem::path(filePath).parent_path();
  if (!dirPath.empty() && !std::filesystem::exists(dirPath)) {
    std::error_code ec = {};
    std::filesystem::create_directories(dirPath, ec);
    if (ec) {
      return false;
    }
  }
  std::ofstream file(filePath, std::ios::binary);
  if (!file) {
    return false;
  }
  file.write(html.data(), static_cast<std::streamsize>(html.size()));
  return file.good();
}

//==============================================================================
// Framework Serializers
//==============================================================================

static std::string KebabToCamelCase(const std::string& kebab) {
  std::string result;
  result.reserve(kebab.size());
  bool capitalizeNext = false;
  bool isVendorPrefix = !kebab.empty() && kebab[0] == '-';

  for (size_t i = 0; i < kebab.size(); i++) {
    char c = kebab[i];
    if (c == '-') {
      capitalizeNext = true;
      continue;
    }
    if (capitalizeNext) {
      result += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
      capitalizeNext = false;
    } else {
      result += c;
    }
  }

  if (isVendorPrefix && !result.empty()) {
    result[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(result[0])));
  }

  return result;
}

static bool IsNumericValue(const std::string& value) {
  if (value.empty()) {
    return false;
  }
  size_t i = 0;
  bool hasDot = false;
  bool hasDigit = false;

  if (value[i] == '-' || value[i] == '+') {
    i++;
  }

  while (i < value.size()) {
    char c = value[i];
    if (c >= '0' && c <= '9') {
      hasDigit = true;
      i++;
    } else if (c == '.' && !hasDot) {
      hasDot = true;
      i++;
    } else {
      return false;
    }
  }
  return hasDigit;
}

static std::string StyleStringToJSXObject(const std::string& styleStr) {
  if (styleStr.empty()) {
    return "{}";
  }

  std::string result = "{";
  size_t pos = 0;
  bool first = true;

  while (pos < styleStr.size()) {
    size_t semicolonPos = styleStr.find(';', pos);
    if (semicolonPos == std::string::npos) {
      semicolonPos = styleStr.size();
    }

    std::string pair = styleStr.substr(pos, semicolonPos - pos);
    pos = semicolonPos + 1;

    if (pair.empty()) {
      continue;
    }

    auto colonPos = pair.find(':');
    if (colonPos == std::string::npos) {
      continue;
    }

    std::string key = pair.substr(0, colonPos);
    std::string value = pair.substr(colonPos + 1);

    while (!key.empty() && key[0] == ' ') {
      key = key.substr(1);
    }
    while (!key.empty() && key.back() == ' ') {
      key.pop_back();
    }
    while (!value.empty() && value[0] == ' ') {
      value = value.substr(1);
    }
    while (!value.empty() && value.back() == ' ') {
      value.pop_back();
    }

    if (key.empty()) {
      continue;
    }

    if (!first) {
      result += ", ";
    }
    first = false;

    std::string camelKey = KebabToCamelCase(key);
    result += camelKey + ": ";

    if (IsNumericValue(value)) {
      result += value;
    } else {
      // Escape double quotes in value for JSX style object
      std::string escaped = value;
      for (size_t p = 0; p < escaped.size(); p++) {
        if (escaped[p] == '"') {
          escaped.replace(p, 1, "\\\"");
          p++;
        }
      }
      result += "\"" + escaped + "\"";
    }
  }

  result += "}";
  return result;
}

static std::string StyleStringToVueObject(const std::string& styleStr) {
  if (styleStr.empty()) {
    return "{}";
  }

  std::string result = "{";
  size_t pos = 0;
  bool first = true;

  while (pos < styleStr.size()) {
    size_t semicolonPos = styleStr.find(';', pos);
    if (semicolonPos == std::string::npos) {
      semicolonPos = styleStr.size();
    }

    std::string pair = styleStr.substr(pos, semicolonPos - pos);
    pos = semicolonPos + 1;

    if (pair.empty()) {
      continue;
    }

    auto colonPos = pair.find(':');
    if (colonPos == std::string::npos) {
      continue;
    }

    std::string key = pair.substr(0, colonPos);
    std::string value = pair.substr(colonPos + 1);

    while (!key.empty() && key[0] == ' ') {
      key = key.substr(1);
    }
    while (!key.empty() && key.back() == ' ') {
      key.pop_back();
    }
    while (!value.empty() && value[0] == ' ') {
      value = value.substr(1);
    }
    while (!value.empty() && value.back() == ' ') {
      value.pop_back();
    }

    if (key.empty()) {
      continue;
    }

    if (!first) {
      result += ", ";
    }
    first = false;

    std::string camelKey = KebabToCamelCase(key);
    result += camelKey + ": ";

    if (IsNumericValue(value)) {
      result += value;
    } else {
      // Escape single quotes in value for Vue style object
      std::string escaped = value;
      for (size_t p = 0; p < escaped.size(); p++) {
        if (escaped[p] == '\'') {
          escaped.replace(p, 1, "\\\'");
          p++;
        }
      }
      result += "'" + escaped + "'";
    }
  }

  result += "}";
  return result;
}

// Decode HTML entities back to raw characters before style parsing.
// This is necessary because escapeAttr() encodes quotes (&#39; &quot;) which
// contain semicolons that would be misinterpreted as CSS property separators.
static std::string DecodeHTMLEntities(const std::string& str) {
  std::string result;
  result.reserve(str.size());
  size_t i = 0;
  while (i < str.size()) {
    if (str[i] == '&') {
      if (str.compare(i, 5, "&#39;") == 0) {
        result += '\'';
        i += 5;
      } else if (str.compare(i, 6, "&quot;") == 0) {
        result += '"';
        i += 6;
      } else if (str.compare(i, 5, "&amp;") == 0) {
        result += '&';
        i += 5;
      } else if (str.compare(i, 4, "&lt;") == 0) {
        result += '<';
        i += 4;
      } else if (str.compare(i, 4, "&gt;") == 0) {
        result += '>';
        i += 4;
      } else {
        result += str[i++];
      }
    } else {
      result += str[i++];
    }
  }
  return result;
}

static std::string TransformStyleAttributes(const std::string& html, bool useDoubleQuotes) {
  std::string result;
  result.reserve(html.size() * 2);
  size_t pos = 0;

  while (pos < html.size()) {
    size_t stylePos = html.find("style=\"", pos);
    if (stylePos == std::string::npos) {
      result += html.substr(pos);
      break;
    }

    result += html.substr(pos, stylePos - pos);

    size_t valueStart = stylePos + 7;
    size_t valueEnd = html.find('"', valueStart);
    if (valueEnd == std::string::npos) {
      result += html.substr(stylePos);
      break;
    }

    std::string styleValue = DecodeHTMLEntities(html.substr(valueStart, valueEnd - valueStart));
    std::string styleObj =
        useDoubleQuotes ? StyleStringToJSXObject(styleValue) : StyleStringToVueObject(styleValue);

    if (useDoubleQuotes) {
      result += "style={" + styleObj + "}";
    } else {
      result += ":style=\"" + styleObj + "\"";
    }

    pos = valueEnd + 1;
  }

  return result;
}

static std::string ReplaceClassWithClassName(const std::string& html) {
  std::string result;
  result.reserve(html.size());
  size_t pos = 0;

  while (pos < html.size()) {
    size_t classPos = html.find("class=\"", pos);
    if (classPos == std::string::npos) {
      result += html.substr(pos);
      break;
    }

    if (classPos > 0) {
      char prevChar = html[classPos - 1];
      if (prevChar != ' ' && prevChar != '\n' && prevChar != '\t' && prevChar != '<') {
        result += html.substr(pos, classPos - pos + 6);
        pos = classPos + 6;
        continue;
      }
    }

    result += html.substr(pos, classPos - pos);
    result += "className=\"";
    pos = classPos + 7;
  }

  return result;
}

static std::string ReplaceHTMLComments(const std::string& html) {
  std::string result;
  result.reserve(html.size());
  size_t pos = 0;

  while (pos < html.size()) {
    size_t commentStart = html.find("<!--", pos);
    if (commentStart == std::string::npos) {
      result += html.substr(pos);
      break;
    }

    result += html.substr(pos, commentStart - pos);

    size_t commentEnd = html.find("-->", commentStart);
    if (commentEnd == std::string::npos) {
      result += html.substr(commentStart);
      break;
    }

    std::string commentContent = html.substr(commentStart + 4, commentEnd - commentStart - 4);
    result += "{/*" + commentContent + "*/}";
    pos = commentEnd + 3;
  }

  return result;
}

static std::string SerializeToReactJSX(const std::string& nativeHTML,
                                       const HTMLExportOptions& options) {
  std::string jsx = nativeHTML;

  jsx = ReplaceClassWithClassName(jsx);
  jsx = ReplaceHTMLComments(jsx);
  jsx = TransformStyleAttributes(jsx, true);

  std::string indentStr(static_cast<size_t>(options.indent * 2), ' ');
  std::string singleIndent(static_cast<size_t>(options.indent), ' ');

  std::string result;
  result.reserve(jsx.size() + 200);
  result += "export default function " + options.componentName + "() {\n";
  result += singleIndent + "return (\n";

  size_t lineStart = 0;
  while (lineStart < jsx.size()) {
    size_t lineEnd = jsx.find('\n', lineStart);
    if (lineEnd == std::string::npos) {
      lineEnd = jsx.size();
    }

    std::string line = jsx.substr(lineStart, lineEnd - lineStart);
    result += indentStr + line + "\n";
    lineStart = lineEnd + 1;
  }

  result += singleIndent + ");\n";
  result += "}\n";

  return result;
}

static std::string SerializeToVueSFC(const std::string& nativeHTML,
                                     const HTMLExportOptions& options) {
  std::string vue = TransformStyleAttributes(nativeHTML, false);

  std::string singleIndent(static_cast<size_t>(options.indent), ' ');

  std::string result;
  result.reserve(vue.size() + 200);
  result += "<template>\n";

  size_t lineStart = 0;
  while (lineStart < vue.size()) {
    size_t lineEnd = vue.find('\n', lineStart);
    if (lineEnd == std::string::npos) {
      lineEnd = vue.size();
    }

    std::string line = vue.substr(lineStart, lineEnd - lineStart);
    result += singleIndent + line + "\n";
    lineStart = lineEnd + 1;
  }

  result += "</template>\n\n";
  result += "<script setup>\n";
  result += "// Generated by PAGX HTMLExporter\n";
  result += "</script>\n";

  return result;
}

}  // namespace pagx
