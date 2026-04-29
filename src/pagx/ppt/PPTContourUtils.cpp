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

#include "pagx/ppt/PPTContourUtils.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace pagx {

Point SegEndpoint(const PathSeg& seg) {
  if (seg.verb == PathVerb::Quad) {
    return seg.pts[1];
  }
  if (seg.verb == PathVerb::Cubic) {
    return seg.pts[2];
  }
  return seg.pts[0];
}

// Approximates the signed area using segment endpoints only (polygon approximation).
// For paths with few high-curvature bezier segments (e.g., a circle from 4 cubics),
// the winding direction may be inaccurate.
float ComputeSignedArea(const PathContour& contour) {
  float area = 0;
  Point prev = contour.start;
  for (const auto& seg : contour.segs) {
    Point next = SegEndpoint(seg);
    area += (prev.x * next.y - next.x * prev.y);
    prev = next;
  }
  area += (prev.x * contour.start.y - contour.start.x * prev.y);
  return area;
}

void ReverseContour(PathContour& c) {
  if (c.segs.empty()) {
    return;
  }
  size_t n = c.segs.size();
  Point originalStart = c.start;
  c.start = SegEndpoint(c.segs[n - 1]);

  std::vector<PathSeg> rev;
  rev.reserve(n);
  for (int i = static_cast<int>(n) - 1; i >= 0; i--) {
    Point dest = (i > 0) ? SegEndpoint(c.segs[i - 1]) : originalStart;
    const auto& seg = c.segs[i];
    PathSeg reversed;
    if (seg.verb == PathVerb::Cubic) {
      reversed.verb = PathVerb::Cubic;
      reversed.pts[0] = seg.pts[1];
      reversed.pts[1] = seg.pts[0];
      reversed.pts[2] = dest;
    } else if (seg.verb == PathVerb::Quad) {
      reversed.verb = PathVerb::Quad;
      reversed.pts[0] = seg.pts[0];
      reversed.pts[1] = dest;
    } else {
      reversed.verb = PathVerb::Line;
      reversed.pts[0] = dest;
    }
    rev.push_back(reversed);
  }
  c.segs = std::move(rev);
}

// Half-edge ray-cast test: returns true when the horizontal ray from `pt` to +x
// crosses the (pi, pj) edge. Caller XORs this into a running `inside` flag so
// that an odd number of crossings means the point is inside the polygon.
static bool RayCrossesEdge(const Point& pt, const Point& pi, const Point& pj) {
  return ((pi.y > pt.y) != (pj.y > pt.y)) &&
         (pt.x < (pj.x - pi.x) * (pt.y - pi.y) / (pj.y - pi.y) + pi.x);
}

// Tests point containment using endpoint polygon approximation (ray casting).
// For paths with few high-curvature bezier segments, the result may be inaccurate
// near curved edges.
bool PointInsideContour(const Point& pt, const PathContour& contour) {
  bool inside = false;
  Point lastVertex = contour.segs.empty() ? contour.start : SegEndpoint(contour.segs.back());
  if (RayCrossesEdge(pt, contour.start, lastVertex)) {
    inside = !inside;
  }
  Point prev = contour.start;
  for (const auto& seg : contour.segs) {
    Point cur = SegEndpoint(seg);
    if (RayCrossesEdge(pt, cur, prev)) {
      inside = !inside;
    }
    prev = cur;
  }
  return inside;
}

// Tests whether `inner` is spatially nested inside `outer`. Samples inner.start
// plus the midpoint of inner's first segment chord and returns true if any
// sample lies strictly inside `outer`. The midpoint fallback exists because
// inner.start can coincide with a polygon-approximation vertex of `outer` when
// both contours come from a path boolean (e.g. tgfx::PathOp::XOR emits an inner
// hole whose start point is tangent to the outer boundary), and ray casting on
// a boundary vertex is numerically unstable and often misses the containment.
bool ContourInsideContour(const PathContour& inner, const PathContour& outer) {
  if (PointInsideContour(inner.start, outer)) {
    return true;
  }
  if (inner.segs.empty()) {
    return false;
  }
  Point next = SegEndpoint(inner.segs.front());
  Point midpoint = {(inner.start.x + next.x) * 0.5f, (inner.start.y + next.y) * 0.5f};
  return PointInsideContour(midpoint, outer);
}

std::vector<PathContour> ParsePathContours(const PathData* data) {
  std::vector<PathContour> contours;
  const auto& verbs = data->verbs();
  const auto& points = data->points();
  size_t ptIndex = 0;
  for (const auto& verb : verbs) {
    if (verb == PathVerb::Move) {
      PathContour c;
      c.start = points[ptIndex++];
      contours.push_back(std::move(c));
    } else if (!contours.empty()) {
      if (verb == PathVerb::Close) {
        contours.back().closed = true;
      } else {
        PathSeg seg;
        seg.verb = verb;
        int ptCount = PathData::PointsPerVerb(verb);
        for (int i = 0; i < ptCount; i++) {
          seg.pts[i] = points[ptIndex++];
        }
        contours.back().segs.push_back(seg);
      }
    }
  }
  return contours;
}

// A contour can only contain another when it is a closed loop with at least
// one segment. Pre-computing this validity bit avoids re-evaluating it on
// every i/j pair of the O(n²) containment scan below.
static std::vector<bool> ComputeContainerValidity(const std::vector<PathContour>& contours) {
  std::vector<bool> valid(contours.size(), false);
  for (size_t i = 0; i < contours.size(); i++) {
    valid[i] = contours[i].closed && !contours[i].segs.empty();
  }
  return valid;
}

