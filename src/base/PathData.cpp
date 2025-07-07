/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include <algorithm>
#include "base/utils/Interpolate.h"
#include "pag/file.h"

namespace pag {

void PathData::moveTo(float x, float y) {
  verbs.push_back(PathDataVerb::MoveTo);
  points.push_back({x, y});
  lastMoveTo.x = x;
  lastMoveTo.y = y;
}

void PathData::lineTo(float x, float y) {
  if (verbs.empty()) {
    moveTo(0, 0);
  }
  verbs.push_back(PathDataVerb::LineTo);
  points.push_back({x, y});
}

void PathData::cubicTo(float controlX1, float controlY1, float controlX2, float controlY2, float x,
                       float y) {
  if (verbs.empty()) {
    moveTo(0, 0);
  }
  verbs.push_back(PathDataVerb::CurveTo);
  points.push_back({controlX1, controlY1});
  points.push_back({controlX2, controlY2});
  points.push_back({x, y});
}

void PathData::close() {
  if (verbs.empty()) {
    return;
  }
  auto lastVerb = verbs.back();
  if (lastVerb == PathDataVerb::Close) {
    return;
  }
  auto lastPoint = points.back();
  if (lastPoint != lastMoveTo) {
    lineTo(lastMoveTo.x, lastMoveTo.y);
  }
  verbs.push_back(PathDataVerb::Close);
}

bool PathData::isClosed() const {
  if (verbs.empty()) {
    return false;
  }
  return verbs.back() == PathDataVerb::Close;
}

void PathData::reverse() {
  if (verbs.empty()) {
    return;
  }
  std::reverse(points.begin(), points.end());
  std::vector<PathDataVerb> list;
  auto size = verbs.size();
  list.reserve(size);
  verbs.swap(list);
  verbs.push_back(PathDataVerb::MoveTo);
  bool hasClose = false;
  for (auto i = size - 1; i > 0; i--) {
    auto verb = list[i];
    switch (verb) {
      case PathDataVerb::Close:
        if (hasClose) {
          verbs.push_back(PathDataVerb::Close);
        }
        hasClose = true;
        break;
      case PathDataVerb::MoveTo:
        if (hasClose) {
          hasClose = false;
          verbs.push_back(PathDataVerb::Close);
        }
        verbs.push_back(PathDataVerb::MoveTo);
        break;
      default:
        verbs.push_back(verb);
        break;
    }
  }
  if (hasClose) {
    verbs.push_back(PathDataVerb::Close);
  }
}

void PathData::clear() {
  verbs.clear();
  points.clear();
}

struct CurveData {
  Point control1;
  Point control2;
  Point end;
};

inline CurveData GetCurveData(PathDataVerb verb, const std::vector<Point>& points, int& index) {
  CurveData data = {};
  if (verb == PathDataVerb::CurveTo) {
    data.control1 = points[index++];
    data.control2 = points[index++];
    data.end = points[index++];
  } else {
    data.control1 = points[index - 1];
    data.end = points[index++];
    data.control2 = data.end;
  }
  return data;
}

void PathData::interpolate(const PathData& path, PathData* result, float t) {
  if (verbs.empty() || path.verbs.empty()) {
    return;
  }
  auto& pointsA = points;
  auto& verbsA = verbs;
  auto& pointsB = path.points;
  auto& verbsB = path.verbs;
  int indexA = 0;
  int indexB = 0;
  auto size = verbsA.size();
  for (size_t i = 0; i < size; i++) {
    auto verbA = verbsA[i];
    auto verbB = verbsB[i];
    if (verbA == verbB) {
      result->verbs.push_back(verbA);
      if (verbA == PathDataVerb::Close) {
        continue;
      }
      auto count = verbA == PathDataVerb::CurveTo ? 3 : 1;
      while (count-- > 0) {
        result->points.push_back(Interpolate(pointsA[indexA++], pointsB[indexB++], t));
      }
    } else {
      auto dataA = GetCurveData(verbA, pointsA, indexA);
      auto dataB = GetCurveData(verbB, pointsB, indexB);
      result->verbs.push_back(PathDataVerb::CurveTo);
      result->points.push_back(Interpolate(dataA.control1, dataB.control1, t));
      result->points.push_back(Interpolate(dataA.control2, dataB.control2, t));
      result->points.push_back(Interpolate(dataA.end, dataB.end, t));
    }
  }
}

}  // namespace pag
