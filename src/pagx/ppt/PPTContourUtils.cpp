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
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>

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

static double Cross(const Point& a, const Point& b) {
  return static_cast<double>(a.x) * b.y - static_cast<double>(b.x) * a.y;
}

// 2x signed area contribution of one segment from `prev` to its endpoint using
// Green's theorem ∮ x dy. Exact for Line/Quad/Cubic so the winding sign is
// correct even for paths built from few high-curvature beziers (e.g. a circle
// from 4 cubics). `double` accumulation keeps precision under large coordinates.
static double SegmentTwiceSignedArea(const PathSeg& seg, const Point& prev) {
  if (seg.verb == PathVerb::Quad) {
    const Point& p0 = prev;
    const Point& p1 = seg.pts[0];
    const Point& p2 = seg.pts[1];
    // chord contribution + (2/3) * 2A(triangle P0,P1,P2)
    double triangle = Cross(p0, p1) + Cross(p1, p2) + Cross(p2, p0);
    return Cross(p0, p2) + (2.0 / 3.0) * triangle;
  }
  if (seg.verb == PathVerb::Cubic) {
    const Point& p0 = prev;
    const Point& p1 = seg.pts[0];
    const Point& p2 = seg.pts[1];
    const Point& p3 = seg.pts[2];
    double c01 = Cross(p0, p1);
    double c02 = Cross(p0, p2);
    double c03 = Cross(p0, p3);
    double c12 = Cross(p1, p2);
    double c13 = Cross(p1, p3);
    double c23 = Cross(p2, p3);
    return (6.0 * c01 + 3.0 * c02 + c03 + 3.0 * c12 + 3.0 * c13 + 6.0 * c23) / 10.0;
  }
  // Line (and Move/Close defensively fall through as a straight edge to endpoint).
  return Cross(prev, seg.pts[0]);
}