// Builds the (i, j) containment matrix in one O(n²) pass: entry (i, j) is
// true when contour i lies inside contour j. Shared by the depth calculation
// and the parent-assignment step so that ContourInsideContour — the expensive
// ray-cast — runs at most once per ordered pair. `inside` is laid out row-major
// as `uint8_t` rather than `std::vector<bool>` so reads/writes don't pay the
// bit-packing cost on the hot inner loop.
static std::vector<uint8_t> ComputeInsideMatrix(const std::vector<PathContour>& contours,
                                                const std::vector<bool>& valid) {
  size_t n = contours.size();
  std::vector<uint8_t> inside(n * n, 0);
  for (size_t i = 0; i < n; i++) {
    for (size_t j = 0; j < n; j++) {
      if (i == j || !valid[j]) {
        continue;
      }
      if (ContourInsideContour(contours[i], contours[j])) {
        inside[i * n + j] = 1;
      }
    }
  }
  return inside;
}

static std::vector<int> ComputeDepthsFromMatrix(size_t n, const std::vector<uint8_t>& inside) {
  std::vector<int> depths(n, 0);
  for (size_t i = 0; i < n; i++) {
    int d = 0;
    const uint8_t* row = inside.data() + i * n;
    for (size_t j = 0; j < n; j++) {
      d += row[j];
    }
    depths[i] = d;
  }
  return depths;
}

static void AdjustWindingForEvenOdd(std::vector<PathContour>& contours,
                                    const std::vector<int>& depths,
                                    const std::vector<bool>& valid) {
  int refSign = 0;
  for (size_t i = 0; i < contours.size(); i++) {
    if (depths[i] == 0 && valid[i]) {
      refSign = (ComputeSignedArea(contours[i]) >= 0) ? 1 : -1;
      break;
    }
  }
  if (refSign == 0 && !contours.empty()) {
    refSign = (ComputeSignedArea(contours[0]) >= 0) ? 1 : -1;
  }
  for (size_t i = 0; i < contours.size(); i++) {
    if (!valid[i]) {
      continue;
    }
    float area = ComputeSignedArea(contours[i]);
    int currentSign = (area >= 0) ? 1 : -1;
    int targetSign = (depths[i] % 2 == 0) ? refSign : -refSign;
    if (currentSign != targetSign) {
      ReverseContour(contours[i]);
    }
  }
}

// Assigns each inner contour to the first (by array order) depth-0 contour that
// contains it, reusing the pre-computed inside matrix so no ContourInsideContour
// call is repeated. If multiple disjoint outer contours exist and the endpoint-
// polygon approximation in PointInsideContour produces a false positive, an inner
// contour may be assigned to the wrong parent. This is unlikely in practice
// because outer contours are typically non-overlapping.
static std::vector<std::vector<size_t>> GroupContoursByOutermost(
    const std::vector<PathContour>& contours, const std::vector<int>& depths,
    const std::vector<bool>& valid, const std::vector<uint8_t>& inside) {
  size_t n = contours.size();
  std::vector<int> parent(n, -1);
  for (size_t i = 0; i < n; i++) {
    if (depths[i] == 0) {
      parent[i] = static_cast<int>(i);
      continue;
    }
    const uint8_t* row = inside.data() + i * n;
    for (size_t j = 0; j < n; j++) {
      if (j != i && depths[j] == 0 && valid[j] && row[j]) {
        parent[i] = static_cast<int>(j);
        break;
      }
    }
    if (parent[i] < 0) {
      parent[i] = static_cast<int>(i);
    }
  }

  std::vector<int> groupIndex(n, -1);
  std::vector<std::vector<size_t>> groups;
  for (size_t i = 0; i < n; i++) {
    int p = parent[i];
    if (groupIndex[p] < 0) {
      groupIndex[p] = static_cast<int>(groups.size());
      groups.push_back({i});
    } else {
      groups[groupIndex[p]].push_back(i);
    }
  }
  return groups;
}

std::vector<std::vector<size_t>> PrepareContourGroups(std::vector<PathContour>& contours,
                                                      FillRule fillRule) {
  auto valid = ComputeContainerValidity(contours);
  auto inside = ComputeInsideMatrix(contours, valid);
  auto depths = ComputeDepthsFromMatrix(contours.size(), inside);
  if (fillRule == FillRule::EvenOdd) {
    AdjustWindingForEvenOdd(contours, depths, valid);
  }
  return GroupContoursByOutermost(contours, depths, valid, inside);
}

void TransformContours(std::vector<PathContour>& contours, const Matrix& transform) {
  for (auto& c : contours) {
    c.start = transform.mapPoint(c.start);
    for (auto& seg : c.segs) {
      int ptCount = PathData::PointsPerVerb(seg.verb);
      for (int i = 0; i < ptCount; i++) {
        seg.pts[i] = transform.mapPoint(seg.pts[i]);
      }
    }
  }
}

void AppendTransformedContours(std::vector<PathContour>& allContours, const PathData* pathData,
                               const Matrix& transform) {
  auto contours = ParsePathContours(pathData);
  TransformContours(contours, transform);
  allContours.insert(allContours.end(), std::make_move_iterator(contours.begin()),
                     std::make_move_iterator(contours.end()));
}

void ExpandBounds(float& minX, float& minY, float& maxX, float& maxY, const Point& p) {
  minX = std::min(minX, p.x);
  minY = std::min(minY, p.y);
  maxX = std::max(maxX, p.x);
  maxY = std::max(maxY, p.y);
}

}  // namespace pagx
