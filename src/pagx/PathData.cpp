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

#include "pagx/nodes/PathData.h"
#include <cmath>
#include <memory>

namespace pagx {

// Point-wise lerp helper for PathData::Interpolate (the project forbids lambdas).
static Point MixPathPoint(const Point& a, const Point& b, float t) {
  return Point{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

PathData* PathData::Interpolate(const PathData* start, const PathData* end, float t) {
  if (start == nullptr || end == nullptr) {
    return const_cast<PathData*>(start);
  }
  const auto& verbsStart = start->verbs();
  const auto& verbsEnd = end->verbs();
  // Different command sequences cannot be paired point-by-point; spec 6.1 falls back to Hold.
  if (verbsStart.size() != verbsEnd.size()) {
    return const_cast<PathData*>(start);
  }
  for (size_t i = 0; i < verbsStart.size(); i++) {
    if (verbsStart[i] != verbsEnd[i]) {
      return const_cast<PathData*>(start);
    }
  }
  static thread_local auto scratch = std::unique_ptr<PathData>(new PathData());
  // The scratch is reused across calls, so reset its command lists before replaying.
  *scratch = PathData();
  const auto& pointsStart = start->points();
  const auto& pointsEnd = end->points();
  size_t pointIndex = 0;
  for (auto verb : verbsStart) {
    switch (verb) {
      case PathVerb::Move: {
        auto p = MixPathPoint(pointsStart[pointIndex], pointsEnd[pointIndex], t);
        scratch->moveTo(p.x, p.y);
        pointIndex++;
        break;
      }
      case PathVerb::Line: {
        auto p = MixPathPoint(pointsStart[pointIndex], pointsEnd[pointIndex], t);
        scratch->lineTo(p.x, p.y);
        pointIndex++;
        break;
      }
      case PathVerb::Quad: {
        auto c = MixPathPoint(pointsStart[pointIndex], pointsEnd[pointIndex], t);
        auto p = MixPathPoint(pointsStart[pointIndex + 1], pointsEnd[pointIndex + 1], t);
        scratch->quadTo(c.x, c.y, p.x, p.y);
        pointIndex += 2;
        break;
      }
      case PathVerb::Cubic: {
        auto c1 = MixPathPoint(pointsStart[pointIndex], pointsEnd[pointIndex], t);
        auto c2 = MixPathPoint(pointsStart[pointIndex + 1], pointsEnd[pointIndex + 1], t);
        auto p = MixPathPoint(pointsStart[pointIndex + 2], pointsEnd[pointIndex + 2], t);
        scratch->cubicTo(c1.x, c1.y, c2.x, c2.y, p.x, p.y);
        pointIndex += 3;
        break;
      }
      case PathVerb::Close:
        scratch->close();
        break;
    }
  }
  return scratch.get();
}

int PathData::PointsPerVerb(PathVerb verb) {
  switch (verb) {
    case PathVerb::Move:
      return 1;
    case PathVerb::Line:
      return 1;
    case PathVerb::Quad:
      return 2;
    case PathVerb::Cubic:
      return 3;
    case PathVerb::Close:
      return 0;
    default:
      return 0;
  }
}

void PathData::moveTo(float x, float y) {
  _verbs.push_back(PathVerb::Move);
  _points.push_back({x, y});
  _boundsDirty = true;
}

void PathData::lineTo(float x, float y) {
  _verbs.push_back(PathVerb::Line);
  _points.push_back({x, y});
  _boundsDirty = true;
}

void PathData::quadTo(float cx, float cy, float x, float y) {
  _verbs.push_back(PathVerb::Quad);
  _points.push_back({cx, cy});
  _points.push_back({x, y});
  _boundsDirty = true;
}

void PathData::cubicTo(float c1x, float c1y, float c2x, float c2y, float x, float y) {
  _verbs.push_back(PathVerb::Cubic);
  _points.push_back({c1x, c1y});
  _points.push_back({c2x, c2y});
  _points.push_back({x, y});
  _boundsDirty = true;
}

void PathData::close() {
  _verbs.push_back(PathVerb::Close);
}

PathData& PathData::operator=(const PathData& other) {
  if (this != &other) {
    _verbs = other._verbs;
    _points = other._points;
    _boundsDirty = true;
  }
  return *this;
}

void PathData::transform(const Matrix& matrix) {
  for (auto& point : _points) {
    point = matrix.mapPoint(point);
  }
  _boundsDirty = true;
}

Rect PathData::getBounds() {
  if (!_boundsDirty) {
    return _cachedBounds;
  }

  _cachedBounds = {};
  if (_points.empty()) {
    _boundsDirty = false;
    return _cachedBounds;
  }

  float minX = _points[0].x;
  float minY = _points[0].y;
  float maxX = _points[0].x;
  float maxY = _points[0].y;

  for (size_t i = 1; i < _points.size(); i++) {
    float x = _points[i].x;
    float y = _points[i].y;
    minX = std::min(minX, x);
    minY = std::min(minY, y);
    maxX = std::max(maxX, x);
    maxY = std::max(maxY, y);
  }

  _cachedBounds = Rect::MakeLTRB(minX, minY, maxX, maxY);
  _boundsDirty = false;
  return _cachedBounds;
}

}  // namespace pagx
