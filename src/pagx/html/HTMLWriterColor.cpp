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

#include <algorithm>
#include <cmath>
#include "base/utils/MathUtil.h"
#include "pagx/html/HTMLWriter.h"
#include "pagx/nodes/ColorStop.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/DiamondGradient.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/utils/StringParser.h"

namespace pagx {

using pag::FloatNearlyZero;

// Maps a point from a Gradient's own coordinate space to the PAGX pixel coordinate space
// expected by the rest of colorToCSS(). When `fitsToGeometry` is true (the default since tgfx
// adopted relative-fill defaults), the gradient parameters live in the geometry's normalized
// (0, 0)-(1, 1) bounding box; multiply by the box size and offset by its top-left to recover
// pixel coordinates. When `fitsToGeometry` is false the parameters are already pixel values
// (in the parent container's coordinate space), so return them unchanged.
static Point MapGradientPoint(const Point& p, bool fitsToGeometry, float boxLeft, float boxTop,
                              float boxWidth, float boxHeight) {
  if (!fitsToGeometry || boxWidth <= 0 || boxHeight <= 0) {
    return p;
  }
  return {p.x * boxWidth + boxLeft, p.y * boxHeight + boxTop};
}

static bool ResolveUniformGradientColor(const Gradient* gradient, Color* color) {
  if (!gradient || gradient->colorStops.empty() || !gradient->colorStops.front()) {
    return false;
  }
  auto firstColor = gradient->colorStops.front()->color;
  for (auto* stop : gradient->colorStops) {
    if (!stop || stop->color != firstColor) {
      return false;
    }
  }
  if (color) {
    *color = firstColor;
  }
  return true;
}

std::string HTMLWriter::buildLinearGradientCSS(const LinearGradient* g, float boxLeft, float boxTop,
                                               float boxWidth, float boxHeight) {
  if (!g->matrix.isIdentity()) {
    bool hasTranslation = !FloatNearlyZero(g->matrix.tx) || !FloatNearlyZero(g->matrix.ty);
    float sx = std::sqrt(g->matrix.a * g->matrix.a + g->matrix.b * g->matrix.b);
    float sy = std::sqrt(g->matrix.c * g->matrix.c + g->matrix.d * g->matrix.d);
    bool hasScale = !FloatNearlyZero(sx - 1.0f) || !FloatNearlyZero(sy - 1.0f);
    if (hasTranslation || hasScale) {
      return {};
    }
  }
  Point s = MapGradientPoint(g->matrix.mapPoint(g->startPoint), g->fitsToGeometry, boxLeft, boxTop,
                             boxWidth, boxHeight);
  Point e = MapGradientPoint(g->matrix.mapPoint(g->endPoint), g->fitsToGeometry, boxLeft, boxTop,
                             boxWidth, boxHeight);
  float ang = std::atan2(e.y - s.y, e.x - s.x) * 180.0f / static_cast<float>(M_PI) + 90.0f;

  if (boxWidth > 0 && boxHeight > 0 && !g->colorStops.empty()) {
    float angleRad = ang * static_cast<float>(M_PI) / 180.0f;
    float dirX = std::sin(angleRad);
    float dirY = -std::cos(angleRad);
    float lineLength = std::abs(boxWidth * dirX) + std::abs(boxHeight * dirY);
    if (lineLength > 0) {
      float sxLocal = s.x - boxLeft;
      float syLocal = s.y - boxTop;
      float exLocal = e.x - boxLeft;
      float eyLocal = e.y - boxTop;
      float cx = boxWidth * 0.5f;
      float cy = boxHeight * 0.5f;
      float lineStartX = cx - dirX * lineLength * 0.5f;
      float lineStartY = cy - dirY * lineLength * 0.5f;
      float startT = ((sxLocal - lineStartX) * dirX + (syLocal - lineStartY) * dirY) / lineLength;
      float endT = ((exLocal - lineStartX) * dirX + (eyLocal - lineStartY) * dirY) / lineLength;
      constexpr float GRADIENT_MAPPING_EPSILON = 1e-4f;
      if (std::abs(endT - startT) > GRADIENT_MAPPING_EPSILON) {
        auto* firstStop = g->colorStops.front();
        auto* lastStop = g->colorStops.back();
        if (!firstStop || !lastStop) {
          return "linear-gradient(" + CssFloatToString(ang) + "deg," + CSSStops(g->colorStops) +
                 ")";
        }
        std::string stops;
        size_t firstIndex = 0;
        if (startT > 0.0f) {
          stops += ColorToRGBA(firstStop->color) + " 0%," + ColorToRGBA(firstStop->color) + " " +
                   CssFloatToString(startT * 100.0f) + "%";
          float mapped0 = startT + g->colorStops[0]->offset * (endT - startT);
          if (std::abs(mapped0 - startT) <= GRADIENT_MAPPING_EPSILON) {
            firstIndex = 1;
          }
        }
        for (size_t i = firstIndex; i < g->colorStops.size(); i++) {
          if (!g->colorStops[i]) {
            continue;
          }
          float mapped = startT + g->colorStops[i]->offset * (endT - startT);
          if (!stops.empty()) {
            stops += ",";
          }
          stops +=
              ColorToRGBA(g->colorStops[i]->color) + " " + CssFloatToString(mapped * 100.0f) + "%";
        }
        if (endT < 1.0f) {
          float mappedLast = startT + g->colorStops.back()->offset * (endT - startT);
          if (std::abs(mappedLast - endT) > GRADIENT_MAPPING_EPSILON) {
            stops +=
                "," + ColorToRGBA(lastStop->color) + " " + CssFloatToString(endT * 100.0f) + "%";
          }
          stops += "," + ColorToRGBA(lastStop->color) + " 100%";
        }
        return "linear-gradient(" + CssFloatToString(ang) + "deg," + stops + ")";
      }
    }
  }
  return "linear-gradient(" + CssFloatToString(ang) + "deg," + CSSStops(g->colorStops) + ")";
}

std::string HTMLWriter::buildRadialGradientCSS(const RadialGradient* g, float boxLeft, float boxTop,
                                               float boxWidth, float boxHeight) {
  float sx = std::sqrt(g->matrix.a * g->matrix.a + g->matrix.b * g->matrix.b);
  float sy = std::sqrt(g->matrix.c * g->matrix.c + g->matrix.d * g->matrix.d);
  bool hasRotation = !FloatNearlyZero(g->matrix.b) || !FloatNearlyZero(g->matrix.c);
  bool nonUniformScale = !FloatNearlyZero(sx - sy);
  if (hasRotation && nonUniformScale) {
    return {};
  }
  Point c = MapGradientPoint(g->matrix.mapPoint(g->center), g->fitsToGeometry, boxLeft, boxTop,
                             boxWidth, boxHeight);
  c.x -= boxLeft;
  c.y -= boxTop;
  float rx = g->radius * sx;
  float ry = g->radius * (nonUniformScale ? sy : sx);
  if (g->fitsToGeometry && boxWidth > 0 && boxHeight > 0) {
    rx = g->radius * boxWidth * sx;
    ry = g->radius * boxHeight * (nonUniformScale ? sy : sx);
  }
  if (nonUniformScale || (g->fitsToGeometry && boxWidth > 0 && boxHeight > 0 &&
                          !FloatNearlyZero(boxWidth - boxHeight))) {
    return "radial-gradient(ellipse " + CssFloatToString(rx) + "px " + CssFloatToString(ry) +
           "px at " + CssFloatToString(c.x) + "px " + CssFloatToString(c.y) + "px," +
           CSSStops(g->colorStops) + ")";
  }
  return "radial-gradient(circle " + CssFloatToString(rx) + "px at " + CssFloatToString(c.x) +
         "px " + CssFloatToString(c.y) + "px," + CSSStops(g->colorStops) + ")";
}

std::string HTMLWriter::buildConicGradientCSS(const ConicGradient* g, float boxLeft, float boxTop,
                                              float boxWidth, float boxHeight) {
  float matRotation = 0.0f;
  if (!g->matrix.isIdentity()) {
    float sx = std::sqrt(g->matrix.a * g->matrix.a + g->matrix.b * g->matrix.b);
    float sy = std::sqrt(g->matrix.c * g->matrix.c + g->matrix.d * g->matrix.d);
    bool hasNonUniformScale = !FloatNearlyZero(sx - 1.0f) || !FloatNearlyZero(sy - 1.0f);
    if (hasNonUniformScale) {
      return {};
    }
    matRotation = std::atan2(g->matrix.b, g->matrix.a) * 180.0f / static_cast<float>(M_PI);
  }
  Point c = MapGradientPoint(g->matrix.mapPoint(g->center), g->fitsToGeometry, boxLeft, boxTop,
                             boxWidth, boxHeight);
  c.x -= boxLeft;
  c.y -= boxTop;
  float cssStartAng = g->startAngle + 90.0f + matRotation;
  float sweepRange = g->endAngle - g->startAngle;
  if (FloatNearlyZero(sweepRange - 360.0f)) {
    return "conic-gradient(from " + CssFloatToString(cssStartAng) + "deg at " +
           CssFloatToString(c.x) + "px " + CssFloatToString(c.y) + "px," + CSSStops(g->colorStops) +
           ")";
  }
  float cssOrigin = 90.0f + matRotation;
  float cssEndAng = g->endAngle + 90.0f + matRotation;
  std::string firstColor = "transparent";
  std::string lastColor = "transparent";
  if (!g->colorStops.empty() && g->colorStops.front()) {
    firstColor = ColorToRGBA(g->colorStops.front()->color);
  }
  if (!g->colorStops.empty() && g->colorStops.back()) {
    lastColor = ColorToRGBA(g->colorStops.back()->color);
  }
  std::string stops;
  stops += lastColor + " " + CssFloatToString(cssOrigin) + "deg";
  stops += "," + firstColor + " " + CssFloatToString(cssOrigin) + "deg";
  stops += "," + firstColor + " " + CssFloatToString(cssStartAng) + "deg";
  for (size_t i = 0; i < g->colorStops.size(); i++) {
    if (!g->colorStops[i]) {
      continue;
    }
    bool isFirst = (i == 0) && FloatNearlyZero(g->colorStops[i]->offset);
    bool isLast =
        (i == g->colorStops.size() - 1) && FloatNearlyZero(g->colorStops[i]->offset - 1.0f);
    if (isFirst || isLast) {
      continue;
    }
    stops += ',';
    stops += ColorToRGBA(g->colorStops[i]->color);
    float angle = cssStartAng + g->colorStops[i]->offset * sweepRange;
    stops += ' ' + CssFloatToString(angle) + "deg";
  }
  stops += "," + lastColor + " " + CssFloatToString(cssEndAng) + "deg";
  return "conic-gradient(at " + CssFloatToString(c.x) + "px " + CssFloatToString(c.y) + "px," +
         stops + ")";
}

std::string HTMLWriter::buildDiamondGradientCSS(const DiamondGradient* g, float boxLeft,
                                                float boxTop, float boxWidth, float boxHeight) {
  Point c = MapGradientPoint(g->matrix.mapPoint(g->center), g->fitsToGeometry, boxLeft, boxTop,
                             boxWidth, boxHeight);
  c.x -= boxLeft;
  c.y -= boxTop;
  float sx = std::sqrt(g->matrix.a * g->matrix.a + g->matrix.b * g->matrix.b);
  float rx = g->radius * sx;
  float ry = rx;
  if (g->fitsToGeometry && boxWidth > 0 && boxHeight > 0) {
    rx = g->radius * boxWidth * sx;
    ry = g->radius * boxHeight * sx;
  }
  if (!FloatNearlyZero(rx - ry)) {
    return "radial-gradient(ellipse " + CssFloatToString(rx) + "px " + CssFloatToString(ry) +
           "px at " + CssFloatToString(c.x) + "px " + CssFloatToString(c.y) + "px," +
           CSSStops(g->colorStops) + ")";
  }
  return "radial-gradient(circle " + CssFloatToString(rx) + "px at " + CssFloatToString(c.x) +
         "px " + CssFloatToString(c.y) + "px," + CSSStops(g->colorStops) + ")";
}

std::string HTMLWriter::colorToCSS(const ColorSource* src, float* outAlpha, float boxLeft,
                                   float boxTop, float boxWidth, float boxHeight) {
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
      auto gradient = static_cast<const LinearGradient*>(src);
      auto css = buildLinearGradientCSS(gradient, boxLeft, boxTop, boxWidth, boxHeight);
      if (!css.empty()) {
        if (outAlpha) {
          *outAlpha = 1.0f;
        }
        return css;
      }
      Color color;
      if (ResolveUniformGradientColor(gradient, &color)) {
        if (outAlpha) {
          *outAlpha = color.alpha;
        }
        return ColorToHex(color);
      }
      if (outAlpha) {
        *outAlpha = 1.0f;
      }
      return {};
    }
    case NodeType::RadialGradient: {
      if (outAlpha) {
        *outAlpha = 1.0f;
      }
      return buildRadialGradientCSS(static_cast<const RadialGradient*>(src), boxLeft, boxTop,
                                    boxWidth, boxHeight);
    }
    case NodeType::ConicGradient: {
      if (outAlpha) {
        *outAlpha = 1.0f;
      }
      return buildConicGradientCSS(static_cast<const ConicGradient*>(src), boxLeft, boxTop,
                                   boxWidth, boxHeight);
    }
    case NodeType::ImagePattern: {
      auto p = static_cast<const ImagePattern*>(src);
      if (outAlpha) {
        *outAlpha = 1.0f;
      }
      if (!p->image) {
        return "transparent";
      }
      return "url(" + GetImageSrc(p->image, _ctx) + ")";
    }
    case NodeType::DiamondGradient: {
      if (outAlpha) {
        *outAlpha = 1.0f;
      }
      return buildDiamondGradientCSS(static_cast<const DiamondGradient*>(src), boxLeft, boxTop,
                                     boxWidth, boxHeight);
    }
    default:
      if (outAlpha) {
        *outAlpha = 1.0f;
      }
      return "transparent";
  }
}

