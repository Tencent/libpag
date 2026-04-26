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

      // CSS linear-gradient's 0% and 100% anchors sit on the gradient line drawn through the
      // box centre; the line's length L = |w * sin(angle)| + |h * cos(angle)|. PAGX's
      // startPoint/endPoint live in the parent Layer's coordinate space, but the element
      // drawing the background (a Rectangle div) usually has a non-zero top-left offset
      // within that space. Translate start/end into the element's local box coordinates
      // before projecting so the stops align correctly. When the box size is unknown, fall
      // back to the plain angle form — still wrong geometrically but matches legacy output
      // for callers that cannot provide a size (e.g. SVG path fills).
      if (boxWidth > 0 && boxHeight > 0 && !g->colorStops.empty()) {
        float angleRad = ang * static_cast<float>(M_PI) / 180.0f;
        // CSS gradient direction: 0deg points up, 90deg points right. The unit vector
        // along the gradient line (pointing from 0% to 100%) is (sin(a), -cos(a)).
        float dirX = std::sin(angleRad);
        float dirY = -std::cos(angleRad);
        float lineLength = std::abs(boxWidth * dirX) + std::abs(boxHeight * dirY);
        if (lineLength > 0) {
          // Shift PAGX points into the element's local box coordinates (where the CSS
          // gradient line is defined). The box's top-left is (boxLeft, boxTop) in the
          // PAGX coordinate system.
          float sxLocal = s.x - boxLeft;
          float syLocal = s.y - boxTop;
          float exLocal = e.x - boxLeft;
          float eyLocal = e.y - boxTop;
          float cx = boxWidth * 0.5f;
          float cy = boxHeight * 0.5f;
          // 0% end of the CSS gradient line in element-local coords.
          float lineStartX = cx - dirX * lineLength * 0.5f;
          float lineStartY = cy - dirY * lineLength * 0.5f;
          // Project PAGX start/end points onto the line and normalise to [0, 1].
          float startT =
              ((sxLocal - lineStartX) * dirX + (syLocal - lineStartY) * dirY) / lineLength;
          float endT = ((exLocal - lineStartX) * dirX + (eyLocal - lineStartY) * dirY) / lineLength;
          // Epsilon for comparing gradient-line parameter values. Stops whose mapped parameter
          // positions differ by less than this are treated as coincident so duplicate stops do
          // not appear in the emitted CSS `linear-gradient(...)` declaration.
          constexpr float GRADIENT_MAPPING_EPSILON = 1e-4f;
          if (std::abs(endT - startT) > GRADIENT_MAPPING_EPSILON) {
            auto* firstStop = g->colorStops.front();
            auto* lastStop = g->colorStops.back();
            if (!firstStop || !lastStop) {
              return "linear-gradient(" + FloatToString(ang) + "deg," + CSSStops(g->colorStops) +
                     ")";
            }
            std::string stops;
            // Leading clamp region: 0%..startT filled with the first stop colour.
            // When the first stop's offset is 0, it maps to startT and duplicates the clamp
            // endpoint — skip it to avoid redundant stops.
            size_t firstIndex = 0;
            if (startT > 0.0f) {
              stops += ColorToRGBA(firstStop->color) + " 0%," + ColorToRGBA(firstStop->color) +
                       " " + FloatToString(startT * 100.0f) + "%";
              // When the first stop's offset is 0, it maps to startT and duplicates the clamp
              // endpoint — start the loop from index 1 to skip it.
              float mapped0 = startT + g->colorStops[0]->offset * (endT - startT);
              if (std::abs(mapped0 - startT) <= GRADIENT_MAPPING_EPSILON) {
                firstIndex = 1;
              }
            }
            // Re-emit stops mapped onto [startT, endT].
            for (size_t i = firstIndex; i < g->colorStops.size(); i++) {
              if (!g->colorStops[i]) {
                continue;
              }
              float mapped = startT + g->colorStops[i]->offset * (endT - startT);
              if (!stops.empty()) {
                stops += ",";
              }
              stops +=
                  ColorToRGBA(g->colorStops[i]->color) + " " + FloatToString(mapped * 100.0f) + "%";
            }
            // Trailing clamp region: endT..100% filled with the last stop colour.
            // When the last stop's offset is 1, it maps to endT and duplicates the clamp
            // start point — skip the endT stop.
            if (endT < 1.0f) {
              float mappedLast = startT + g->colorStops.back()->offset * (endT - startT);
              if (std::abs(mappedLast - endT) > GRADIENT_MAPPING_EPSILON) {
                stops +=
                    "," + ColorToRGBA(lastStop->color) + " " + FloatToString(endT * 100.0f) + "%";
              }
              stops += "," + ColorToRGBA(lastStop->color) + " 100%";
            }
            return "linear-gradient(" + FloatToString(ang) + "deg," + stops + ")";
          }
        }
      }

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
      // PAGX RadialGradient.center lives in the enclosing layer's local coordinate space (same
      // space as the shape being painted). tgfx samples the gradient directly at those layer
      // coordinates. CSS `radial-gradient(... at X Y)` however interprets X/Y relative to the
      // element's own top-left corner. Translate the gradient centre into element-local space
      // by subtracting the box's top-left offset, otherwise a gradient declared at the layer
      // origin stays stuck in the top-left corner of every div that uses it (e.g. mandala's
      // tealGlow has center="50,50" but is painted inside an ellipse positioned around (180,180)
      // — tgfx renders it as fully-transparent outer stop, but naive CSS would draw it centred
      // on (50,50) within the div, introducing a visible teal glow the native render lacks).
      Point c = g->matrix.mapPoint(g->center);
      c.x -= boxLeft;
      c.y -= boxTop;
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
      std::string firstColor = "transparent";
      std::string lastColor = "transparent";
      if (!g->colorStops.empty() && g->colorStops.front()) {
        firstColor = ColorToRGBA(g->colorStops.front()->color);
      }
      if (!g->colorStops.empty() && g->colorStops.back()) {
        lastColor = ColorToRGBA(g->colorStops.back()->color);
      }
      std::string stops;
      stops += lastColor + " " + FloatToString(cssOrigin) + "deg";
      stops += "," + firstColor + " " + FloatToString(cssOrigin) + "deg";
      stops += "," + firstColor + " " + FloatToString(cssStartAng) + "deg";
      // Skip the first stop if offset==0 (it maps to cssStartAng, already emitted above)
      // and the last stop if offset==1 (it maps to cssEndAng, emitted below).
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
      c.x -= boxLeft;
      c.y -= boxTop;
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
      if (!stop) {
        continue;
      }
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
      if (!stop) {
        continue;
      }
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
      if (!stop) {
        continue;
      }
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
