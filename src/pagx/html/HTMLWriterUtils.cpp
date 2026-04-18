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
#include <cstdio>
#include <string>
#include "base/utils/MathUtil.h"
#include "pagx/html/HTMLWriter.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/svg/SVGBlendMode.h"
#include "pagx/types/Alignment.h"
#include "pagx/types/Arrangement.h"
#include "pagx/types/Padding.h"
#include "pagx/utils/Base64.h"
#include "pagx/utils/StringParser.h"

namespace pagx {

using pag::DegreesToRadians;
using pag::FloatNearlyZero;

void ColorToRGB(const Color& color, int& r, int& g, int& b) {
  r = std::clamp(static_cast<int>(std::round(color.red * 255.0f)), 0, 255);
  g = std::clamp(static_cast<int>(std::round(color.green * 255.0f)), 0, 255);
  b = std::clamp(static_cast<int>(std::round(color.blue * 255.0f)), 0, 255);
}

std::string ColorToHex(const Color& color) {
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
std::string ColorToSVGHex(const Color& color) {
  int r = 0, g = 0, b = 0;
  ColorToRGB(color, r, g, b);
  char buf[8] = {};
  snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
  return buf;
}

std::string ColorToRGBA(const Color& color, float extra) {
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

const char* DetectImageMime(const uint8_t* bytes, size_t size) {
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

std::string EscapeCSSUrl(const std::string& url) {
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

std::string GetImageSrc(const Image* image) {
  if (image->data) {
    auto mime = DetectImageMime(image->data->bytes(), image->data->size());
    return "data:" + std::string(mime) + ";base64," +
           Base64Encode(image->data->bytes(), image->data->size());
  }
  return EscapeCSSUrl(image->filePath);
}

std::string CSSStops(const std::vector<ColorStop*>& stops) {
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

std::string MatrixToCSS(const Matrix& m) {
  std::string r = "matrix(";
  r += FloatToString(m.a) + ',' + FloatToString(m.b) + ',' + FloatToString(m.c) + ',' +
       FloatToString(m.d) + ',' + FloatToString(m.tx) + ',' + FloatToString(m.ty) + ')';
  return r;
}

std::string Matrix3DToCSS(const Matrix3D& m) {
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

const char* BlendModeToMixBlendMode(BlendMode mode) {
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

Color LerpColor(const Color& a, const Color& b, float t) {
  Color result = {};
  result.red = a.red + (b.red - a.red) * t;
  result.green = a.green + (b.green - a.green) * t;
  result.blue = a.blue + (b.blue - a.blue) * t;
  result.alpha = a.alpha + (b.alpha - a.alpha) * t;
  result.colorSpace = a.colorSpace;
  return result;
}

std::string LayerTransformCSS(const Layer* layer) {
  if (!layer->matrix3D.isIdentity()) {
    return Matrix3DToCSS(layer->matrix3D);
  }
  // x/y are input attributes resolved via Layer::renderPosition(); they are emitted as
  // left/top by the caller and must not be baked into the transform matrix here.
  Matrix m = layer->matrix;
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

Matrix BuildGroupMatrix(const Group* group) {
  bool hasAnchor = !FloatNearlyZero(group->anchor.x) || !FloatNearlyZero(group->anchor.y);
  // renderPosition() returns the layout-resolved top-left (incorporates constraint layout such as
  // centerX/centerY/left/right). When the group has no layout constraints, renderPosition equals
  // (position.x, position.y), so this subsumes the explicit position property.
  auto renderPos = group->renderPosition();
  bool hasPosition = !FloatNearlyZero(renderPos.x) || !FloatNearlyZero(renderPos.y);
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
    shear.c = std::tan(DegreesToRadians(-group->skew));
    m = shear * m;
    m = Matrix::Rotate(-group->skewAxis) * m;
  }
  if (hasRotation) {
    m = Matrix::Rotate(group->rotation) * m;
  }
  if (hasPosition) {
    m = Matrix::Translate(renderPos.x, renderPos.y) * m;
  }

  return m;
}

const char* AlignmentToCSS(Alignment alignment) {
  switch (alignment) {
    case Alignment::Start:
      return "flex-start";
    case Alignment::Center:
      return "center";
    case Alignment::End:
      return "flex-end";
    case Alignment::Stretch:
      return "stretch";
  }
  return "stretch";
}

const char* ArrangementToCSS(Arrangement arrangement) {
  switch (arrangement) {
    case Arrangement::Start:
      return "flex-start";
    case Arrangement::Center:
      return "center";
    case Arrangement::End:
      return "flex-end";
    case Arrangement::SpaceBetween:
      return "space-between";
    case Arrangement::SpaceEvenly:
      return "space-evenly";
    case Arrangement::SpaceAround:
      return "space-around";
  }
  return "flex-start";
}

std::string PaddingToCSS(const Padding& padding) {
  if (FloatNearlyZero(padding.right - padding.left) &&
      FloatNearlyZero(padding.bottom - padding.top) &&
      FloatNearlyZero(padding.top - padding.left)) {
    return FloatToString(padding.top) + "px";
  }
  if (FloatNearlyZero(padding.top - padding.bottom) &&
      FloatNearlyZero(padding.left - padding.right)) {
    return FloatToString(padding.top) + "px " + FloatToString(padding.right) + "px";
  }
  return FloatToString(padding.top) + "px " + FloatToString(padding.right) + "px " +
         FloatToString(padding.bottom) + "px " + FloatToString(padding.left) + "px";
}

}  // namespace pagx
