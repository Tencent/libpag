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
//  license is distributed on an "AS IS" BASIS, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/html/importer/HTMLTransform3D.h"
#include <cctype>
#include <cmath>
#include <cstdlib>
#include "pagx/html/importer/HTMLDetail.h"
#include "pagx/html/importer/HTMLDiagnosticSink.h"
#include "pagx/html/importer/HTMLValueParser.h"
#include "pagx/runtime/Matrix3DDecompose.h"
#include "pagx/utils/LayerTransformUtils.h"

namespace pagx {

using namespace pagx::html;

namespace {

constexpr float kPoseEpsilon = 1e-6f;

bool NearlyZero(float value) {
  return std::fabs(value) <= kPoseEpsilon;
}

bool NearlyOne(float value) {
  return std::fabs(value - 1.0f) <= kPoseEpsilon;
}

Matrix3D Multiply(const Matrix3D& left, const Matrix3D& right) {
  Matrix3D result = {};
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      float sum = 0.0f;
      for (int k = 0; k < 4; ++k) {
        sum += left.getRowColumn(row, k) * right.getRowColumn(k, column);
      }
      result.setRowColumn(row, column, sum);
    }
  }
  return result;
}

Matrix3D MakeTranslation(float x, float y, float z) {
  Matrix3D m = {};
  m.setRowColumn(0, 3, x);
  m.setRowColumn(1, 3, y);
  m.setRowColumn(2, 3, z);
  return m;
}

Matrix3D MakeScale(float x, float y, float z) {
  Matrix3D m = {};
  m.setRowColumn(0, 0, x);
  m.setRowColumn(1, 1, y);
  m.setRowColumn(2, 2, z);
  return m;
}

Matrix3D MakeRotationX(float degrees) {
  const float radians = static_cast<float>(degrees * DEGREES_TO_RADIANS);
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  Matrix3D m = {};
  m.setRowColumn(1, 1, c);
  m.setRowColumn(1, 2, -s);
  m.setRowColumn(2, 1, s);
  m.setRowColumn(2, 2, c);
  return m;
}

Matrix3D MakeRotationY(float degrees) {
  const float radians = static_cast<float>(degrees * DEGREES_TO_RADIANS);
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  Matrix3D m = {};
  m.setRowColumn(0, 0, c);
  m.setRowColumn(0, 2, s);
  m.setRowColumn(2, 0, -s);
  m.setRowColumn(2, 2, c);
  return m;
}

Matrix3D MakeRotationZ(float degrees) {
  const float radians = static_cast<float>(degrees * DEGREES_TO_RADIANS);
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  Matrix3D m = {};
  m.setRowColumn(0, 0, c);
  m.setRowColumn(0, 1, -s);
  m.setRowColumn(1, 0, s);
  m.setRowColumn(1, 1, c);
  return m;
}

// Rodrigues rotation around a unit axis.
Matrix3D MakeRotationAxis(float x, float y, float z, float degrees) {
  const float radians = static_cast<float>(degrees * DEGREES_TO_RADIANS);
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  const float t = 1.0f - c;
  Matrix3D m = {};
  m.setRowColumn(0, 0, t * x * x + c);
  m.setRowColumn(0, 1, t * x * y - s * z);
  m.setRowColumn(0, 2, t * x * z + s * y);
  m.setRowColumn(1, 0, t * x * y + s * z);
  m.setRowColumn(1, 1, t * y * y + c);
  m.setRowColumn(1, 2, t * y * z - s * x);
  m.setRowColumn(2, 0, t * x * z - s * y);
  m.setRowColumn(2, 1, t * y * z + s * x);
  m.setRowColumn(2, 2, t * z * z + c);
  return m;
}

// `shearX` fills the y -> x coefficient (`c`) and `shearY` the x -> y one (`b`), i.e. the CSS
// convention: `skewX(a)` is `matrix(1, 0, tan a, 1)`. `skewX` used to negate its angle, which made
// HTML -> PAGX -> HTML mirror the shear (the snapshot path hid it by baking the computed matrix).
Matrix3D MakeShear(float shearX, float shearY) {
  Matrix3D m = {};
  m.setRowColumn(0, 1, static_cast<float>(std::tan(shearX * DEGREES_TO_RADIANS)));
  m.setRowColumn(1, 0, static_cast<float>(std::tan(shearY * DEGREES_TO_RADIANS)));
  return m;
}

