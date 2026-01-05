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

#include "BezierPath.h"
#include <mutex>
#include <unordered_map>

namespace pag {

#define MaxBezierTValue 0x3FFFFFFF

inline bool TSpanBigEnough(int tSpan) {
  return (tSpan >> 10) != 0;
}

inline float MaxFloat(float a, float b) {
  return a > b ? a : b;
}

static float Distance(const Point& a, const Point& b) {
  auto dx = a.x - b.x;
  auto dy = a.y - b.y;
  return sqrtf(dx * dx + dy * dy);
}

static bool PointOnLine(const Point& point1, const Point& point2, const Point& point3,
                        const float& precision) {
  auto distance = point1.x * point2.y + point1.y * point3.x + point2.x * point3.y -
                  point3.x * point2.y - point3.y * point1.x - point2.x * point1.y;
  return fabs(distance) < precision;
}

/**
 * Given the cubic bezier points, split it at the specified t value, where 0 < t < 1, and return the
 * two new cubics in result: result[0..3] and result[3..6]
 */
static void SplitCubicCurveAt(const Point points[4], Point result[7], float t) {
  auto p1 = Interpolate(points[0], points[1], t);
  auto bc = Interpolate(points[1], points[2], t);
  auto p5 = Interpolate(points[2], points[3], t);
  auto p2 = Interpolate(p1, bc, t);
  auto p4 = Interpolate(bc, p5, t);

  result[0] = points[0];
  result[1] = p1;
  result[2] = p2;
  result[3] = Interpolate(p2, p4, t);
  result[4] = p4;
  result[5] = p5;
  result[6] = points[3];
}

static bool DistanceExceedsLimit(const Point& pt1, const Point& pt2, const float& precision) {
  return MaxFloat(fabsf(pt2.x - pt1.x), fabsf(pt2.y - pt1.y)) > precision;
}

static bool CubicTooCurvy(const Point pts[4], const float& precision) {
  static float oneOfThird = 1.0f / 3;
  static float twoOfThird = 2.0f / 3;
  auto pt1 = Interpolate(pts[0], pts[3], oneOfThird);
  auto pt2 = Interpolate(pts[0], pts[3], twoOfThird);
  return DistanceExceedsLimit(pts[1], pt1, precision) ||
         DistanceExceedsLimit(pts[2], pt2, precision);
}

static float BuildCubicSegments(const Point points[4], float distance, unsigned minT, unsigned maxT,
                                std::vector<BezierSegment>& segments, const float& precision) {
  if (TSpanBigEnough(maxT - minT) && CubicTooCurvy(points, precision)) {
    auto halfT = (minT + maxT) >> 1;
    Point result[7];
    SplitCubicCurveAt(points, result, 0.5f);
    distance = BuildCubicSegments(result, distance, minT, halfT, segments, precision);
    distance = BuildCubicSegments(&result[3], distance, halfT, maxT, segments, precision);
  } else {
    distance += Distance(points[0], points[3]);
    BezierSegment segment = {points[3], distance, maxT};
    segments.push_back(segment);
  }
  return distance;
}

BezierKey BezierKey::Make(const Point points[4], float precision) {
  BezierKey bezierKey = {};
  auto values = reinterpret_cast<const float*>(points);
  for (int i = 0; i < 8; i++) {
    bezierKey.values[i] = static_cast<int32_t>(roundf(values[i] / precision));
  }
  bezierKey.values[8] = static_cast<int32_t>(roundf(1 / precision));
  return bezierKey;
}

bool BezierKey::operator==(const pag::BezierKey& other) const {
  for (int i = 0; i < 9; i++) {
    if (values[i] != other.values[i]) {
      return false;
    }
  }
  return true;
}

size_t BezierHasher::operator()(const pag::BezierKey& key) const {
  size_t hash = 0;
  auto& values = key.values;
  for (int i = 0; i < 9; i++) {
    hash ^= values[i] + 0x9e3779b9 + (hash << 6) + (hash >> 2);
  }
  return hash;
}

static std::unordered_map<BezierKey, std::weak_ptr<BezierPath>, BezierHasher> BezierCacheMap =
    std::unordered_map<BezierKey, std::weak_ptr<BezierPath>, BezierHasher>{};
static std::mutex locker = {};

std::shared_ptr<BezierPath> BezierPath::Build(const pag::Point& start, const pag::Point& control1,
                                              const pag::Point& control2, const pag::Point& end,
                                              float precision) {
  Point points[] = {start, control1, control2, end};
  auto bezierKey = BezierKey::Make(points, precision);
  {
    std::lock_guard<std::mutex> autoLock(locker);
    auto result = BezierCacheMap.find(bezierKey);
    if (result != BezierCacheMap.end()) {
      auto& weak = result->second;
      auto data = weak.lock();
      if (data) {
        return data;
      }
      BezierCacheMap.erase(result);
    }
  }

  auto bezierPath = std::shared_ptr<BezierPath>(new BezierPath());
  BezierSegment segment = {points[0], 0, 0};
  bezierPath->segments.push_back(segment);
  if (PointOnLine(points[0], points[3], points[1], precision) &&
      PointOnLine(points[0], points[3], points[2], precision)) {
    bezierPath->length = Distance(points[0], points[3]);
    segment = {points[3], bezierPath->length, MaxBezierTValue};
    bezierPath->segments.push_back(segment);
  } else {
    bezierPath->length =
        BuildCubicSegments(points, 0, 0, MaxBezierTValue, bezierPath->segments, precision);
  }
  {
    std::lock_guard<std::mutex> autoLock(locker);
    std::weak_ptr<BezierPath> weak = bezierPath;
    BezierCacheMap.insert(std::make_pair(bezierKey, std::move(weak)));
  }
  return bezierPath;
}

Point BezierPath::getPosition(float percent) const {
  if (percent <= 0) {
    return segments[0].position;
  }
  if (percent >= 1) {
    return segments.back().position;
  }
  int startIndex, endIndex;
  float fraction;
  findSegmentAtDistance(length * percent, startIndex, endIndex, fraction);
  return Interpolate(segments[startIndex].position, segments[endIndex].position, fraction);
}

float BezierPath::getT(float percent) const {
  if (percent <= 0) {
    return 0;
  }
  if (percent >= 1) {
    return 1;
  }
  int startIndex, endIndex;
  float fraction;
  findSegmentAtDistance(length * percent, startIndex, endIndex, fraction);
  return Interpolate<float>(segments[startIndex].tValue, segments[endIndex].tValue, fraction) /
         static_cast<float>(MaxBezierTValue);
}

float BezierPath::getY(float x) const {
  int startIndex = 0;
  auto endIndex = static_cast<int>(segments.size() - 1);
  while (endIndex - startIndex > 1) {
    auto middleIndex = (startIndex + endIndex) >> 1;
    if (x < segments[middleIndex].position.x) {
      endIndex = middleIndex;
    } else {
      startIndex = middleIndex;
    }
  }
  auto& start = segments[startIndex].position;
  auto& end = segments[endIndex].position;
  auto xRange = end.x - start.x;
  if (xRange == 0) {
    return start.y;
  }
  auto fraction = (x - start.x) / xRange;
  return Interpolate(start.y, end.y, fraction);
}

float BezierPath::getX(float y) const {
  int startIndex = 0;
  auto endIndex = static_cast<int>(segments.size() - 1);
  while (endIndex - startIndex > 1) {
    auto middleIndex = (startIndex + endIndex) >> 1;
    if (y < segments[middleIndex].position.y) {
      endIndex = middleIndex;
    } else {
      startIndex = middleIndex;
    }
  }
  auto& start = segments[startIndex].position;
  auto& end = segments[endIndex].position;
  auto yRange = end.y - start.y;
  if (yRange == 0) {
    return start.x;
  }
  auto fraction = (y - start.y) / yRange;
  return Interpolate(start.x, end.x, fraction);
}

float BezierPath::getLength() const {
  return length;
}

void BezierPath::findSegmentAtDistance(float distance, int& startIndex, int& endIndex,
                                       float& fraction) const {
  startIndex = 0;
  endIndex = static_cast<int>(segments.size() - 1);
  while (endIndex - startIndex > 1) {
    auto middleIndex = (startIndex + endIndex) >> 1;
    if (distance < segments[middleIndex].distance) {
      endIndex = middleIndex;
    } else {
      startIndex = middleIndex;
    }
  }
  auto& start = segments[startIndex];
  auto& end = segments[endIndex];
  auto distanceRange = end.distance - start.distance;
  if (distanceRange == 0) {
    fraction = 0;
  } else {
    fraction = (distance - start.distance) / distanceRange;
  }
}

}  // namespace pag
