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
#include <string>
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
#include "pagx/types/Rect.h"
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
    _buf += "/>\n";
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
    _buf.append(static_cast<size_t>(_level * _spaces), ' ');
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

static std::string ColorToHex(const Color& color) {
  int r = std::clamp(static_cast<int>(std::round(color.red * 255.0f)), 0, 255);
  int g = std::clamp(static_cast<int>(std::round(color.green * 255.0f)), 0, 255);
  int b = std::clamp(static_cast<int>(std::round(color.blue * 255.0f)), 0, 255);
  char buf[8] = {};
  snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
  return buf;
}

static std::string ColorToRGBA(const Color& color, float extra = 1.0f) {
  float a = color.alpha * extra;
  if (a >= 1.0f) {
    return ColorToHex(color);
  }
  int r = std::clamp(static_cast<int>(std::round(color.red * 255.0f)), 0, 255);
  int g = std::clamp(static_cast<int>(std::round(color.green * 255.0f)), 0, 255);
  int b = std::clamp(static_cast<int>(std::round(color.blue * 255.0f)), 0, 255);
  return "rgba(" + std::to_string(r) + "," + std::to_string(g) + "," + std::to_string(b) + "," +
         FloatToString(a) + ")";
}

static std::string GetImageSrc(const Image* image) {
  if (image->data) {
    return "data:image/png;base64," + Base64Encode(image->data->bytes(), image->data->size());
  }
  return image->filePath;
}