Matrix3D MakePerspective(float distance) {
  Matrix3D m = {};
  m.setRowColumn(3, 2, -1.0f / distance);
  return m;
}

// `ParseAngle` maps unparsable input to 0 and unknown units to plain degrees, which would silently
// turn `rotate(45px)` into a 45 degree rotation, so the unit is validated here instead. CSS accepts
// `deg` / `grad` / `rad` / `turn` case insensitively; a unitless value is only valid as `0`.
bool ParseRotationDegrees(const std::string& token, float& out) {
  const std::string trimmed = ToLower(Trim(token));
  if (trimmed.empty()) {
    return false;
  }
  char* end = nullptr;
  const float value = std::strtof(trimmed.c_str(), &end);
  if (end == trimmed.c_str() || !std::isfinite(value)) {
    return false;
  }
  // The unit has to follow the number directly. CSS has no whitespace inside a dimension, so
  // `rotate(45 deg)` is an invalid declaration rather than a 45 degree rotation.
  const std::string suffix = std::string(end);
  if (suffix.empty()) {
    if (value != 0.0f) {
      return false;
    }
    out = 0.0f;
    return true;
  }
  if (suffix == "deg") {
    out = value;
  } else if (suffix == "grad") {
    out = value * 0.9f;
  } else if (suffix == "rad") {
    out = static_cast<float>(value * RADIANS_TO_DEGREES);
  } else if (suffix == "turn") {
    out = value * 360.0f;
  } else {
    return false;
  }
  return std::isfinite(out);
}

// Resolves one `transform-origin` axis token to layer-local pixels. `dimension` is NaN when the
// reference box size is unknown, which leaves percentage / keyword forms unresolvable.
float ResolveOriginAxis(const std::string& token, float dimension, HTMLValueParser& valueParser) {
  if (token == "center") {
    return dimension * 0.5f;
  }
  if (token == "left" || token == "top") {
    return 0.0f;
  }
  if (token == "right" || token == "bottom") {
    return dimension;
  }
  float fraction = 0.0f;
  if (ParseCssPercentage(token, fraction)) {
    return dimension * fraction;
  }
  return valueParser.parseAbsoluteLengthPx(token);
}

// One arity diagnostic per function family, shared by the "wrong number of arguments" and the
// "argument failed to parse" exits so both describe the same contract.
std::string TranslateArityMessage(const std::string& label, const std::string& token) {
  if (label == "translate") {
    return "html: translate expects 1 or 2 length arguments; got '" + token + "'";
  }
  if (label == "translate3d") {
    return "html: translate3d expects 3 length arguments; got '" + token + "'";
  }
  return "html: " + label + " expects 1 length argument; got '" + token + "'";
}

std::string ScaleArityMessage(const std::string& label, const std::string& token) {
  if (label == "scale") {
    return "html: scale expects 1 or 2 numeric arguments; got '" + token + "'";
  }
  if (label == "scale3d") {
    return "html: scale3d expects 3 numeric arguments; got '" + token + "'";
  }
  return "html: " + label + " expects 1 numeric argument; got '" + token + "'";
}

// Resolves one length argument: a plain px value or a percentage of `referencePx`. `referencePx` is
// NaN when the axis has no percentage basis (the Z axis) or the box size is unknown, in which case a
// percentage is reported as unsupported. `calc()` is deliberately out of scope: it needs CSS type
// checking (a length times a length, or a bare number, is not a length), and the snapshot path feeds
// computed values so nothing here depends on it.
bool ResolveLengthToken(const std::string& token, float referencePx, HTMLValueParser& valueParser,
                        float& out) {
  const std::string trimmed = Trim(token);
  if (!trimmed.empty() && trimmed.back() == '%') {
    float fraction = 0.0f;
    if (!ParseCssPercentage(trimmed, fraction) || !std::isfinite(referencePx)) {
      return false;
    }
    out = referencePx * fraction;
    return true;
  }
  // The callers name the offending argument themselves, so the generic unit warning is suppressed
  // here: `translate(10foo)` reports once, not twice.
  out = valueParser.parseAbsoluteLengthPx(trimmed, /*warn=*/false);
  return std::isfinite(out);
}

// Argument accessors for one transform function. A small struct keeps the parser flat and avoids
// the capturing lambdas the project style discourages.
struct TransformArgs {
  const std::vector<std::string>& parts;
  HTMLValueParser& valueParser;
  // Percentage lengths resolve against the element box: width for X, height for Y.
  float referenceWidthPx = NAN;
  float referenceHeightPx = NAN;

