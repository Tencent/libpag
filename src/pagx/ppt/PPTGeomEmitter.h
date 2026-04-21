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

void EmitPoint(XMLBuilder& out, const char* tag, float x, float y, float ofsX, float ofsY);

void EmitQuadBezTo(XMLBuilder& out, const char* tag, float x0, float y0, float x1, float y1,
                   float ofsX, float ofsY);

void EmitCubicBezTo(XMLBuilder& out, float x0, float y0, float x1, float y1, float x2, float y2,
                    float ofsX, float ofsY);

void EmitContourSegments(XMLBuilder& out, const PathContour& c, float csX, float csY, float sOfsX,
                         float sOfsY);

void EmitContour(XMLBuilder& out, const PathContour& c, float csX, float csY, float sOfsX,
                 float sOfsY);

void EmitBridgedGroup(XMLBuilder& out, const std::vector<PathContour>& contours,
                      const std::vector<size_t>& group, float csX, float csY, float sOfsX,
                      float sOfsY);

void EmitGroupCustGeom(XMLBuilder& out, const std::vector<PathContour>& contours,
                       const std::vector<size_t>& group, int64_t pathWidth, int64_t pathHeight,
                       float scaleX, float scaleY, float scaledOfsX, float scaledOfsY);

/**
 * Emits a single `<a:custGeom>` whose `<a:pathLst>` contains one `<a:path>` per
 * precomputed group (each group is bridged into one path). Unlike
 * `EmitGroupCustGeom` this does not insert the zero-length bounds-marker path
 * that pins multi-shape group bounds — callers that emit a single shape can
 * use this to skip the redundant marker, while callers that emit one shape
 * per group should keep using `EmitGroupCustGeom`.
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

void EmitCustGeomHeader(XMLBuilder& out);

}  // namespace pagx