std::string HTMLWriter::colorToSVGFill(const ColorSource* src, float* outAlpha, float bboxX,
                                       float bboxY, float bboxW, float bboxH) {
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
    // Use SVG pattern + foreignObject to embed CSS conic-gradient.
    // patternUnits/patternContentUnits=objectBoundingBox cannot be used here: Chromium
    // interprets foreignObject width/height in CSS px (the HTML coordinate space), not in
    // objectBoundingBox fractions, so `width="1" height="1"` produces a 1×1 px tile and the
    // gradient is invisible. Instead use userSpaceOnUse with the caller-supplied bounding box
    // so the foreignObject maps exactly onto the geometry's pixel extent.
    auto g = static_cast<const ConicGradient*>(src);
    std::string id = _ctx->nextId("cpat");
    // Fall back to a generous tile when no bbox was supplied (caller didn't know the geometry).
    float fw = (bboxW > 0) ? bboxW : 1000.0f;
    float fh = (bboxH > 0) ? bboxH : 1000.0f;
    float fx = (bboxW > 0) ? bboxX : -500.0f;
    float fy = (bboxH > 0) ? bboxY : -500.0f;
    _defs->openTag("pattern");
    _defs->addAttr("id", id);
    _defs->addAttr("patternUnits", "userSpaceOnUse");
    _defs->addAttr("x", CssFloatToString(fx));
    _defs->addAttr("y", CssFloatToString(fy));
    _defs->addAttr("width", CssFloatToString(fw));
    _defs->addAttr("height", CssFloatToString(fh));
    _defs->closeTagStart();
    _defs->openTag("foreignObject");
    _defs->addAttr("x", CssFloatToString(fx));
    _defs->addAttr("y", CssFloatToString(fy));
    _defs->addAttr("width", CssFloatToString(fw));
    _defs->addAttr("height", CssFloatToString(fh));
    _defs->closeTagStart();
    // Extract rotation angle from the matrix and add it to the CSS start angle.
    float matRotation = std::atan2(g->matrix.b, g->matrix.a) * 180.0f / static_cast<float>(M_PI);
    Point c = g->center;
    // When fitsToGeometry, the PAGX centre is already in normalised 0..1 space relative to the
    // geometry's bounding box — emit it as a percentage so it maps to the same relative point
    // inside the foreignObject regardless of the geometry's pixel extent. When fitsToGeometry
    // is false, c is in absolute document-space pixels; subtract the foreignObject's top-left
    // corner (fx, fy) to convert to a foreignObject-local pixel offset.
    std::string cxStr, cyStr;
    if (g->fitsToGeometry) {
      cxStr = CssFloatToString(c.x * 100.0f) + "%";
      cyStr = CssFloatToString(c.y * 100.0f) + "%";
    } else {
      cxStr = CssFloatToString(c.x - fx) + "px";
      cyStr = CssFloatToString(c.y - fy) + "px";
    }
    float cssStartAng = g->startAngle + 90.0f + matRotation;
    float sweepRange = g->endAngle - g->startAngle;
    std::string cssGrad;
    if (FloatNearlyZero(sweepRange - 360.0f)) {
      cssGrad = "conic-gradient(from " + CssFloatToString(cssStartAng) + "deg at " + cxStr + " " +
                cyStr + "," + CSSStops(g->colorStops) + ")";
    } else {
      std::string stops;
      bool first = true;
      for (size_t i = 0; i < g->colorStops.size(); i++) {
        if (!g->colorStops[i]) {
          continue;
        }
        if (!first) {
          stops += ',';
        }
        first = false;
        stops += ColorToRGBA(g->colorStops[i]->color);
        float angle = cssStartAng + g->colorStops[i]->offset * sweepRange;
        stops += ' ' + CssFloatToString(angle) + "deg";
      }
      cssGrad = "conic-gradient(at " + cxStr + " " + cyStr + "," + stops + ")";
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
    _defs->addAttr("cx", CssFloatToString(g->center.x));
    _defs->addAttr("cy", CssFloatToString(g->center.y));
    _defs->addAttr("r", CssFloatToString(g->radius));
    _defs->addAttr("gradientUnits", g->fitsToGeometry ? "objectBoundingBox" : "userSpaceOnUse");
    if (!g->matrix.isIdentity()) {
      _defs->addAttr("gradientTransform", MatrixToCSS(g->matrix));
    }
    _defs->closeTagStart();
    for (auto* stop : g->colorStops) {
      if (!stop) {
        continue;
      }
      _defs->openTag("stop");
      _defs->addAttr("offset", CssFloatToString(stop->offset));
      _defs->addAttr("stop-color", ColorToSVGHex(stop->color));
      if (stop->color.alpha < 1.0f) {
        _defs->addAttr("stop-opacity", CssFloatToString(stop->color.alpha));
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
    std::string imgUrl = "url(" + GetImageSrc(p->image, _ctx) + ")";
    std::string repeatX = "no-repeat";
    std::string repeatY = "no-repeat";
    if (p->tileModeX == TileMode::Repeat || p->tileModeX == TileMode::Mirror) {
      repeatX = "repeat";
    }
    if (p->tileModeY == TileMode::Repeat || p->tileModeY == TileMode::Mirror) {
      repeatY = "repeat";
    }
    std::string bgRepeat = (repeatX == repeatY) ? repeatX : (repeatX + " " + repeatY);
    std::string bgSize;
    std::string bgPos;
    if (!p->matrix.isIdentity()) {
      float sx = std::sqrt(p->matrix.a * p->matrix.a + p->matrix.b * p->matrix.b);
      float sy = std::sqrt(p->matrix.c * p->matrix.c + p->matrix.d * p->matrix.d);
      if (!FloatNearlyZero(sx - 1.0f) || !FloatNearlyZero(sy - 1.0f)) {
        bgSize = CssFloatToString(sx * 100.0f) + "% " + CssFloatToString(sy * 100.0f) + "%";
      }
      if (!FloatNearlyZero(p->matrix.tx) || !FloatNearlyZero(p->matrix.ty)) {
        bgPos = CssFloatToString(p->matrix.tx) + "px " + CssFloatToString(p->matrix.ty) + "px";
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
    std::string patternSize =
        CssFloatToString(std::max({_ctx->docWidth, _ctx->docHeight, 10000.0f}));
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

void HTMLWriter::writeSVGGradientDef(const ColorSource* src, const std::string& id, float bboxX,
                                     float bboxY, float bboxW, float bboxH) {
  writeSVGGradientDefInto(*_defs, src, id, bboxX, bboxY, bboxW, bboxH);
}

void HTMLWriter::writeSVGGradientDefInto(HTMLBuilder& builder, const ColorSource* src,
                                         const std::string& id, float bboxX, float bboxY,
                                         float bboxW, float bboxH) {
  // When fitsToGeometry=true and a valid bounding box is supplied, convert the normalised
  // [0,1] gradient coordinates to userSpaceOnUse pixel coordinates. This is required when the
  // gradient fill is shared across multiple SVG <path> elements (e.g. GlyphRun glyphs) because
  // SVG applies objectBoundingBox independently to each element, making every glyph produce its
  // own gradient instead of a single sweep across the whole text block.
  bool resolveToUserSpace = bboxW > 0 && bboxH > 0;
  if (src->nodeType() == NodeType::LinearGradient) {
    auto g = static_cast<const LinearGradient*>(src);
    builder.openTag("linearGradient");
    builder.addAttr("id", id);
    if (g->fitsToGeometry && resolveToUserSpace) {
      // Map normalised [0,1] coords into the caller-supplied pixel bounding box.
      builder.addAttr("x1", CssFloatToString(bboxX + g->startPoint.x * bboxW));
      builder.addAttr("y1", CssFloatToString(bboxY + g->startPoint.y * bboxH));
      builder.addAttr("x2", CssFloatToString(bboxX + g->endPoint.x * bboxW));
      builder.addAttr("y2", CssFloatToString(bboxY + g->endPoint.y * bboxH));
      builder.addAttr("gradientUnits", "userSpaceOnUse");
    } else {
      builder.addAttr("x1", CssFloatToString(g->startPoint.x));
      builder.addAttr("y1", CssFloatToString(g->startPoint.y));
      builder.addAttr("x2", CssFloatToString(g->endPoint.x));
      builder.addAttr("y2", CssFloatToString(g->endPoint.y));
      // fitsToGeometry maps to SVG objectBoundingBox (coords in [0,1] of the filled shape's
      // bbox); fitsToGeometry=false keeps the legacy userSpaceOnUse semantics for pixel-space
      // gradient parameters shared across multiple geometries.
      builder.addAttr("gradientUnits", g->fitsToGeometry ? "objectBoundingBox" : "userSpaceOnUse");
    }
    if (!g->matrix.isIdentity()) {
      builder.addAttr("gradientTransform", MatrixToCSS(g->matrix));
    }
    builder.closeTagStart();
    for (auto* stop : g->colorStops) {
      if (!stop) {
        continue;
      }
      builder.openTag("stop");
      builder.addAttr("offset", CssFloatToString(stop->offset));
      builder.addAttr("stop-color", ColorToSVGHex(stop->color));
      if (stop->color.alpha < 1.0f) {
        builder.addAttr("stop-opacity", CssFloatToString(stop->color.alpha));
      }
      builder.closeTagSelfClosing();
    }
    builder.closeTag();
  } else if (src->nodeType() == NodeType::RadialGradient) {
    auto g = static_cast<const RadialGradient*>(src);
    builder.openTag("radialGradient");
    builder.addAttr("id", id);
    if (g->fitsToGeometry && resolveToUserSpace) {
      // Map normalised centre and radius into pixel space.
      builder.addAttr("cx", CssFloatToString(bboxX + g->center.x * bboxW));
      builder.addAttr("cy", CssFloatToString(bboxY + g->center.y * bboxH));
      builder.addAttr("r", CssFloatToString(g->radius * std::min(bboxW, bboxH)));
      builder.addAttr("gradientUnits", "userSpaceOnUse");
    } else {
      builder.addAttr("cx", CssFloatToString(g->center.x));
      builder.addAttr("cy", CssFloatToString(g->center.y));
      builder.addAttr("r", CssFloatToString(g->radius));
      builder.addAttr("gradientUnits", g->fitsToGeometry ? "objectBoundingBox" : "userSpaceOnUse");
    }
    if (!g->matrix.isIdentity()) {
      builder.addAttr("gradientTransform", MatrixToCSS(g->matrix));
    }
    builder.closeTagStart();
    for (auto* stop : g->colorStops) {
      if (!stop) {
        continue;
      }
      builder.openTag("stop");
      builder.addAttr("offset", CssFloatToString(stop->offset));
      builder.addAttr("stop-color", ColorToSVGHex(stop->color));
      if (stop->color.alpha < 1.0f) {
        builder.addAttr("stop-opacity", CssFloatToString(stop->color.alpha));
      }
      builder.closeTagSelfClosing();
    }
    builder.closeTag();
  }
}

}  // namespace pagx