  // `axis`: 0 = x (percent of width), 1 = y (percent of height), 2 = z (no percentage basis).
  bool LengthAt(size_t index, float& value, int axis = 0) const {
    if (index >= parts.size()) {
      return false;
    }
    const float reference = axis == 0 ? referenceWidthPx : (axis == 1 ? referenceHeightPx : NAN);
    return ResolveLengthToken(parts[index], reference, valueParser, value);
  }

  bool NumberAt(size_t index, float& value) const {
    return index < parts.size() && ParseScalarFloat(parts[index], value);
  }

  bool AngleAt(size_t index, float& value) const {
    return index < parts.size() && ParseRotationDegrees(parts[index], value);
  }
};

bool IsFinite(const Matrix3D& matrix) {
  for (float value : matrix.values) {
    if (!std::isfinite(value)) {
      return false;
    }
  }
  return true;
}

// Parses one `fn(args)` token. Returns false after emitting a diagnostic.
bool ParseFunction(const std::string& token, HTMLValueParser& valueParser,
                   HTMLDiagnosticSink& diagnostics, Matrix3D* out, float referenceWidthPx,
                   float referenceHeightPx) {
  std::string name;
  std::string args;
  if (!ParseCssFunctionCall(token, name, args)) {
    diagnostics.warn("html: transform '" + token + "' is malformed; ignored");
    return false;
  }
  if (!IsSupportedTransformFunction(name)) {
    diagnostics.warn("html: transform function '" + name +
                     "' is not in the supported subset; ignored");
    return false;
  }
  // Diagnostics keep the authored spelling (`translateX`, `skewX`, ...) so the message points at
  // exactly the token the author wrote.
  const std::string label = Trim(token.substr(0, token.find('(')));
  const auto parts = SplitTopLevelCommas(args);

  const TransformArgs transformArgs = {parts, valueParser, referenceWidthPx, referenceHeightPx};

  if (name == "translate" || name == "translate3d" || name == "translatex" ||
      name == "translatey" || name == "translatez") {
    const bool countOk = name == "translate3d" ? parts.size() == 3u
                         : name == "translate" ? parts.size() == 1u || parts.size() == 2u
                                               : parts.size() == 1u;
    if (!countOk) {
      diagnostics.warn(TranslateArityMessage(label, token));
      return false;
    }
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    if (name == "translate" || name == "translate3d") {
      // `translate` stops at two lengths, so only `translate3d` reaches the depth axis.
      if (!transformArgs.LengthAt(0, x, 0)) {
        diagnostics.warn("html: translate first argument '" + parts[0] +
                         "' is not a px length; ignored");
        return false;
      }
      if (parts.size() > 1 && !transformArgs.LengthAt(1, y, 1)) {
        diagnostics.warn("html: translate second argument '" + parts[1] +
                         "' is not a px length; ignored");
        return false;
      }
      if (parts.size() > 2 && !transformArgs.LengthAt(2, z, 2)) {
        diagnostics.warn("html: translate third argument '" + parts[2] +
                         "' is not a px length; ignored");
        return false;
      }
    } else {
      float& only = name == "translatex" ? x : (name == "translatey" ? y : z);
      const int onlyAxis = name == "translatex" ? 0 : (name == "translatey" ? 1 : 2);
      if (!transformArgs.LengthAt(0, only, onlyAxis)) {
        diagnostics.warn("html: " + label + " argument '" + parts[0] +
                         "' is not a px length; ignored");
        return false;
      }
    }
    *out = MakeTranslation(x, y, z);
    return true;
  }

  if (name == "scale" || name == "scale3d" || name == "scalex" || name == "scaley" ||
      name == "scalez") {
    const bool countOk = name == "scale3d" ? parts.size() == 3u
                         : name == "scale" ? parts.size() == 1u || parts.size() == 2u
                                           : parts.size() == 1u;
    if (!countOk) {
      diagnostics.warn(ScaleArityMessage(label, token));
      return false;
    }
    float x = 1.0f;
    float y = 1.0f;
    float z = 1.0f;
    if (name == "scale" || name == "scale3d") {
      // A single argument scales both axes uniformly before the optional Y / Z values apply.
      if (!transformArgs.NumberAt(0, x)) {
        diagnostics.warn(ScaleArityMessage(label, token));
        return false;
      }
      y = x;
      if (parts.size() > 1 && !transformArgs.NumberAt(1, y)) {
        diagnostics.warn(ScaleArityMessage(label, token));
        return false;
      }
      if (parts.size() > 2 && !transformArgs.NumberAt(2, z)) {
        diagnostics.warn(ScaleArityMessage(label, token));
        return false;
      }
    } else {
      float& only = name == "scalex" ? x : (name == "scaley" ? y : z);
      if (!transformArgs.NumberAt(0, only)) {
        diagnostics.warn(ScaleArityMessage(label, token));
        return false;
      }
    }
    *out = MakeScale(x, y, z);
    return true;
  }

  if (name == "rotate" || name == "rotatex" || name == "rotatey" || name == "rotatez") {
    float degrees = 0.0f;
    if (parts.size() != 1) {
      diagnostics.warn("html: " + label + " expects 1 argument; got '" + token + "'");
      return false;
    }
    if (!transformArgs.AngleAt(0, degrees)) {
      diagnostics.warn("html: " + label + " angle '" + parts[0] +
                       "' is not a valid angle (deg / grad / rad / turn); ignored");
      return false;
    }
    if (name == "rotatex") {
      *out = MakeRotationX(degrees);
    } else if (name == "rotatey") {
      *out = MakeRotationY(degrees);
    } else {
      *out = MakeRotationZ(degrees);
    }
    return true;
  }

  if (name == "rotate3d") {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float degrees = 0.0f;
    if (parts.size() != 4 || !transformArgs.NumberAt(0, x) || !transformArgs.NumberAt(1, y) ||
        !transformArgs.NumberAt(2, z) || !transformArgs.AngleAt(3, degrees)) {
      diagnostics.warn("html: rotate3d expects 3 axis numbers and 1 angle; ignored");
      return false;
    }
    // The axis is normalized in double precision: `rotate3d(1e20, 0, 0, 45deg)` squares to infinity
    // in float, which used to slip past the zero-vector check and collapse the rotation to `cos * I`.
    // Inputs beyond the float range never get here (the argument parser rejects them as non-numeric),
    // so a zero-length vector is the only degenerate case left to catch.
    const double axisX = static_cast<double>(x);
    const double axisY = static_cast<double>(y);
    const double axisZ = static_cast<double>(z);
    const double length = std::sqrt(axisX * axisX + axisY * axisY + axisZ * axisZ);
    if (!(length > 1e-6)) {
      diagnostics.warn("html: rotate3d axis is a zero vector; ignored");
      return false;
    }
    *out = MakeRotationAxis(static_cast<float>(axisX / length), static_cast<float>(axisY / length),
                            static_cast<float>(axisZ / length), degrees);
    return true;
  }

  if (name == "skewx") {
    float degrees = 0.0f;
    if (parts.size() != 1) {
      diagnostics.warn("html: skewX expects 1 argument; got '" + token + "'");
      return false;
    }
    if (!transformArgs.AngleAt(0, degrees)) {
      diagnostics.warn("html: skewX angle '" + parts[0] +
                       "' is not a valid angle (deg / grad / rad / turn); ignored");
      return false;
    }
    *out = MakeShear(degrees, 0.0f);
    return true;
  }
  if (name == "skewy") {
    float degrees = 0.0f;
    if (parts.size() != 1) {
      diagnostics.warn("html: skewY expects 1 argument; got '" + token + "'");
      return false;
    }
    if (!transformArgs.AngleAt(0, degrees)) {
      diagnostics.warn("html: skewY angle '" + parts[0] +
                       "' is not a valid angle (deg / grad / rad / turn); ignored");
      return false;
    }
    *out = MakeShear(0.0f, degrees);
    return true;
  }
  if (name == "skew") {
    float x = 0.0f;
    float y = 0.0f;
    if (parts.empty() || parts.size() > 2 || !transformArgs.AngleAt(0, x) ||
        (parts.size() == 2 && !transformArgs.AngleAt(1, y))) {
      diagnostics.warn("html: skew expects 1 or 2 angles; got '" + token + "'");
      return false;
    }
    *out = MakeShear(x, y);
    return true;
  }

  if (name == "matrix") {
    float values[6] = {};
    if (parts.size() != 6) {
      diagnostics.warn("html: matrix expects 6 numeric arguments; got '" + token + "'");
      return false;
    }
    for (size_t i = 0; i < 6; ++i) {
      if (!transformArgs.NumberAt(i, values[i])) {
        diagnostics.warn("html: matrix argument '" + parts[i] + "' is not a number; ignored");
        return false;
      }
    }
    Matrix affine = {};
    affine.a = values[0];
    affine.b = values[1];
    affine.c = values[2];
    affine.d = values[3];
    affine.tx = values[4];
    affine.ty = values[5];
    *out = LiftMatrixToMatrix3D(affine, 0.0f);
    return true;
  }

  if (name == "matrix3d") {
    if (parts.size() != 16) {
      diagnostics.warn("html: matrix3d expects 16 numeric arguments; got '" + token + "'");
      return false;
    }
    Matrix3D matrix = {};
    for (size_t i = 0; i < 16; ++i) {
      if (!transformArgs.NumberAt(i, matrix.values[i])) {
        diagnostics.warn("html: matrix3d argument '" + parts[i] + "' is not a number; ignored");
        return false;
      }
    }
    *out = matrix;
    return true;
  }

  if (name == "perspective") {
    if (parts.size() != 1) {
      diagnostics.warn("html: perspective expects 1 length; got '" + token + "'");
      return false;
    }
    if (ToLower(Trim(parts[0])) == "none") {
      *out = {};
      return true;
    }
    float distance = 0.0f;
    if (!transformArgs.LengthAt(0, distance, 2) || !(distance > 0.0f)) {
      diagnostics.warn("html: perspective distance must be a positive length; got '" + token +
                       "'; ignored");
      return false;
    }
    *out = MakePerspective(distance);
    return true;
  }

  diagnostics.warn("html: transform function '" + name + "' is not supported; ignored");
  return false;
}

