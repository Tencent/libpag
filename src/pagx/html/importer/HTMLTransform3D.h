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

#pragma once

#include <cmath>
#include <string>
#include <vector>
#include "pagx/types/Matrix.h"
#include "pagx/types/Matrix3D.h"
#include "pagx/types/Point.h"

namespace pagx {

class HTMLValueParser;
class HTMLDiagnosticSink;

/**
 * The box size `transform-origin` resolves percentages and keywords against. It equals the CSS box
 * size, except for an `<svg>` sized by its `width` / `height` attributes: those stand in for the
 * reference box only, so layer sizing and other percentage-based properties stay untouched.
 */
struct HTMLTransformReference {
  float widthPx = NAN;
  float heightPx = NAN;
};

/**
 * Resolved CSS `transform-origin` in layer-local pixels. `resolved` is false when the value uses a
 * percentage or keyword form whose reference box size is unknown, or when a token cannot be parsed;
 * callers then fall back to the top-left pivot and warn.
 */
struct HTMLTransformOrigin {
  Point point = {};
  float z = 0.0f;
  bool resolved = false;
};

/**
 * Returns true when `name` (already lower-cased) is a CSS transform function the importer maps onto
 * the PAGX 4x4 pose.
 */
bool IsSupportedTransformFunction(const std::string& name);

/**
 * Splits a CSS `transform` value into its whitespace-separated function calls. Returns false when
 * the value has unbalanced parentheses or trailing garbage after a call.
 */
bool SplitTransformFunctions(const std::string& value, std::vector<std::string>* out);

/**
 * Parses a CSS `transform-origin` value against a box of the given size. Both axes resolve against
 * `width` / `height`, and the optional third value is the pivot depth in pixels.
 */
HTMLTransformOrigin ResolveTransformOrigin(const std::string& value, float width, float height,
                                           HTMLValueParser& valueParser);

/**
 * Parses a CSS `transform` value (a whitespace-separated function list) into a 4x4 pose, composed
 * in CSS order. Emits a diagnostic and returns false when a function is unknown, an argument is
 * invalid, the perspective distance is not positive, a `rotate3d` axis is degenerate, the result is
 * not finite, or a 3D result is not invertible (PAGX rejects such poses).
 */
bool ParseCssTransformList(const std::string& value, HTMLValueParser& valueParser,
                           HTMLDiagnosticSink& diagnostics, Matrix3D* out);

/**
 * Returns true when the pose only acts on the z = 0 plane: no depth translation, no z-dependent
 * linear terms and no projection row. Such a pose is written as a 2D affine, which keeps documents
 * whose 4x4 is only pedantically 3D on the cheap path.
 */
bool IsFlatTransform(const Matrix3D& matrix);

/**
 * Converts a flat pose (see IsFlatTransform) into its 2D affine.
 */
Matrix ToAffine2D(const Matrix3D& matrix);

/**
 * Returns true when the pose's linear 3x3 block is the identity, i.e. it only translates or
 * projects and the pivot therefore cannot change the visible result.
 */
bool IsLinearIdentity(const Matrix3D& matrix);

/**
 * Returns true when the pose carries a projection row of its own (anything other than
 * `(0, 0, 0, 1)`), i.e. it contains a `perspective()` transform.
 */
bool HasProjection(const Matrix3D& matrix);

/**
 * Returns `T(origin) * pose * T(-origin)`, i.e. the pose with the pivot folded in. Used for poses
 * that carry their own projection: CSS conjugates `perspective()` with `transform-origin` while
 * PAGX keeps the projection outside `anchor`, so baking is the only exact encoding.
 */
Matrix3D BakeTransformOrigin(const Matrix3D& pose, const Point& origin, float originZ);

}  // namespace pagx
