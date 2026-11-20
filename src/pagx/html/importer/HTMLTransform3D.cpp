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
#include <unordered_map>
#include "pagx/html/importer/HTMLDetail.h"
#include "pagx/html/importer/HTMLDiagnosticSink.h"
#include "pagx/html/importer/HTMLValueParser.h"
#include "pagx/nodes/Layer.h"
#include "pagx/runtime/Matrix3DDecompose.h"
#include "pagx/utils/LayerTransformUtils.h"

namespace pagx {

using namespace pagx::html;

namespace {

constexpr float POSE_EPSILON = 1e-6f;

// Substitute for a zero scale factor when a caller asks for a pose it can persist (see `ZeroScale`).
// A zero factor is rank-deficient, so composing it — in CSS order — collapses the whole list:
// `rotateY(-90deg) scale(0)` becomes the zero matrix and the rotation is gone. Replacing the factor
// *before* the multiply keeps the direction (`epsilon * R(-90deg)`), which is what makes the pose
// invertible and therefore representable as a `matrix3D` keyframe.
//
// 1e-3 is picked from the rendering budget: on a 60px box the clamped edge is 60 * 1e-3 = 0.06px,
// i.e. 0.0036 square px, which no viewer can see. Float32 rotation / scale decomposition stays well
// conditioned down to about 1e-5, so 1e-3 is still far outside that noise band and there is no
// reason to go smaller — a smaller factor buys nothing visually and only moves closer to the range
// where the decomposition itself becomes unreliable.
constexpr float SINGULAR_SCALE_CLAMP = 1e-3f;

// Zero carries no direction, so the substitute is the positive epsilon.
float ClampSingularScale(float value) {
  return value == 0.0f ? SINGULAR_SCALE_CLAMP : value;
}

bool NearlyZero(float value) {
  return std::fabs(value) <= POSE_EPSILON;
}

bool NearlyOne(float value) {
  return std::fabs(value - 1.0f) <= POSE_EPSILON;
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

// One `transform-origin` axis token, resolved against a box of `dimension` px. The three outcomes must
// stay distinct: a bad token invalidates the declaration, while a legal token whose box size is still
// unknown is merely unresolvable — collapsing the two is what made `nan% 20px` report "unknown size".
enum class OriginTokenAxis { Resolved, TokenInvalid, SizeUnknown };

OriginTokenAxis ApplyOriginFraction(float dimension, float fraction, float& out) {
  if (std::isnan(dimension)) {
    out = NAN;
    return OriginTokenAxis::SizeUnknown;
  }
  out = dimension * fraction;
  return OriginTokenAxis::Resolved;
}

OriginTokenAxis ResolveOriginAxis(const std::string& token, float dimension,
                                  HTMLValueParser& valueParser, float& out, bool quiet) {
  if (token == "center") {
    return ApplyOriginFraction(dimension, 0.5f, out);
  }
  if (token == "left" || token == "top") {
    out = 0.0f;
    return OriginTokenAxis::Resolved;
  }
  if (token == "right" || token == "bottom") {
    return ApplyOriginFraction(dimension, 1.0f, out);
  }
  float fraction = 0.0f;
  if (ParseCssPercentage(token, fraction)) {
    // `nan%` / `inf%` parse as a percentage but carry no value; the declaration is invalid rather
    // than unresolvable, because a real box would not have saved it either.
    if (!std::isfinite(fraction)) {
      return OriginTokenAxis::TokenInvalid;
    }
    return ApplyOriginFraction(dimension, fraction, out);
  }
  // The caller reports the declaration itself as invalid, so the generic unit warning is suppressed
  // here: `10foo 20px` yields one diagnostic, not a unit warning plus a position warning.
  out = quiet ? valueParser.parseQuietLengthPx(token)
              : valueParser.parseAbsoluteLengthPx(token, /*warn=*/false);
  return std::isfinite(out) ? OriginTokenAxis::Resolved : OriginTokenAxis::TokenInvalid;
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
    return std::isfinite(out);
  }
  // The callers name the offending argument themselves, so the generic unit warning is suppressed
  // here: `translate(10foo)` reports once, not twice.
  out = valueParser.parseAbsoluteLengthPx(trimmed, /*warn=*/false);
  return std::isfinite(out);
}

// Which box dimension a percentage length argument resolves against. There is no default: a caller
// that forgot the axis used to resolve percentages against the width silently.
enum class TransformAxis { X, Y, Z };

// Argument accessors for one transform function. A small struct keeps the parser flat and avoids
// the capturing lambdas the project style discourages.
struct TransformArgs {
  const std::vector<std::string>& parts;
  HTMLValueParser& valueParser;
  // Percentage lengths resolve against the element box: width for X, height for Y.
  float referenceWidthPx = NAN;
  float referenceHeightPx = NAN;
  // How the `scale` family treats a zero factor; set by `ParseCssTransformList` and reported back
  // through `clampedZeroScale`.
  ZeroScale zeroScale = ZeroScale::Keep;
  bool* clampedZeroScale = nullptr;