// Which axis a `transform-origin` token may occupy. `left` / `right` are horizontal, `top` /
// `bottom` vertical; `center`, lengths and percentages fill either axis.
enum class OriginAxis { Horizontal, Vertical, Any };

OriginAxis OriginAxisOf(const std::string& token) {
  if (token == "left" || token == "right") {
    return OriginAxis::Horizontal;
  }
  if (token == "top" || token == "bottom") {
    return OriginAxis::Vertical;
  }
  return OriginAxis::Any;
}

bool IsOriginKeyword(const std::string& token) {
  return token == "left" || token == "right" || token == "top" || token == "bottom" ||
         token == "center";
}

}  // namespace

bool IsSupportedTransformFunction(const std::string& name) {
  static const char* kSupported[] = {
      "translate", "translatex", "translatey", "translatez", "translate3d", "scale",
      "scalex",    "scaley",     "scalez",     "scale3d",    "rotate",      "rotatex",
      "rotatey",   "rotatez",    "rotate3d",   "skewx",      "skewy",       "skew",
      "matrix",    "matrix3d",   "perspective"};
  for (const auto* candidate : kSupported) {
    if (name == candidate) {
      return true;
    }
  }
  return false;
}

bool SplitTransformFunctions(const std::string& value, std::vector<std::string>* out) {
  if (out == nullptr) {
    return false;
  }
  out->clear();
  size_t index = 0;
  const size_t size = value.size();
  while (index < size) {
    while (index < size && std::isspace(static_cast<unsigned char>(value[index]))) {
      ++index;
    }
    if (index >= size) {
      break;
    }
    const size_t nameStart = index;
    while (index < size && value[index] != '(' &&
           !std::isspace(static_cast<unsigned char>(value[index]))) {
      ++index;
    }
    if (index >= size || value[index] != '(') {
      return false;
    }
    // `<transform-list>` is `<transform-function>+`, so `rotate(45deg)scale(2)` is legal and needs no
    // separator. The closing paren is matched by depth, so an argument may carry parentheses of its
    // own (`translateX(calc(100% - 10px))`).
    int depth = 0;
    size_t closeParen = index;
    for (; closeParen < size; ++closeParen) {
      if (value[closeParen] == '(') {
        ++depth;
      } else if (value[closeParen] == ')' && --depth == 0) {
        break;
      }
    }
    if (closeParen >= size) {
      return false;
    }
    out->push_back(Trim(value.substr(nameStart, closeParen - nameStart + 1)));
    index = closeParen + 1;
  }
  return !out->empty();
}

