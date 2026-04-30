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

#pragma once

#include <cstdint>
#include <vector>
#include "pagx/ppt/PPTContourUtils.h"
#include "pagx/types/FillRule.h"

namespace pagx {

class XMLBuilder;

// 1 px = 1/96 inch = 914400/96 = 9525 EMU (English Metric Units).
inline constexpr double EMU_PER_PX = 9525.0;

int64_t PxToEMU(float px);

/**
 * Emits a single `<a:custGeom>` with a `<a:pathLst>` containing one `<a:path>`
 * per precomputed group. Groups with nested inner contours are stitched into
 * a single bridged path (zero-width bridge lines) so even-odd fill renders
 * the expected holes. Used when the caller emits one shape for the whole
 * contour set and does not need the multi-shape bounds marker.
 */
void EmitContourGeomFromGroups(XMLBuilder& out, const std::vector<PathContour>& contours,
                               const std::vector<std::vector<size_t>>& groups, int64_t pathWidth,
                               int64_t pathHeight, float scaleX, float scaleY, float scaledOfsX,
                               float scaledOfsY);

/**
 * Emits a single `<a:custGeom>` whose `<a:pathLst>` contains one `<a:path>`
 * with every contour concatenated (no bridging, no per-group splitting).
 */
void EmitFlatContourGeom(XMLBuilder& out, const std::vector<PathContour>& contours,
                         int64_t pathWidth, int64_t pathHeight, float scaleX, float scaleY,
                         float scaledOfsX, float scaledOfsY);

/**
 * Selects how EmitGroupCustGeom places the corner bounds markers that pin the
 * content bounding box to the declared (pathWidth, pathHeight). WeChat requires
 * drawable segments at both corners to recognise the bbox, so the marker is
 * always composed of zero-length lines.
 *
 * InlineSegments
 *   Two `moveTo + lnTo` zero-length segments share the same `<a:path>` as the
 *   main geometry. Use when the enclosing sp has no stroke (e.g. text-as-path
 *   glyph shapes) so round caps cannot paint visible dots at the corner points.
 *
 * StandaloneStrokelessPath
 *   The two corner segments are emitted in a separate `<a:path stroke="0"
 *   fill="none">` placed before the main geometry path. Use when the enclosing
 *   sp may carry a round-capped stroke: the per-path `stroke="0"` keeps the
 *   markers from rendering as dots while the main geometry still honours the
 *   sp-level `<a:ln>`.
 */
enum class BoundsMarkerStyle {
  InlineSegments,
  StandaloneStrokelessPath,
};

/**
 * Emits a single `<a:custGeom>` for one group that also includes the
 * zero-length bounds-marker path pinning the content bounding box to the
 * declared (pathWidth, pathHeight). Use this when emitting one shape per
 * group so each shape carries its own bounds marker.
 */
void EmitGroupCustGeom(XMLBuilder& out, const std::vector<PathContour>& contours,
                       const std::vector<size_t>& group, int64_t pathWidth, int64_t pathHeight,
                       float scaleX, float scaleY, float scaledOfsX, float scaledOfsY,
                       BoundsMarkerStyle markerStyle);

}  // namespace pagx