  // The Z axis has no percentage basis, so a percentage there is reported as unsupported.
  bool LengthAt(size_t index, float& value, TransformAxis axis) const {
    if (index >= parts.size()) {
      return false;
    }
    float reference = NAN;
    if (axis == TransformAxis::X) {
      reference = referenceWidthPx;
    } else if (axis == TransformAxis::Y) {
      reference = referenceHeightPx;
    }
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

// One transform function's arguments together with everything its family handler needs for parsing
// and for the diagnostics: the authored token and spelling, the comma-split arguments and the pose
// to fill in.
struct TransformCall {
  const std::string& token;
  const std::string& name;
  const std::string& label;
  const TransformArgs& args;
  HTMLDiagnosticSink& diagnostics;
  Matrix3D* out;
};

using TransformFamilyHandler = bool (*)(const TransformCall& call);

// Argument count shared by the `translate` / `scale` families: the two-argument form takes one or
// two, the `3d` form exactly three, and the single-axis forms exactly one.
bool AcceptsFamilyArity(size_t count, bool twoArgumentForm, bool threeArgumentForm) {
  if (threeArgumentForm) {
    return count == 3u;
  }
  if (twoArgumentForm) {
    return count == 1u || count == 2u;
  }
  return count == 1u;
}

bool ParseTranslate(const TransformCall& call) {
  const auto& parts = call.args.parts;
  if (!AcceptsFamilyArity(parts.size(), call.name == "translate", call.name == "translate3d")) {
    if (call.name == "translate") {
      call.diagnostics.warn("html: translate expects 1 or 2 length arguments; got '" + call.token +
                            "'");
    } else if (call.name == "translate3d") {
      call.diagnostics.warn("html: translate3d expects 3 length arguments; got '" + call.token +
                            "'");
    } else {
      call.diagnostics.warn("html: " + call.label + " expects 1 length argument; got '" +
                            call.token + "'");
    }
    return false;
  }
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  if (call.name == "translate" || call.name == "translate3d") {
    // `translate` stops at two lengths, so only `translate3d` reaches the depth axis.
    if (!call.args.LengthAt(0, x, TransformAxis::X)) {
      call.diagnostics.warn("html: translate first argument '" + parts[0] +
                            "' is not a px length; ignored");
      return false;
    }
    if (parts.size() > 1 && !call.args.LengthAt(1, y, TransformAxis::Y)) {
      call.diagnostics.warn("html: translate second argument '" + parts[1] +
                            "' is not a px length; ignored");
      return false;
    }
    if (parts.size() > 2 && !call.args.LengthAt(2, z, TransformAxis::Z)) {
      call.diagnostics.warn("html: translate third argument '" + parts[2] +
                            "' is not a px length; ignored");
      return false;
    }
  } else {
    float& only = call.name == "translatex" ? x : (call.name == "translatey" ? y : z);
    const TransformAxis onlyAxis = call.name == "translatex"   ? TransformAxis::X
                                   : call.name == "translatey" ? TransformAxis::Y
                                                               : TransformAxis::Z;
    if (!call.args.LengthAt(0, only, onlyAxis)) {
      call.diagnostics.warn("html: " + call.label + " argument '" + parts[0] +
                            "' is not a px length; ignored");
      return false;
    }
  }
  *call.out = MakeTranslation(x, y, z);
  return true;
}

bool ParseScale(const TransformCall& call) {
  const auto& parts = call.args.parts;
  // The wrong-arity exit and every argument that fails to parse share this text: a numeric scale
  // argument has no per-argument wording of its own.
  std::string arityMessage;
  if (call.name == "scale") {
    arityMessage = "html: scale expects 1 or 2 numeric arguments; got '" + call.token + "'";
  } else if (call.name == "scale3d") {
    arityMessage = "html: scale3d expects 3 numeric arguments; got '" + call.token + "'";
  } else {
    arityMessage = "html: " + call.label + " expects 1 numeric argument; got '" + call.token + "'";
  }
  if (!AcceptsFamilyArity(parts.size(), call.name == "scale", call.name == "scale3d")) {
    call.diagnostics.warn(arityMessage);
    return false;
  }
  float x = 1.0f;
  float y = 1.0f;
  float z = 1.0f;
  if (call.name == "scale" || call.name == "scale3d") {
    // A single argument scales both axes uniformly before the optional Y / Z values apply.
    if (!call.args.NumberAt(0, x)) {
      call.diagnostics.warn(arityMessage);
      return false;
    }
    y = x;
    if (parts.size() > 1 && !call.args.NumberAt(1, y)) {
      call.diagnostics.warn(arityMessage);
      return false;
    }
    if (parts.size() > 2 && !call.args.NumberAt(2, z)) {
      call.diagnostics.warn(arityMessage);
      return false;
    }
  } else {
    float& only = call.name == "scalex" ? x : (call.name == "scaley" ? y : z);
    if (!call.args.NumberAt(0, only)) {
      call.diagnostics.warn(arityMessage);
      return false;
    }
  }
  if (call.args.zeroScale == ZeroScale::ClampToEpsilon && (x == 0.0f || y == 0.0f || z == 0.0f)) {
    x = ClampSingularScale(x);
    y = ClampSingularScale(y);
    z = ClampSingularScale(z);
    if (call.args.clampedZeroScale != nullptr) {
      *call.args.clampedZeroScale = true;
    }
  }
  *call.out = MakeScale(x, y, z);
  return true;
}

bool ParseRotate(const TransformCall& call) {
  const auto& parts = call.args.parts;
  if (parts.size() != 1) {
    call.diagnostics.warn("html: " + call.label + " expects 1 argument; got '" + call.token + "'");
    return false;
  }
  float degrees = 0.0f;
  if (!call.args.AngleAt(0, degrees)) {
    call.diagnostics.warn("html: " + call.label + " angle '" + parts[0] +
                          "' is not a valid angle (deg / grad / rad / turn); ignored");
    return false;
  }
  if (call.name == "rotatex") {
    *call.out = MakeRotationX(degrees);
  } else if (call.name == "rotatey") {
    *call.out = MakeRotationY(degrees);
  } else {
    *call.out = MakeRotationZ(degrees);
  }
  return true;
}

bool ParseRotate3D(const TransformCall& call) {
  const auto& parts = call.args.parts;
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float degrees = 0.0f;
  if (parts.size() != 4 || !call.args.NumberAt(0, x) || !call.args.NumberAt(1, y) ||
      !call.args.NumberAt(2, z) || !call.args.AngleAt(3, degrees)) {
    call.diagnostics.warn("html: rotate3d expects 3 axis numbers and 1 angle; ignored");
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
  // Only a true zero (or a non-finite sum) is degenerate: `rotate3d(1e-7, 0, 0, 60deg)` is a stable
  // rotation about X, and rejecting it with a fixed length threshold silently dropped the function.
  if (!std::isfinite(length) || length == 0.0) {
    call.diagnostics.warn("html: rotate3d axis is a zero vector; ignored");
    return false;
  }
  *call.out =
      MakeRotationAxis(static_cast<float>(axisX / length), static_cast<float>(axisY / length),
                       static_cast<float>(axisZ / length), degrees);
  return true;
}

bool ParseSkew(const TransformCall& call) {
  const auto& parts = call.args.parts;
  if (call.name == "skew") {
    float x = 0.0f;
    float y = 0.0f;
    if (parts.empty() || parts.size() > 2 || !call.args.AngleAt(0, x) ||
        (parts.size() == 2 && !call.args.AngleAt(1, y))) {
      call.diagnostics.warn("html: skew expects 1 or 2 angles; got '" + call.token + "'");
      return false;
    }
    *call.out = MakeShear(x, y);
    return true;
  }
  const bool vertical = call.name == "skewy";
  const char* label = vertical ? "skewY" : "skewX";
  if (parts.size() != 1) {
    call.diagnostics.warn("html: " + std::string(label) + " expects 1 argument; got '" +
                          call.token + "'");
    return false;
  }
  float degrees = 0.0f;
  if (!call.args.AngleAt(0, degrees)) {
    call.diagnostics.warn("html: " + std::string(label) + " angle '" + parts[0] +
                          "' is not a valid angle (deg / grad / rad / turn); ignored");
    return false;
  }
  *call.out = vertical ? MakeShear(0.0f, degrees) : MakeShear(degrees, 0.0f);
  return true;
}

// Shared by `matrix` and `matrix3d`: both spell their diagnostics with the function name and the
// argument count, and both reject an argument that is not a number.
bool ParseMatrixValues(const TransformCall& call, size_t count, float* values) {
  const auto& parts = call.args.parts;
  if (parts.size() != count) {
    call.diagnostics.warn("html: " + call.name + " expects " + std::to_string(count) +
                          " numeric arguments; got '" + call.token + "'");
    return false;
  }
  for (size_t i = 0; i < count; ++i) {
    if (!call.args.NumberAt(i, values[i])) {
      call.diagnostics.warn("html: " + call.name + " argument '" + parts[i] +
                            "' is not a number; ignored");
      return false;
    }
  }
  return true;
}

bool ParseMatrix(const TransformCall& call) {
  float values[6] = {};
  if (!ParseMatrixValues(call, 6, values)) {
    return false;
  }
  Matrix affine = {};
  affine.a = values[0];
  affine.b = values[1];
  affine.c = values[2];
  affine.d = values[3];
  affine.tx = values[4];
  affine.ty = values[5];
  *call.out = LiftMatrixToMatrix3D(affine, 0.0f);
  return true;
}

bool ParseMatrix3D(const TransformCall& call) {
  Matrix3D matrix = {};
  if (!ParseMatrixValues(call, 16, matrix.values)) {
    return false;
  }
  *call.out = matrix;
  return true;
}

bool ParsePerspective(const TransformCall& call) {
  const auto& parts = call.args.parts;
  if (parts.size() != 1) {
    call.diagnostics.warn("html: perspective expects 1 length; got '" + call.token + "'");
    return false;
  }
  if (ToLower(Trim(parts[0])) == "none") {
    *call.out = {};
    return true;
  }
  float distance = 0.0f;
  if (!call.args.LengthAt(0, distance, TransformAxis::Z) || !std::isfinite(distance) ||
      distance < 0.0f) {
    call.diagnostics.warn("html: perspective distance must be a non-negative length; got '" +
                          call.token + "'; ignored");
    return false;
  }
  // CSS Transforms 2 clamps a distance below 1px to 1px. `perspective(0)` would otherwise be
  // rejected and take the rest of the function list down with it, losing a sibling
  // `translateX(10px)` that the browser still applies.
  const float clamped = distance < 1.0f ? 1.0f : distance;
  *call.out = MakePerspective(clamped);
  return true;
}

// The single list of supported transform functions: the parser dispatches on it and
// `IsSupportedTransformFunction` answers from it, so a name cannot be known to one and unknown to the
// other. A hash map, not a linear scan: every transform token passes through this lookup, from the
// subset transformer as well as from `ParseFunction`.
TransformFamilyHandler FindTransformHandler(const std::string& name) {
  static const std::unordered_map<std::string, TransformFamilyHandler> TRANSFORM_FAMILIES = {
      {"translate", ParseTranslate},
      {"translatex", ParseTranslate},
      {"translatey", ParseTranslate},
      {"translatez", ParseTranslate},
      {"translate3d", ParseTranslate},
      {"scale", ParseScale},
      {"scalex", ParseScale},
      {"scaley", ParseScale},
      {"scalez", ParseScale},
      {"scale3d", ParseScale},
      {"rotate", ParseRotate},
      {"rotatex", ParseRotate},
      {"rotatey", ParseRotate},
      {"rotatez", ParseRotate},
      {"rotate3d", ParseRotate3D},
      {"skewx", ParseSkew},
      {"skewy", ParseSkew},
      {"skew", ParseSkew},
      {"matrix", ParseMatrix},
      {"matrix3d", ParseMatrix3D},
      {"perspective", ParsePerspective},
  };
  const auto found = TRANSFORM_FAMILIES.find(name);
  return found == TRANSFORM_FAMILIES.end() ? nullptr : found->second;
}

// Parses one `fn(args)` token. Returns false after emitting a diagnostic.
bool ParseFunction(const std::string& token, HTMLValueParser& valueParser,
                   HTMLDiagnosticSink& diagnostics, Matrix3D* out, float referenceWidthPx,
                   float referenceHeightPx, ZeroScale zeroScale, bool* clampedZeroScale) {
  std::string name;
  std::string args;
  if (!ParseCssFunctionCall(token, name, args)) {
    diagnostics.warn("html: transform '" + token + "' is malformed; ignored");
    return false;
  }
  const TransformFamilyHandler handler = FindTransformHandler(name);
  if (handler == nullptr) {
    diagnostics.warn("html: transform function '" + name +
                     "' is not in the supported subset; ignored");
    return false;
  }
  // Diagnostics keep the authored spelling (`translateX`, `skewX`, ...) so the message points at
  // exactly the token the author wrote.
  const std::string label = Trim(token.substr(0, token.find('(')));
  const auto parts = SplitTopLevelCommas(args);
  const TransformArgs transformArgs = {
      parts, valueParser, referenceWidthPx, referenceHeightPx, zeroScale, clampedZeroScale};
  const TransformCall call = {token, name, label, transformArgs, diagnostics, out};
  return handler(call);
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
  return FindTransformHandler(name) != nullptr;
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
    // `<transform-list>` is `<transform-function>+`, so `rotate(45deg)scale(2)` is legal and needs no
    // separator, while a space between the name and the opening paren is not: the name span has to
    // stay whitespace free.
    size_t openParen = 0;
    size_t closeParen = 0;
    if (!LocateCssFunctionCall(value, index, openParen, closeParen)) {
      return false;
    }
    if (value.find_first_of(" \t\n\v\f\r", index) < openParen) {
      return false;
    }
    out->push_back(Trim(value.substr(index, closeParen - index + 1)));
    index = closeParen + 1;
  }
  return !out->empty();
}

// A declaration CSS drops falls back to the initial `50% 50%`, i.e. the box centre. That centre
// needs a known box size; without one `resolved` stays false and the caller keeps the top-left
// pivot and reports it once.
HTMLTransformOrigin InvalidOrigin(float width, float height) {
  HTMLTransformOrigin origin = {};
  origin.valid = false;
  if (std::isfinite(width) && std::isfinite(height)) {
    origin.point = Point{width * 0.5f, height * 0.5f};
    origin.resolved = true;
  }
  return origin;
}

HTMLTransformOrigin ResolveTransformOrigin(const std::string& value, float width, float height,
                                           HTMLValueParser& valueParser, bool quiet) {
  HTMLTransformOrigin origin = {};
  std::string text = ToLower(Trim(value));
  if (text.empty()) {
    text = "50% 50%";
  }
  const auto tokens = SplitTopLevelWhitespace(text);
  if (tokens.empty()) {
    return origin;
  }
  // A `<position>` takes at most three values (the third is the pivot depth); four or more is not a
  // valid declaration either.
  if (tokens.size() > 3) {
    return InvalidOrigin(width, height);
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

  // An invalid `<position>` drops the declaration, so the initial `50% 50%` applies. Reporting it
  // keeps the author-facing diagnostic honest rather than silently pivoting elsewhere.
  if (invalid) {
    return InvalidOrigin(width, height);
  }

  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  // Every component is validated before any size-dependent early return: an unknown box only decides
  // whether the value can be expressed in pixels, never whether the declaration is legal. Returning
  // early on the axes used to skip the depth and silently drop a legal `30px` third value.
  const OriginTokenAxis axisX = ResolveOriginAxis(tokenX, width, valueParser, x, quiet);
  const OriginTokenAxis axisY = ResolveOriginAxis(tokenY, height, valueParser, y, quiet);
  if (axisX == OriginTokenAxis::TokenInvalid || axisY == OriginTokenAxis::TokenInvalid) {
    return InvalidOrigin(width, height);
  }
  if (tokens.size() == 3) {
    // The third value is a bare depth; a percentage has no basis there.
    z = quiet ? valueParser.parseQuietLengthPx(tokens[2])
              : valueParser.parseAbsoluteLengthPx(tokens[2], /*warn=*/false);
    if (!std::isfinite(z)) {
      return InvalidOrigin(width, height);
    }
  }
  if (axisX == OriginTokenAxis::SizeUnknown || axisY == OriginTokenAxis::SizeUnknown) {
    // Legal declaration, unknown box: keep `valid` (the author wrote something Chromium accepts) and
    // let the caller report the single "cannot be resolved" diagnostic.
    origin.valid = true;
    origin.z = z;
    return origin;
  }
  origin.point = Point{x, y};
  origin.z = z;
  origin.resolved = true;
  return origin;
}

HTMLElementAnchor ResolveElementAnchor(const std::string& originValue,
                                       const HTMLTransformReference& reference, const Layer* layer,
                                       HTMLValueParser& valueParser, bool quiet) {
  HTMLTransformReference size = reference;
  if (std::isnan(size.widthPx) && layer != nullptr) {
    size.widthPx = layer->width;
  }
  if (std::isnan(size.heightPx) && layer != nullptr) {
    size.heightPx = layer->height;
  }
  const auto origin =
      ResolveTransformOrigin(originValue, size.widthPx, size.heightPx, valueParser, quiet);
  HTMLElementAnchor anchor = {};
  anchor.point = origin.point;
  anchor.depth = origin.z;
  anchor.resolved = origin.resolved;
  anchor.valid = origin.valid;
  return anchor;
}

bool ParseCssTransformList(const std::string& value, HTMLValueParser& valueParser,
                           HTMLDiagnosticSink& diagnostics, Matrix3D* out, float referenceWidthPx,
                           float referenceHeightPx, ZeroScale zeroScale, bool* clampedZeroScale) {
  if (out == nullptr) {
    return false;
  }
  const std::string trimmed = Trim(value);
  if (trimmed.empty()) {
    *out = {};
    return false;
  }
  if (ToLower(trimmed) == "none") {
    // `none` is a valid declaration of "no transform", so it resolves to the identity and reports
    // success: `false` only ever means "there is a diagnostic".
    *out = {};
    return true;
  }
  std::vector<std::string> tokens = {};
  if (!SplitTransformFunctions(trimmed, &tokens)) {
    diagnostics.warn("html: transform '" + trimmed + "' is malformed; ignored");
    return false;
  }
  Matrix3D pose = {};
  for (const auto& token : tokens) {
    Matrix3D step = {};
    if (!ParseFunction(token, valueParser, diagnostics, &step, referenceWidthPx, referenceHeightPx,
                       zeroScale, clampedZeroScale)) {
      return false;
    }
    pose = Multiply(pose, step);
  }
  if (!IsFinite(pose)) {
    diagnostics.warn("html: transform '" + trimmed + "' produces a non-finite pose; ignored");
    return false;
  }
  // PAGX requires a 3D pose to be invertible; a singular flat pose stays legal because a 2D
  // matrix has no such constraint (a zero scale is a valid, if empty, 2D transform). With
  // `ClampToEpsilon` a `scale` factor can no longer be the singular one — the clamp runs before the
  // multiply — so what fails here is a singular pose from a `matrix` / `matrix3d` factor, which
  // carries no direction to recover.
  if (!IsFlatTransform(pose) && !IsInvertibleMatrix3D(pose)) {
    diagnostics.warn("html: transform '" + trimmed + "' is not invertible; ignored");
    return false;
  }
  *out = pose;
  return true;
}

PosePlacement PlacePose(const Matrix3D& pose) {
  PosePlacement placement = {};
  // The parent camera is never folded into a pose: it lives on the wrapper layer that PAGX composes
  // outside the pose, so a fixed camera stays fixed while the pose interpolates.
  placement.pose = pose;
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
  // `!= 0`). The projection coefficient is `1 / distance`, so a camera at `distance >= 1 / POSE_EPSILON`
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
      if (std::fabs(matrix.getRowColumn(row, column) - expected) > POSE_EPSILON) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace pagx