HTMLTransformOrigin ResolveTransformOrigin(const std::string& value, float width, float height,
                                           HTMLValueParser& valueParser) {
  HTMLTransformOrigin origin = {};
  std::string text = ToLower(Trim(value));
  if (text.empty()) {
    text = "50% 50%";
  }
  const auto tokens = SplitTopLevelWhitespace(text);
  if (tokens.empty() || tokens.size() > 3) {
    return origin;
  }

  // CSS orders the two components by which axis each token belongs to, not by position: `bottom
  // left` is x=left, y=bottom and `center left` is x=left, y=center. A length may only take a slot
  // whose keyword class is `Any`, so `top 20px` and `20px left` are invalid declarations (Chromium
  // drops them and falls back to `50% 50%`) while `20px top`, `left 20px` and `20px center` are fine.
  std::string tokenX = "center";
  std::string tokenY = "center";
  bool invalid = false;
  if (tokens.size() == 1) {
    if (OriginAxisOf(tokens[0]) == OriginAxis::Vertical) {
      tokenY = tokens[0];
    } else {
      tokenX = tokens[0];
    }
  } else {
    const OriginAxis first = OriginAxisOf(tokens[0]);
    const OriginAxis second = OriginAxisOf(tokens[1]);
    const bool firstIsLength = !IsOriginKeyword(tokens[0]);
    const bool secondIsLength = !IsOriginKeyword(tokens[1]);
    if (first == second && first != OriginAxis::Any) {
      // `left right` / `top bottom` name the same axis twice.
      invalid = true;
    } else if (first == OriginAxis::Vertical && secondIsLength) {
      // `top 20px`: the vertical keyword cannot precede a bare length.
      invalid = true;
    } else if (firstIsLength && second == OriginAxis::Horizontal) {
      // `20px left`: a bare length cannot precede a horizontal keyword.
      invalid = true;
    } else if (first == OriginAxis::Vertical || second == OriginAxis::Horizontal) {
      tokenX = tokens[1];
      tokenY = tokens[0];
    } else {
      tokenX = tokens[0];
      tokenY = tokens[1];
    }
  }

  if (invalid) {
    // An invalid `<position>` drops the declaration, so the initial `50% 50%` applies. Reporting it
    // keeps the author-facing diagnostic honest rather than silently pivoting elsewhere.
    origin.valid = false;
    if (std::isfinite(width) && std::isfinite(height)) {
      origin.point = Point{width * 0.5f, height * 0.5f};
      origin.resolved = true;
    }
    return origin;
  }

  const float x = ResolveOriginAxis(tokenX, width, valueParser);
  const float y = ResolveOriginAxis(tokenY, height, valueParser);
  if (!std::isfinite(x) || !std::isfinite(y)) {
    return origin;
  }
  float z = 0.0f;
  if (tokens.size() == 3) {
    z = valueParser.parseAbsoluteLengthPx(tokens[2]);
    if (!std::isfinite(z)) {
      return origin;
    }
  }
  origin.point = Point{x, y};
  origin.z = z;
  origin.resolved = true;
  return origin;
}

