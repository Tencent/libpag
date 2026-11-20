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
class Layer;

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
 * A CSS `perspective` camera: the distance to the viewer plus its `perspective-origin` vanishing point,
 * expressed in the coordinates of the layer the projection is folded into.
 *
 * Two frames exist and must not be merged: the inherited bag carries the camera in the *parent's*
 * coordinates (the child has not been placed yet), while the box carries the same camera re-expressed
 * in the element's own local coordinates. `ApplyParentPerspective` performs that one subtraction.
 */
struct HTMLParentCamera {
  // Distance from the viewing plane in px; <= 0 means "no camera".
  float distancePx = 0.0f;
  // Vanishing point; NaN on either axis means the declaring box had no absolute size to resolve
  // `perspective-origin` against.
  Point localOrigin = {NAN, NAN};

  // True when a projection exists at all, i.e. an author declared a positive `perspective`.
  bool Has() const {
    return distancePx > 0.0f && std::isfinite(distancePx);
  }

  // True when that projection also knows its vanishing point; without one it conjugates about the
  // layer origin.
  bool HasOrigin() const {
    return Has() && std::isfinite(localOrigin.x) && std::isfinite(localOrigin.y);
  }
};

/**
 * Resolved CSS `transform-origin` in layer-local pixels. `resolved` is false when the value uses a
 * percentage or keyword form whose reference box size is unknown; callers then fall back to the
 * top-left pivot and warn. A token that does not parse is not merely unresolvable: it invalidates
 * the whole declaration, so `valid` is false and `point` carries the initial `50% 50%`.
 */
struct HTMLTransformOrigin {
  Point point = {};
  float z = 0.0f;
  // The value parsed and every component resolved against the box.
  bool resolved = false;
  // The authored value is not a valid CSS `<position>` (e.g. `top 20px`), so the declaration is
  // dropped and the initial `50% 50%` applies. `point` already carries that fallback.
  bool valid = true;
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
                                           HTMLValueParser& valueParser, bool quiet = false);

/**
 * The pivot of one element's local transform: `transform-origin` in layer-local pixels plus its
 * depth. `resolved` is false when the value needs a box size that no step of the chain supplied; the
 * pivot then falls back to the layer's top-left corner. `valid` mirrors the declaration itself: false
 * means the authored value is not a CSS `<position>` at all, so `point` carries the initial
 * `50% 50%` instead.
 */
struct HTMLElementAnchor {
  Point point = {};
  float depth = 0.0f;
  bool resolved = false;
  bool valid = true;
};

/**
 * Resolves one element's `transform-origin`. The static box path and the keyframe path must arrive at
 * the same pivot, and they used to answer this separately, so the size fallback chain lives here and
 * nowhere else:
 *
 *  1. `reference` — the element's resolved `width` / `height` in px, including an `<svg>`'s
 *     `width` / `height` attributes (see `HTMLStyleCascade::getTransformReferenceSize`). This is the
 *     same data the keyframe path used to re-parse out of the element's own declarations.
 *  2. `layer` — the final layer size. A percentage / intrinsic / content-sized box only carries a px
 *     size once the layer exists; skipping this step is why the animated pivot could resolve while
 *     the static one silently fell back to the top-left corner.
 *
 * Both steps are tried in that order; the first one that resolves a percentage / keyword origin wins.
 * `layer` may be null when the caller runs before the layer exists (the cascade does, for the
 * declaration-validity diagnostic).
 */
HTMLElementAnchor ResolveElementAnchor(const std::string& originValue,
                                       const HTMLTransformReference& reference, const Layer* layer,
                                       HTMLValueParser& valueParser, bool quiet = false);

/**
 * Parses a CSS `transform` value (a whitespace-separated function list) into a 4x4 pose, composed
 * in CSS order. `none` is a valid declaration of "no transform", so it resolves to the identity and
 * reports success. Three failures return false with a diagnostic: an unknown function, an invalid
 * argument (unit, arity, angle), or an unpresentable result (non-finite, or a 3D pose PAGX would
 * reject because it is not invertible). The absent value (empty string) is the only silent false.
 * A `perspective` distance may be zero — it clamps to 1px — and only a negative distance is invalid.
 */
bool ParseCssTransformList(const std::string& value, HTMLValueParser& valueParser,
                           HTMLDiagnosticSink& diagnostics, Matrix3D* out,
                           float referenceWidthPx = NAN, float referenceHeightPx = NAN);

/**
 * Returns `P * pose` for a CSS `perspective` declared on the pose's parent. The projection is pushed
 * into the child pose because PAGX keeps it on the layer that owns the pose; `Layer.perspective`
 * cannot be used for that, as it would only project the declaring layer itself. Two callers need it:
 * `PlacePose` folds the camera into a child pose, and `HTMLLayerBuilder::applyBareParentCamera`
 * materialises it on a `preserve-3d` layer that has no pose of its own.
 */

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
 *
 * The camera is carried by a wrapper layer and never folded into the pose, so the projection only
 * matters when the pose itself leaves the z = 0 plane: on that plane it divides by a z that is zero
 * everywhere. A pivot depth `anchorZ` does not change that — `T(a) * L * T(-a)` maps the plane onto
 * itself for a flat `L` — so a non-zero depth alone must not count as affected.
 */
struct PosePlacement {
  bool valid = false;
  bool flat = true;
  bool linearIsIdentity = true;
  Matrix3D pose = {};
};

PosePlacement PlacePose(const Matrix3D& pose);

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