static std::string CSSStops(const std::vector<ColorStop*>& stops) {
  std::string r;
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

static std::string LayerTransformCSS(const Layer* layer) {
  if (!layer->matrix3D.isIdentity()) {
    return Matrix3DToCSS(layer->matrix3D);
  }
  if (!layer->matrix.isIdentity()) {
    return MatrixToCSS(layer->matrix);
  }
  if (!FloatNearlyZero(layer->x) || !FloatNearlyZero(layer->y)) {
    return "translate(" + FloatToString(layer->x) + "px," + FloatToString(layer->y) + "px)";
  }
  return {};
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
  std::string d;
  d.reserve(static_cast<size_t>(total) * 24);
  for (int i = 0; i < total; i++) {
    float angle = 0;
    float radius = 0;
    if (isStar) {
      bool outer = (i % 2 == 0);
      int pi = i / 2;
      float frac = static_cast<float>(pi) / ps->pointCount;
      float half = 0.5f / ps->pointCount;
      angle = ps->rotation + (outer ? frac : frac + half) * 360.0f;
      radius = outer ? ps->outerRadius : ps->innerRadius;
    } else {
      angle = ps->rotation + static_cast<float>(i) / ps->pointCount * 360.0f;
      radius = ps->outerRadius;
    }
    float rad = DegreesToRadians(angle);
    float px = ps->position.x + radius * std::cos(rad);
    float py = ps->position.y + radius * std::sin(rad);
    d += (i == 0 ? "M" : "L");
    d += FloatToString(px) + ',' + FloatToString(py);
  }
  d += 'Z';
  return d;
}

//==============================================================================
// HTMLWriterContext
//==============================================================================

static std::string RectToPathData(const Rectangle* r) {
  float x = r->position.x - r->size.width / 2;
  float y = r->position.y - r->size.height / 2;
  float w = r->size.width;
  float h = r->size.height;
  if (r->roundness <= 0) {
    return "M" + FloatToString(x) + "," + FloatToString(y) + "H" + FloatToString(x + w) + "V" +
           FloatToString(y + h) + "H" + FloatToString(x) + "Z";
  }
  float rn = std::min(r->roundness, std::min(w / 2, h / 2));
  std::string d;
  d += "M" + FloatToString(x + rn) + "," + FloatToString(y);
  d += "H" + FloatToString(x + w - rn);
  d += "A" + FloatToString(rn) + "," + FloatToString(rn) + " 0 0 1 " + FloatToString(x + w) + "," +
       FloatToString(y + rn);
  d += "V" + FloatToString(y + h - rn);
  d += "A" + FloatToString(rn) + "," + FloatToString(rn) + " 0 0 1 " + FloatToString(x + w - rn) +
       "," + FloatToString(y + h);
  d += "H" + FloatToString(x + rn);
  d += "A" + FloatToString(rn) + "," + FloatToString(rn) + " 0 0 1 " + FloatToString(x) + "," +
       FloatToString(y + h - rn);
  d += "V" + FloatToString(y + rn);
  d += "A" + FloatToString(rn) + "," + FloatToString(rn) + " 0 0 1 " + FloatToString(x + rn) + "," +
       FloatToString(y);
  d += "Z";
  return d;
}

static std::string EllipseToPathData(const Ellipse* e) {
  float cx = e->position.x;
  float cy = e->position.y;
  float rx = e->size.width / 2;
  float ry = e->size.height / 2;
  return "M" + FloatToString(cx - rx) + "," + FloatToString(cy) + "A" + FloatToString(rx) + "," +
         FloatToString(ry) + " 0 1 0 " + FloatToString(cx + rx) + "," + FloatToString(cy) + "A" +
         FloatToString(rx) + "," + FloatToString(ry) + " 0 1 0 " + FloatToString(cx - rx) + "," +
         FloatToString(cy) + "Z";
}

class HTMLWriterContext {
 public:
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
  };

  // Color source conversions
  std::string colorToCSS(const ColorSource* src, float* outAlpha);
  std::string colorToSVGFill(const ColorSource* src, float* outAlpha);
  void writeSVGGradientDef(const ColorSource* src, const std::string& id);

  // Rendering
  void writeLayerContents(HTMLBuilder& out, const Layer* layer, float alpha, bool distribute);
  void writeElements(HTMLBuilder& out, const std::vector<Element*>& elements, float alpha,
                     bool distribute);
  void renderGeo(HTMLBuilder& out, const std::vector<GeoInfo>& geos, const Fill* fill,
                 const Stroke* stroke, float alpha, bool hasTrim, const TrimPath* trim,
                 bool hasMerge);
  bool canCSS(const std::vector<GeoInfo>& geos, const Fill* fill, const Stroke* stroke,
              bool hasTrim, bool hasMerge);
  void renderCSSDiv(HTMLBuilder& out, const GeoInfo& geo, const Fill* fill, float alpha);
  void renderSVG(HTMLBuilder& out, const std::vector<GeoInfo>& geos, const Fill* fill,
                 const Stroke* stroke, float alpha);
  void writeText(HTMLBuilder& out, const Text* text, const Fill* fill, const Stroke* stroke,
                 const TextBox* tb, float alpha);
  void writeGlyphRunSVG(HTMLBuilder& out, const Text* text, const Fill* fill, const Stroke* stroke,
                        float alpha);
  void writeGroup(HTMLBuilder& out, const Group* group, float alpha, bool distribute);
  void writeRepeater(HTMLBuilder& out, const Repeater* rep, const std::vector<GeoInfo>& geos,
                     const Fill* fill, const Stroke* stroke, float alpha);
  void writeComposition(HTMLBuilder& out, const Composition* comp);

  // Filter defs
  std::string writeFilterDefs(const std::vector<LayerFilter*>& filters);

  // Mask/clip defs
  std::string writeMaskDef(const Layer* mask, MaskType type);
  std::string writeClipDef(const Layer* mask);
  void writeClipContent(HTMLBuilder& out, const Layer* layer, const Matrix& parent);

  // SVG fill/stroke attributes
  void applySVGFill(HTMLBuilder& out, const Fill* fill);
  void applySVGStroke(HTMLBuilder& out, const Stroke* stroke);
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
      Point s = g->matrix.mapPoint(g->startPoint);
      Point e = g->matrix.mapPoint(g->endPoint);
      float ang = std::atan2(e.y - s.y, e.x - s.x) * 180.0f / 3.14159265358979f + 90.0f;
      return "linear-gradient(" + FloatToString(ang) + "deg," + CSSStops(g->colorStops) + ")";
    }
    case NodeType::RadialGradient: {
      auto g = static_cast<const RadialGradient*>(src);
      if (outAlpha) {
        *outAlpha = 1.0f;
      }
      Point c = g->matrix.mapPoint(g->center);
      float sx = std::sqrt(g->matrix.a * g->matrix.a + g->matrix.b * g->matrix.b);
      float r = g->radius * sx;
      return "radial-gradient(circle " + FloatToString(r) + "px at " + FloatToString(c.x) + "px " +
             FloatToString(c.y) + "px," + CSSStops(g->colorStops) + ")";
    }
    case NodeType::ConicGradient: {
      auto g = static_cast<const ConicGradient*>(src);
      if (outAlpha) {
        *outAlpha = 1.0f;
      }
      Point c = g->matrix.mapPoint(g->center);
      float cssAng = g->startAngle + 90.0f;
      return "conic-gradient(from " + FloatToString(cssAng) + "deg at " + FloatToString(c.x) +
             "px " + FloatToString(c.y) + "px," + CSSStops(g->colorStops) + ")";
    }
    case NodeType::ImagePattern: {
      auto p = static_cast<const ImagePattern*>(src);
      if (outAlpha) {
        *outAlpha = 1.0f;
      }
      if (!p->image) {
        return "transparent";
      }
      return "url(" + GetImageSrc(p->image) + ")";
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
    return ColorToHex(sc->color);
  }
  if (src->nodeType() == NodeType::LinearGradient || src->nodeType() == NodeType::RadialGradient) {
    std::string id = _ctx->nextId("grad");
    writeSVGGradientDef(src, id);
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
      _defs->addAttr("stop-color", ColorToHex(stop->color));
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
      _defs->addAttr("stop-color", ColorToHex(stop->color));
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
  std::vector<std::string> dropRes;
  std::vector<std::string> innerRes;
  bool needSrc = false;
  for (auto* f : filters) {
    switch (f->nodeType()) {
      case NodeType::BlurFilter: {
        auto b = static_cast<const BlurFilter*>(f);
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
        _defs->openTag("feGaussianBlur");
        _defs->addAttr("in", "SourceAlpha");
        std::string sd = FloatToString(s->blurX);
        if (s->blurX != s->blurY) {
          sd += " " + FloatToString(s->blurY);
        }
        _defs->addAttr("stdDeviation", sd);
        _defs->addAttr("result", "iBlur" + i);
        _defs->closeTagSelfClosing();
        _defs->openTag("feOffset");
        _defs->addAttr("in", "iBlur" + i);
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
        _defs->openTag("feColorMatrix");
        _defs->addAttr("in", "iComp" + i);
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
        _defs->addAttr("flood-color", ColorToHex(bf->color));
        if (bf->color.alpha < 1.0f) {
          _defs->addAttr("flood-opacity", FloatToString(bf->color.alpha));
        }
        _defs->addAttr("result", "bFlood" + i);
        _defs->closeTagSelfClosing();
        _defs->openTag("feBlend");
        _defs->addAttr("in", "SourceGraphic");
        _defs->addAttr("in2", "bFlood" + i);
        auto ms = BlendModeToSVGString(bf->blendMode);
        if (ms) {
          _defs->addAttr("mode", ms);
        }
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

std::string HTMLWriter::writeMaskDef(const Layer* mask, MaskType type) {
  std::string id = mask->id.empty() ? _ctx->nextId("mask") : mask->id;
  _defs->openTag("mask");
  _defs->addAttr("id", id);
  if (type == MaskType::Alpha) {
    _defs->addAttr("style", "mask-type:alpha");
  }
  _defs->closeTagStart();
  _defs->openTag("g");
  _defs->closeTagStart();
  for (auto* e : mask->contents) {
    if (e->nodeType() == NodeType::Rectangle) {
      auto rect = static_cast<const Rectangle*>(e);
      _defs->openTag("rect");
      float x = rect->position.x - rect->size.width / 2;
      float y = rect->position.y - rect->size.height / 2;
      if (!FloatNearlyZero(x)) {
        _defs->addAttr("x", FloatToString(x));
      }
      if (!FloatNearlyZero(y)) {
        _defs->addAttr("y", FloatToString(y));
      }
      _defs->addAttr("width", FloatToString(rect->size.width));
      _defs->addAttr("height", FloatToString(rect->size.height));
      if (rect->roundness > 0) {
        _defs->addAttr("rx", FloatToString(rect->roundness));
      }
      _defs->addAttr("fill", "white");
      _defs->closeTagSelfClosing();
    } else if (e->nodeType() == NodeType::Ellipse) {
      auto el = static_cast<const Ellipse*>(e);
      _defs->openTag("ellipse");
      _defs->addAttr("cx", FloatToString(el->position.x));
      _defs->addAttr("cy", FloatToString(el->position.y));
      _defs->addAttr("rx", FloatToString(el->size.width / 2));
      _defs->addAttr("ry", FloatToString(el->size.height / 2));
      _defs->addAttr("fill", "white");
      _defs->closeTagSelfClosing();
    } else if (e->nodeType() == NodeType::Path) {
      auto p = static_cast<const Path*>(e);
      if (p->data && !p->data->isEmpty()) {
        _defs->openTag("path");
        _defs->addAttr("d", PathDataToSVGString(*p->data));
        _defs->addAttr("fill", "white");
        _defs->closeTagSelfClosing();
      }
    } else if (e->nodeType() == NodeType::Polystar) {
      auto ps = static_cast<const Polystar*>(e);
      std::string d = BuildPolystarPath(ps);
      if (!d.empty()) {
        _defs->openTag("path");
        _defs->addAttr("d", d);
        _defs->addAttr("fill", "white");
        _defs->closeTagSelfClosing();
      }
    }
  }
  _defs->closeTag();  // </g>
  _defs->closeTag();  // </mask>
  return id;
}

std::string HTMLWriter::writeClipDef(const Layer* mask) {
  std::string id = mask->id.empty() ? _ctx->nextId("clip") : mask->id;
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
      if (p->data && !p->data->isEmpty()) {
        out.openTag("path");
        if (!tr.empty()) {
          out.addAttr("transform", tr);
        }
        out.addAttr("d", PathDataToSVGString(*p->data));
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

void HTMLWriter::applySVGStroke(HTMLBuilder& out, const Stroke* stroke) {
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
  if (stroke->width != 1.0f) {
    out.addAttr("stroke-width", FloatToString(stroke->width));
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
    std::string d;
    for (size_t i = 0; i < stroke->dashes.size(); i++) {
      if (i > 0) {
        d += ',';
      }
      d += FloatToString(stroke->dashes[i]);
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
  auto t = geos[0].type;
  if (t != NodeType::Rectangle && t != NodeType::Ellipse) {
    return false;
  }
  if (fill && fill->color && fill->color->nodeType() == NodeType::DiamondGradient) {
    return false;
  }
  return true;
}

void HTMLWriter::renderCSSDiv(HTMLBuilder& out, const GeoInfo& geo, const Fill* fill, float alpha) {
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
    } else if (ct == NodeType::ImagePattern) {
      style += ";background-image:" + css + ";background-size:cover";
    } else {
      float ea = ca * fill->alpha;
      if (ea < 1.0f) {
        auto sc = static_cast<const SolidColor*>(fill->color);
        style += ";background-color:" + ColorToRGBA(sc->color, fill->alpha);
      } else {
        style += ";background-color:" + css;
      }
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
                           const Stroke* stroke, float alpha) {
  float pad = stroke ? stroke->width : 0.0f;
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

  for (auto& g : geos) {
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
        applySVGFill(out, fill);
        applySVGStroke(out, stroke);
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
        applySVGFill(out, fill);
        applySVGStroke(out, stroke);
        out.closeTagSelfClosing();
        break;
      }
      case NodeType::Path: {
        auto p = static_cast<const Path*>(g.element);
        if (p->data && !p->data->isEmpty()) {
          out.openTag("path");
          out.addAttr("d", PathDataToSVGString(*p->data));
          applySVGFill(out, fill);
          applySVGStroke(out, stroke);
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
          applySVGFill(out, fill);
          applySVGStroke(out, stroke);
          out.closeTagSelfClosing();
        }
        break;
      }
      default:
        break;
    }
  }
  out.closeTag();  // </svg>
}

void HTMLWriter::renderGeo(HTMLBuilder& out, const std::vector<GeoInfo>& geos, const Fill* fill,
                           const Stroke* stroke, float alpha, bool hasTrim,
                           const TrimPath* /*trim*/, bool hasMerge) {
  if (geos.empty()) {
    return;
  }
  if (hasMerge) {
    // Merge all geometries into a single <path> element
    std::string mergedPath;
    for (auto& g : geos) {
      switch (g.type) {
        case NodeType::Rectangle:
          mergedPath += RectToPathData(static_cast<const Rectangle*>(g.element));
          break;
        case NodeType::Ellipse:
          mergedPath += EllipseToPathData(static_cast<const Ellipse*>(g.element));
          break;
        case NodeType::Path: {
          auto p = static_cast<const Path*>(g.element);
          if (p->data && !p->data->isEmpty()) {
            mergedPath += PathDataToSVGString(*p->data);
          }
          break;
        }
        case NodeType::Polystar:
          mergedPath += BuildPolystarPath(static_cast<const Polystar*>(g.element));
          break;
        default:
          break;
      }
    }
    if (mergedPath.empty()) {
      return;
    }
    std::vector<GeoInfo> merged = {{NodeType::Path, nullptr}};
    // Use inline SVG with the merged path
    float pad = stroke ? stroke->width : 0.0f;
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
    applySVGFill(out, fill);
    applySVGStroke(out, stroke);
    out.closeTagSelfClosing();
    out.closeTag();
    return;
  }
  if (canCSS(geos, fill, stroke, hasTrim, false)) {
    renderCSSDiv(out, geos[0], fill, alpha);
  } else {
    renderSVG(out, geos, fill, stroke, alpha);
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
    if (tb->lineHeight > 0) {
      style += ";line-height:" + FloatToString(tb->lineHeight) + "px";
    }
    if (tb->overflow == Overflow::Hidden) {
      style += ";overflow:hidden";
    }
  } else {
    float ty = text->position.y - text->fontSize * 0.8f;
    style += "position:absolute;left:" + FloatToString(text->position.x) +
             "px;top:" + FloatToString(ty) + "px;white-space:pre";
    if (text->textAnchor == TextAnchor::Center) {
      style += ";transform:translateX(-50%)";
    } else if (text->textAnchor == TextAnchor::End) {
      style += ";transform:translateX(-100%)";
    }
  }
  if (!text->fontFamily.empty()) {
    style += ";font-family:'" + text->fontFamily + "'";
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
  if (fill && fill->color && fill->color->nodeType() == NodeType::SolidColor) {
    auto sc = static_cast<const SolidColor*>(fill->color);
    style += ";color:" + ColorToRGBA(sc->color, fill->alpha);
  }
  if (alpha < 1.0f) {
    style += ";opacity:" + FloatToString(alpha);
  }
  (void)stroke;
  out.openTag("span");
  out.addAttr("style", style);
  out.closeTagWithText(text->text);
}

void HTMLWriter::writeGlyphRunSVG(HTMLBuilder& out, const Text* text, const Fill* fill,
                                  const Stroke* stroke, float alpha) {
  auto paths = ComputeGlyphPaths(*text, text->position.x, text->position.y);
  if (paths.empty()) {
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
  out.closeTag();  // </svg>
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
  writeElements(out, group->elements, 1.0f, false);
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
    float np = n > 1 ? static_cast<float>(idx) / static_cast<float>(n - 1) : 0.0f;
    float ca = rep->startAlpha + (rep->endAlpha - rep->startAlpha) * np;
    if (idx == n - 1 && frac > 0 && frac < 1.0f) {
      ca *= frac;
    }
    float ea = ca * alpha;
    if (!m.isIdentity() || ea < 1.0f) {
      out.openTag("div");
      std::string s = "position:relative";
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

void HTMLWriter::writeComposition(HTMLBuilder& out, const Composition* comp) {
  for (auto* layer : comp->layers) {
    writeLayer(out, layer);
  }
}

//==============================================================================
// HTMLWriter – element processing
//==============================================================================

void HTMLWriter::writeElements(HTMLBuilder& out, const std::vector<Element*>& elements, float alpha,
                               bool distribute) {
  std::vector<GeoInfo> geos = {};
  const Fill* curFill = nullptr;
  const Stroke* curStroke = nullptr;
  const TextBox* curTextBox = nullptr;
  bool hasTrim = false;
  const TrimPath* curTrim = nullptr;
  bool hasMerge = false;

  auto paintGeos = [&](const Fill* f, const Stroke* s) {
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
    float a = distribute ? alpha : 1.0f;
    if (hasText) {
      for (auto& g : geos) {
        if (g.type == NodeType::Text) {
          writeText(out, static_cast<const Text*>(g.element), f, s, curTextBox, a);
        } else {
          std::vector<GeoInfo> single = {g};
          renderGeo(out, single, f, s, a, hasTrim, curTrim, hasMerge);
        }
      }
    } else {
      renderGeo(out, geos, f, s, a, hasTrim, curTrim, hasMerge);
    }
  };

  for (auto* element : elements) {
    auto type = element->nodeType();
    switch (type) {
      case NodeType::Rectangle:
      case NodeType::Ellipse:
      case NodeType::Path:
      case NodeType::Polystar:
      case NodeType::Text:
        geos.push_back({type, element});
        break;
      case NodeType::Fill:
        curFill = static_cast<const Fill*>(element);
        paintGeos(curFill, curStroke);
        break;
      case NodeType::Stroke:
        curStroke = static_cast<const Stroke*>(element);
        paintGeos(curFill, curStroke);
        break;
      case NodeType::TextBox:
        curTextBox = static_cast<const TextBox*>(element);
        break;
      case NodeType::TrimPath:
        hasTrim = true;
        curTrim = static_cast<const TrimPath*>(element);
        break;
      case NodeType::RoundCorner:
        break;
      case NodeType::MergePath:
        hasMerge = true;
        break;
      case NodeType::TextModifier:
      case NodeType::TextPath:
        break;
      case NodeType::Group:
        writeGroup(out, static_cast<const Group*>(element), alpha, distribute);
        break;
      case NodeType::Repeater: {
        auto rep = static_cast<const Repeater*>(element);
        writeRepeater(out, rep, geos, curFill, curStroke, distribute ? alpha : 1.0f);
        geos.clear();
        curFill = nullptr;
        curStroke = nullptr;
        hasTrim = false;
        curTrim = nullptr;
        hasMerge = false;
        break;
      }
      default:
        break;
    }
  }
}

void HTMLWriter::writeLayerContents(HTMLBuilder& out, const Layer* layer, float alpha,
                                    bool distribute) {
  writeElements(out, layer->contents, alpha, distribute);
}

//==============================================================================
// HTMLWriter – layer
//==============================================================================

void HTMLWriter::writeLayer(HTMLBuilder& out, const Layer* layer, float /*parentAlpha*/,
                            bool /*distributeAlpha*/) {
  if (!layer->visible && layer->mask == nullptr) {
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

  if (!layer->matrix3D.isIdentity() || layer->preserve3D) {
    style += ";transform-style:preserve-3d";
  }

  bool groupOp = layer->groupOpacity;
  float layerAlpha = layer->alpha;
  bool childDistribute = !groupOp && layerAlpha < 1.0f;

  if (groupOp && layerAlpha < 1.0f) {
    style += ";opacity:" + FloatToString(layerAlpha);
  }

  if (layer->blendMode != BlendMode::Normal) {
    auto blendStr = BlendModeToSVGString(layer->blendMode);
    if (blendStr) {
      style += ";mix-blend-mode:";
      style += blendStr;
    } else if (layer->blendMode == BlendMode::PlusLighter) {
      style += ";mix-blend-mode:plus-lighter";
    }
  }

  if (layer->hasScrollRect) {
    style += ";clip-path:inset(" + FloatToString(layer->scrollRect.y) + "px " +
             FloatToString(layer->scrollRect.x) + "px)";
    style += ";overflow:hidden";
  }

  // Filters
  std::string filterCSS;
  if (!layer->filters.empty()) {
    filterCSS = writeFilterDefs(layer->filters);
    if (!filterCSS.empty()) {
      style += ";filter:" + filterCSS;
    }
  }

  // Styles (drop shadow, etc.)
  for (auto* ls : layer->styles) {
    if (ls->nodeType() == NodeType::DropShadowStyle) {
      auto ds = static_cast<const DropShadowStyle*>(ls);
      if (ds->blurX == ds->blurY) {
        style += ";filter:drop-shadow(" + FloatToString(ds->offsetX) + "px " +
                 FloatToString(ds->offsetY) + "px " + FloatToString(ds->blurX) + "px " +
                 ColorToRGBA(ds->color) + ")";
      }
    }
  }

  // Mask
  if (layer->mask != nullptr) {
    if (layer->maskType == MaskType::Contour) {
      auto clipId = writeClipDef(layer->mask);
      style += ";clip-path:url(#" + clipId + ")";
    } else {
      auto maskId = writeMaskDef(layer->mask, layer->maskType);
      style += ";mask:url(#" + maskId + ")";
      style += ";-webkit-mask:url(#" + maskId + ")";
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

  // Background blur styles
  for (auto* ls : layer->styles) {
    if (ls->nodeType() == NodeType::BackgroundBlurStyle) {
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

  float contentAlpha = childDistribute ? layerAlpha : 1.0f;
  writeLayerContents(out, layer, contentAlpha, childDistribute);

  if (layer->composition) {
    writeComposition(out, layer->composition);
  }

  for (auto* child : layer->children) {
    writeLayer(out, child, contentAlpha, childDistribute);
  }

  out.closeTag();
}

//==============================================================================
// Main export function
//==============================================================================

std::string HTMLExporter::ToHTML(const PAGXDocument& doc, const Options& options) {
  HTMLBuilder html(options.indent);
  HTMLBuilder defs(options.indent, 2);
  HTMLWriterContext ctx;
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

  return html.release();
}

bool HTMLExporter::ToFile(const PAGXDocument& document, const std::string& filePath,
                          const Options& options) {
  auto html = ToHTML(document, options);
  if (html.empty()) {
    return false;
  }
  auto dirPath = std::filesystem::path(filePath).parent_path();
  if (!std::filesystem::exists(dirPath)) {
    std::filesystem::create_directories(dirPath);
  }
  std::ofstream file(filePath, std::ios::binary);
  if (!file) {
    return false;
  }
  file.write(html.data(), static_cast<std::streamsize>(html.size()));
  return file.good();
}

}  // namespace pagx