bool ParseCssTransformList(const std::string& value, HTMLValueParser& valueParser,
                           HTMLDiagnosticSink& diagnostics, Matrix3D* out, float referenceWidthPx,
                           float referenceHeightPx) {
  if (out == nullptr) {
    return false;
  }
  const std::string trimmed = Trim(value);
  if (trimmed.empty() || ToLower(trimmed) == "none") {
    *out = {};
    return false;
  }
  std::vector<std::string> tokens = {};
  if (!SplitTransformFunctions(trimmed, &tokens)) {
    diagnostics.warn("html: transform '" + trimmed + "' is malformed; ignored");
    return false;
  }
  Matrix3D pose = {};
  for (const auto& token : tokens) {
    Matrix3D step = {};
    if (!ParseFunction(token, valueParser, diagnostics, &step, referenceWidthPx,
                       referenceHeightPx)) {
      return false;
    }
    pose = Multiply(pose, step);
  }
  if (!IsFinite(pose)) {
    diagnostics.warn("html: transform '" + trimmed + "' produces a non-finite pose; ignored");
    return false;
  }
  // PAGX requires a 3D pose to be invertible; a singular flat pose stays legal because a 2D
  // matrix has no such constraint (a zero scale is a valid, if empty, 2D transform).
  if (!IsFlatTransform(pose) && !IsInvertibleMatrix3D(pose)) {
    diagnostics.warn("html: transform '" + trimmed + "' is not invertible; ignored");
    return false;
  }
  *out = pose;
  return true;
}

