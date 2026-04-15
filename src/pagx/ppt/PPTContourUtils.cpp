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

// Tests point containment using endpoint polygon approximation (ray casting).
// For paths with few high-curvature bezier segments, the result may be inaccurate
// near curved edges.
bool PointInsideContour(const Point& pt, const PathContour& contour) {
  bool inside = false;
  auto testEdge = [&](const Point& pi, const Point& pj) {
    if (((pi.y > pt.y) != (pj.y > pt.y)) &&
        (pt.x < (pj.x - pi.x) * (pt.y - pi.y) / (pj.y - pi.y) + pi.x)) {
      inside = !inside;
    }
  };
  Point lastVertex = contour.segs.empty() ? contour.start : SegEndpoint(contour.segs.back());
  testEdge(contour.start, lastVertex);
  Point prev = contour.start;
  for (const auto& seg : contour.segs) {
    Point cur = SegEndpoint(seg);
    testEdge(cur, prev);
    prev = cur;
  }
  return inside;
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

std::vector<int> ComputeContainmentDepths(const std::vector<PathContour>& contours) {
  std::vector<int> depths(contours.size(), 0);
  for (size_t i = 0; i < contours.size(); i++) {
    for (size_t j = 0; j < contours.size(); j++) {
      if (i == j || !contours[j].closed || contours[j].segs.empty()) {
        continue;
      }
      if (PointInsideContour(contours[i].start, contours[j])) {
        depths[i]++;
      }
    }
  }
  return depths;
}

void AdjustWindingForEvenOdd(std::vector<PathContour>& contours, const std::vector<int>& depths) {
  int refSign = 0;
  for (size_t i = 0; i < contours.size(); i++) {
    if (depths[i] == 0 && contours[i].closed && !contours[i].segs.empty()) {
      refSign = (ComputeSignedArea(contours[i]) >= 0) ? 1 : -1;
      break;
    }
  }
  if (refSign == 0 && !contours.empty()) {
    refSign = (ComputeSignedArea(contours[0]) >= 0) ? 1 : -1;
  }
  for (size_t i = 0; i < contours.size(); i++) {
    if (!contours[i].closed || contours[i].segs.empty()) {
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
// contains it.  If multiple disjoint outer contours exist and the endpoint-polygon
// approximation in PointInsideContour produces a false positive, an inner contour
// may be assigned to the wrong parent.  This is unlikely in practice because outer
// contours are typically non-overlapping.
std::vector<std::vector<size_t>> GroupContoursByOutermost(const std::vector<PathContour>& contours,
                                                          const std::vector<int>& depths) {
  std::vector<int> parent(contours.size(), -1);
  for (size_t i = 0; i < contours.size(); i++) {
    if (depths[i] == 0) {
      parent[i] = static_cast<int>(i);
    } else {
      for (size_t j = 0; j < contours.size(); j++) {
        if (depths[j] == 0 && j != i && contours[j].closed && !contours[j].segs.empty()) {
          if (PointInsideContour(contours[i].start, contours[j])) {
            parent[i] = static_cast<int>(j);
            break;
          }
        }
      }
      if (parent[i] < 0) {
        parent[i] = static_cast<int>(i);
      }
    }
  }

  std::vector<int> groupIndex(contours.size(), -1);
  std::vector<std::vector<size_t>> groups;
  for (size_t i = 0; i < contours.size(); i++) {
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
  auto depths = ComputeContainmentDepths(contours);
  if (fillRule == FillRule::EvenOdd) {
    AdjustWindingForEvenOdd(contours, depths);
  }
  return GroupContoursByOutermost(contours, depths);
}

Point TransformPoint(const Matrix& t, const Point& p) {
  return {t.a * p.x + t.c * p.y + t.tx, t.b * p.x + t.d * p.y + t.ty};
}

void TransformContours(std::vector<PathContour>& contours, const Matrix& transform) {
  for (auto& c : contours) {
    c.start = TransformPoint(transform, c.start);
    for (auto& seg : c.segs) {
      int ptCount = PathData::PointsPerVerb(seg.verb);
      for (int i = 0; i < ptCount; i++) {
        seg.pts[i] = TransformPoint(transform, seg.pts[i]);
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
