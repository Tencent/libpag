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
      // Partial sweep: use absolute CSS angles (PAGX angle + 90°) without 'from' keyword.
      // Native clamp behavior (PAGX coordinate, 0°=right):
      //   0° → startAngle: first stop color (clamp t=0)
      //   startAngle → endAngle: gradient
      //   endAngle → 360°: last stop color (clamp t=1)
      // CSS 0° is top (12 o'clock), so CSS 0°→90° (12→3 o'clock) maps to PAGX 270°→360°,
      // which is in the endAngle→360° clamp region, filled with lastColor.
      float cssOrigin = 90.0f + matRotation;
      float cssEndAng = g->endAngle + 90.0f + matRotation;
      std::string firstColor =
          g->colorStops.empty() ? "transparent" : ColorToRGBA(g->colorStops.front()->color);
      std::string lastColor =
          g->colorStops.empty() ? "transparent" : ColorToRGBA(g->colorStops.back()->color);
      std::string stops;
      stops += lastColor + " " + FloatToString(cssOrigin) + "deg";
      stops += "," + firstColor + " " + FloatToString(cssOrigin) + "deg";
      stops += "," + firstColor + " " + FloatToString(cssStartAng) + "deg";
      for (size_t i = 0; i < g->colorStops.size(); i++) {
        stops += ',';
        stops += ColorToRGBA(g->colorStops[i]->color);
        float angle = cssStartAng + g->colorStops[i]->offset * sweepRange;
        stops += ' ' + FloatToString(angle) + "deg";
      }
      stops += "," + lastColor + " " + FloatToString(cssEndAng) + "deg";
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
    std::string bgSize;
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

}  // namespace pagx