// Internal: the identity with its bottom row set to `(0, 0, -1/distance, 1)`. `Layer.perspective`
// builds the same matrix, but a CSS `perspective` declaration lives on the parent and has to be
// pushed into the child pose instead.
static Matrix3D PerspectiveMatrix(float distance) {
  if (!(distance > 0.0f) || !std::isfinite(distance)) {
    return {};
  }
  return MakePerspective(distance);
}

Matrix3D PushPerspective(const Matrix3D& pose, float distance, float originX, float originY) {
  if (!(distance > 0.0f) || !std::isfinite(distance)) {
    return pose;
  }
  // CSS projects about `perspective-origin` (the parent's centre by default), not about the parent's
  // local origin, so the projection is conjugated with that point before it is folded into the pose.
  const Matrix3D projection = PerspectiveMatrix(distance);
  if (!std::isfinite(originX) || !std::isfinite(originY) || (originX == 0.0f && originY == 0.0f)) {
    return Multiply(projection, pose);
  }
  const Matrix3D toOrigin = MakeTranslation(originX, originY, 0.0f);
  const Matrix3D fromOrigin = MakeTranslation(-originX, -originY, 0.0f);
  return Multiply(Multiply(toOrigin, projection), Multiply(fromOrigin, pose));
}

PosePlacement PlacePose(const Matrix3D& pose, float parentPerspectivePx,
                        float localPerspectiveOriginX, float localPerspectiveOriginY) {
  PosePlacement placement = {};
  placement.pose =
      PushPerspective(pose, parentPerspectivePx, localPerspectiveOriginX, localPerspectiveOriginY);
  placement.flat = IsFlatTransform(placement.pose);
  // PAGX requires a 3D pose to be invertible; a flat pose has no such constraint, because a zero
  // scale is a valid (if empty) 2D transform.
  placement.valid = placement.flat || IsInvertibleMatrix3D(placement.pose);
  placement.linearIsIdentity = IsLinearIdentity(placement.pose);
  return placement;
}

bool IsFlatTransform(const Matrix3D& matrix) {
  // Composing rotations leaves values like 0.99999994 on the Z axis, so the plane test needs a
  // tolerance instead of exact comparisons.
  //
  // This is a *geometric* test, so it deliberately differs from `HasPerspective` in
  // `LayerTransformUtils.h`, which asks whether the matrix carries a projection row at all (an exact
  // `!= 0`). The projection coefficient is `1 / distance`, so a camera at `distance >= 1 / kPoseEpsilon`
  // (i.e. `perspective(1000000px)` or further) reads as flat here while `HasPerspective` still sees a
  // projection: on the z = 0 plane the two disagree by less than a pixel, and the exact test is what
  // export needs to keep the row it was given.
  return NearlyZero(matrix.getRowColumn(2, 0)) && NearlyZero(matrix.getRowColumn(2, 1)) &&
         NearlyOne(matrix.getRowColumn(2, 2)) && NearlyZero(matrix.getRowColumn(2, 3)) &&
         NearlyZero(matrix.getRowColumn(3, 0)) && NearlyZero(matrix.getRowColumn(3, 1)) &&
         NearlyZero(matrix.getRowColumn(3, 2)) && NearlyOne(matrix.getRowColumn(3, 3)) &&
         NearlyZero(matrix.getRowColumn(0, 2)) && NearlyZero(matrix.getRowColumn(1, 2));
}

Matrix ToAffine2D(const Matrix3D& matrix) {
  Matrix affine = {};
  affine.a = matrix.getRowColumn(0, 0);
  affine.b = matrix.getRowColumn(1, 0);
  affine.c = matrix.getRowColumn(0, 1);
  affine.d = matrix.getRowColumn(1, 1);
  affine.tx = matrix.getRowColumn(0, 3);
  affine.ty = matrix.getRowColumn(1, 3);
  return affine;
}

bool IsLinearIdentity(const Matrix3D& matrix) {
  for (int row = 0; row < 3; ++row) {
    for (int column = 0; column < 3; ++column) {
      const float expected = row == column ? 1.0f : 0.0f;
      if (std::fabs(matrix.getRowColumn(row, column) - expected) > kPoseEpsilon) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace pagx
