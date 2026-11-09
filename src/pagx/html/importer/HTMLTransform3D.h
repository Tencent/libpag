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
                           HTMLDiagnosticSink& diagnostics, Matrix3D* out,
                           float referenceWidthPx = NAN, float referenceHeightPx = NAN);

/**
 * Returns the projection matrix `P` for the CSS perspective distance `distance`: the identity with
 * its bottom row set to `(0, 0, -1/distance, 1)`. `Layer.perspective` builds the same matrix, but a
 * CSS `perspective` declaration lives on the parent and must be pushed into the child pose instead.
 */
Matrix3D PerspectiveMatrix(float distance);

/**
 * Returns true when the pose is invertible. PAGX requires a 3D pose to be invertible, so a pose that
 * gains a projection after parsing (see PushPerspective) has to be re-checked.
 */
bool IsInvertible(const Matrix3D& matrix);

/**
 * Returns `P * pose` for a CSS `perspective` declared on the pose's parent. The projection is pushed
 * into the child pose because PAGX keeps it on the layer that owns the pose; `Layer.perspective`
 * cannot be used for that, as it would only project the declaring layer itself.
 */
Matrix3D PushPerspective(const Matrix3D& pose, float distance, float originX, float originY);

/**
 * Where a pose goes once its parent camera has been folded in. Both the static box path and the
 * animation keyframe path ask this question, and answering it in two places is how they drifted
 * apart, so the classification lives here:
 *
 *  - `valid`   — false when the pushed pose is 3D and not invertible (PAGX rejects such a pose). The
 *                callers decide the consequence: the static path drops the whole transform, the
 *                keyframe path drops that one stop.
 *  - `flat`    — the pushed pose stays on the z = 0 plane, so it can travel on the 2D `matrix`
 *                channel; a pose that only became 3D through the parent camera must not.
 *  - `linearIsIdentity` — the pivot has no effect (pure translation or pure projection), so no
 *                `anchor` is needed.
 *  - `pose`    — the pushed pose itself. It never carries the pivot: `anchor` is applied by the
 *                runtime after interpolation.
 */
struct PosePlacement {
  bool valid = false;
  bool flat = true;
  bool linearIsIdentity = true;
  Matrix3D pose = {};
};

PosePlacement PlacePose(const Matrix3D& pose, float parentPerspectivePx,
                        float parentPerspectiveOriginX, float parentPerspectiveOriginY);

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

}  // namespace pagx
