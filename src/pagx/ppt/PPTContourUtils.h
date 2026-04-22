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

#include <vector>
#include "pagx/nodes/PathData.h"
#include "pagx/types/FillRule.h"
#include "pagx/types/Matrix.h"
#include "pagx/types/Point.h"

namespace pagx {

struct PathSeg {
  PathVerb verb = PathVerb::Move;
  Point pts[3] = {};
};

struct PathContour {
  Point start = {};
  std::vector<PathSeg> segs;
  bool closed = false;
};

Point SegEndpoint(const PathSeg& seg);

float ComputeSignedArea(const PathContour& contour);

void ReverseContour(PathContour& c);

bool PointInsideContour(const Point& pt, const PathContour& contour);

bool ContourInsideContour(const PathContour& inner, const PathContour& outer);

std::vector<PathContour> ParsePathContours(const PathData* data);

std::vector<int> ComputeContainmentDepths(const std::vector<PathContour>& contours);

void AdjustWindingForEvenOdd(std::vector<PathContour>& contours, const std::vector<int>& depths);

std::vector<std::vector<size_t>> GroupContoursByOutermost(const std::vector<PathContour>& contours,
                                                          const std::vector<int>& depths);

std::vector<std::vector<size_t>> PrepareContourGroups(std::vector<PathContour>& contours,
                                                      FillRule fillRule);

Point TransformPoint(const Matrix& t, const Point& p);

void TransformContours(std::vector<PathContour>& contours, const Matrix& transform);

void AppendTransformedContours(std::vector<PathContour>& allContours, const PathData* pathData,
                               const Matrix& transform);

void ExpandBounds(float& minX, float& minY, float& maxX, float& maxY, const Point& p);

}  // namespace pagx