// Computes the exact signed area (actually 2x, sign preserved) by integrating
// along each bezier segment analytically. Unlike an endpoint-polygon
// approximation, this is robust to paths with few high-curvature curves.
float ComputeSignedArea(const PathContour& contour) {
  double area = 0;
  Point prev = contour.start;
  for (const auto& seg : contour.segs) {
    area += SegmentTwiceSignedArea(seg, prev);
    prev = SegEndpoint(seg);
  }
  area += Cross(prev, contour.start);
  return static_cast<float>(area);
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
// that an odd number of crossings means the point is inside the polygon. The
// intersection x-coordinate is computed in `double` to avoid float cancellation
// on near-horizontal edges and under large coordinates.
static bool RayCrossesEdge(const Point& pt, const Point& pi, const Point& pj) {
  if ((pi.y > pt.y) == (pj.y > pt.y)) {
    return false;
  }
  double dy = static_cast<double>(pj.y) - pi.y;
  double xIntersect =
      (static_cast<double>(pj.x) - pi.x) * (static_cast<double>(pt.y) - pi.y) / dy + pi.x;
  return static_cast<double>(pt.x) < xIntersect;
}

// Tests point containment using endpoint polygon approximation (ray casting).
// For paths with few high-curvature bezier segments, the result may be inaccurate
// near curved edges.
bool PointInsideContour(const Point& pt, const PathContour& contour) {
  if (contour.segs.empty()) {
    return false;
  }
  bool inside = false;
  Point prev = contour.start;
  for (const auto& seg : contour.segs) {
    Point cur = SegEndpoint(seg);
    if (RayCrossesEdge(pt, prev, cur)) {
      inside = !inside;
    }
    prev = cur;
  }
  // Closing edge from the last vertex back to the start.
  if (RayCrossesEdge(pt, prev, contour.start)) {
    inside = !inside;
  }
  return inside;
}

// Returns the true curve midpoint (t=0.5) of a segment starting at `start`, via
// closed-form de Casteljau. Using the curve midpoint instead of the chord
// midpoint makes the ContourInsideContour fallback robust for high-curvature
// segments where the chord midpoint may lie outside the actual curve.
static Point SegmentMidpoint(const PathSeg& seg, const Point& start) {
  if (seg.verb == PathVerb::Quad) {
    const Point& p0 = start;
    const Point& p1 = seg.pts[0];
    const Point& p2 = seg.pts[1];
    return {0.25f * p0.x + 0.5f * p1.x + 0.25f * p2.x, 0.25f * p0.y + 0.5f * p1.y + 0.25f * p2.y};
  }
  if (seg.verb == PathVerb::Cubic) {
    const Point& p0 = start;
    const Point& p1 = seg.pts[0];
    const Point& p2 = seg.pts[1];
    const Point& p3 = seg.pts[2];
    return {0.125f * (p0.x + 3.0f * p1.x + 3.0f * p2.x + p3.x),
            0.125f * (p0.y + 3.0f * p1.y + 3.0f * p2.y + p3.y)};
  }
  const Point& p1 = seg.pts[0];
  return {(start.x + p1.x) * 0.5f, (start.y + p1.y) * 0.5f};
}

// Tests whether `inner` is spatially nested inside `outer`. Samples inner.start
// plus the true curve midpoint (t=0.5) of inner's first segment and returns
// true if any sample lies strictly inside `outer`. The midpoint fallback exists
// because inner.start can coincide with a polygon-approximation vertex of
// `outer` when both contours come from a path boolean (e.g. tgfx::PathOp::XOR
// emits an inner hole whose start point is tangent to the outer boundary), and
// ray casting on a boundary vertex is numerically unstable and often misses the
// containment.
bool ContourInsideContour(const PathContour& inner, const PathContour& outer) {
  if (PointInsideContour(inner.start, outer)) {
    return true;
  }
  if (inner.segs.empty()) {
    return false;
  }
  Point midpoint = SegmentMidpoint(inner.segs.front(), inner.start);
  return PointInsideContour(midpoint, outer);
}

std::vector<PathContour> ParsePathContours(const PathData* data) {
  std::vector<PathContour> contours;
  const auto& verbs = data->verbs();
  const auto& points = data->points();
  size_t ptIndex = 0;
  for (const auto& verb : verbs) {
    if (verb == PathVerb::Move) {
      assert(ptIndex < points.size());
      contours.emplace_back();
      contours.back().start = points[ptIndex++];
    } else if (!contours.empty()) {
      if (verb == PathVerb::Close) {
        contours.back().closed = true;
      } else {
        PathSeg seg;
        seg.verb = verb;
        int ptCount = PathData::PointsPerVerb(verb);
        for (int i = 0; i < ptCount; i++) {
          assert(ptIndex < points.size());
          seg.pts[i] = points[ptIndex++];
        }
        contours.back().segs.push_back(seg);
      }
    }
  }
  return contours;
}

// Per-contour metadata shared by the bridging pipeline: axis-aligned bounds
// (for pair pruning), a "can contain" flag (only closed rings with at least
// one segment can hold another contour), and the row-major inside-matrix —
// entry (i, j) is true when contour i lies inside contour j. Building all
// three once lets depth calculation and parent assignment reuse the same
// data without repeating the expensive ContourInsideContour ray cast.
struct ContourBounds {
  float minX;
  float minY;
  float maxX;
  float maxY;

  bool disjoint(const ContourBounds& o) const {
    return maxX < o.minX || o.maxX < minX || maxY < o.minY || o.maxY < minY;
  }
};

struct ContainmentTable {
  std::vector<ContourBounds> bounds;
  std::vector<bool> canContain;
  // inside[i * n + j] == 1  <=>  contour i is inside contour j.
  // uint8_t (not vector<bool>) to avoid bit-packing on the hot inner loop.
  std::vector<uint8_t> inside;
  size_t n = 0;

  bool isInside(size_t i, size_t j) const {
    assert(i < n && j < n);
    return inside[i * n + j] != 0;
  }
};

static void ExpandBoundsBy(ContourBounds& b, const Point& p) {
  b.minX = std::min(b.minX, p.x);
  b.minY = std::min(b.minY, p.y);
  b.maxX = std::max(b.maxX, p.x);
  b.maxY = std::max(b.maxY, p.y);
}

static ContourBounds ComputeOneBounds(const PathContour& c) {
  constexpr float inf = std::numeric_limits<float>::infinity();
  ContourBounds b{inf, inf, -inf, -inf};
  ExpandBoundsBy(b, c.start);
  for (const auto& seg : c.segs) {
    int ptCount = PathData::PointsPerVerb(seg.verb);
    for (int k = 0; k < ptCount; k++) {
      ExpandBoundsBy(b, seg.pts[k]);
    }
  }
  return b;
}

// Builds the containment table in one O(n²) pass. Bbox pruning works because
// ContourInsideContour samples points from inner's control-point bbox (a
// superset of the curves' convex hull). When inner's bbox is disjoint from
// outer's bbox, both samples must fall outside outer, so we can safely skip
// the ray cast.
static ContainmentTable BuildContainmentTable(const std::vector<PathContour>& contours) {
  ContainmentTable table;
  const size_t n = contours.size();
  table.n = n;
  table.bounds.reserve(n);
  table.canContain.assign(n, false);
  table.inside.assign(n * n, 0);

  for (size_t i = 0; i < n; i++) {
    table.bounds.push_back(ComputeOneBounds(contours[i]));
    table.canContain[i] = contours[i].closed && !contours[i].segs.empty();
  }
  for (size_t i = 0; i < n; i++) {
    for (size_t j = 0; j < n; j++) {
      if (i == j || !table.canContain[j]) {
        continue;
      }
      if (table.bounds[i].disjoint(table.bounds[j])) {
        continue;
      }
      if (ContourInsideContour(contours[i], contours[j])) {
        table.inside[i * n + j] = 1;
      }
    }
  }
  return table;
}

// Depth of contour i is the number of other contours that strictly contain it.
static std::vector<int> ComputeDepths(const ContainmentTable& table) {
  std::vector<int> depths(table.n, 0);
  for (size_t i = 0; i < table.n; i++) {
    const uint8_t* row = table.inside.data() + i * table.n;
    int d = 0;
    for (size_t j = 0; j < table.n; j++) {
      d += row[j];
    }
    depths[i] = d;
  }
  return depths;
}

// Picks the orientation sign that depth-0 (outermost) contours should use,
// falling back to any valid contour if none have depth 0. Returns 0 only when
// no contour is valid, in which case the caller must skip winding adjustment.
static int ChooseReferenceSign(const std::vector<int>& signs, const std::vector<int>& depths,
                               const std::vector<bool>& canContain) {
  const size_t n = signs.size();
  for (size_t i = 0; i < n; i++) {
    if (canContain[i] && depths[i] == 0) {
      return signs[i];
    }
  }
  for (size_t i = 0; i < n; i++) {
    if (canContain[i]) {
      return signs[i];
    }
  }
  return 0;
}

// For even-odd fill, ensure alternating winding by depth: depth-0 contours
// share the outermost sign, depth-1 contours use the opposite, and so on.
// Mis-oriented contours are reversed in place. PowerPoint's stitching relies
// on this invariant to render holes correctly.
static void AdjustWindingForEvenOdd(std::vector<PathContour>& contours,
                                    const std::vector<int>& depths,
                                    const std::vector<bool>& canContain) {
  const size_t n = contours.size();
  std::vector<int> signs(n, 0);
  for (size_t i = 0; i < n; i++) {
    if (canContain[i]) {
      signs[i] = (ComputeSignedArea(contours[i]) >= 0) ? 1 : -1;
    }
  }
  int refSign = ChooseReferenceSign(signs, depths, canContain);
  if (refSign == 0) {
    return;
  }
  for (size_t i = 0; i < n; i++) {
    if (!canContain[i]) {
      continue;
    }
    int targetSign = (depths[i] % 2 == 0) ? refSign : -refSign;
    if (signs[i] != targetSign) {
      ReverseContour(contours[i]);
    }
  }
}

// For each contour, returns the index of the depth-0 contour that will own it
// in the bridged output. Depth-0 contours own themselves; any other contour is
// assigned to the first depth-0 contour (by array order) that contains it.
// Orphans (no containing depth-0 contour, which should not happen for
// well-formed input but is handled defensively) are made their own root so
// they still appear as a standalone group.
//
// If the endpoint-polygon approximation in PointInsideContour produces a false
// positive when multiple disjoint depth-0 contours exist, an inner contour may
// end up assigned to the wrong parent. This is unlikely in practice because
// depth-0 contours are typically non-overlapping.
static std::vector<int> AssignParents(const ContainmentTable& table,
                                      const std::vector<int>& depths) {
  const size_t n = table.n;
  std::vector<int> parent(n, -1);
  for (size_t i = 0; i < n; i++) {
    if (depths[i] == 0) {
      parent[i] = static_cast<int>(i);
      continue;
    }
    for (size_t j = 0; j < n; j++) {
      if (j != i && depths[j] == 0 && table.canContain[j] && table.isInside(i, j)) {
        parent[i] = static_cast<int>(j);
        break;
      }
    }
    if (parent[i] < 0) {
      parent[i] = static_cast<int>(i);
    }
  }
  return parent;
}

// Collects contours sharing the same parent (outermost ancestor) into one
// group. Group order follows first-appearance of each root so the output is
// deterministic, and each root sits at index 0 of its group.
static std::vector<std::vector<size_t>> BuildGroupsFromParents(const std::vector<int>& parent) {
  const size_t n = parent.size();
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
  auto table = BuildContainmentTable(contours);
  auto depths = ComputeDepths(table);
  if (fillRule == FillRule::EvenOdd) {
    AdjustWindingForEvenOdd(contours, depths, table.canContain);
  }
  auto parent = AssignParents(table, depths);
  return BuildGroupsFromParents(parent);
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
