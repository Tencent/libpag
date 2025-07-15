/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "BezierPath3D.h"
#include <mutex>
#include <unordered_map>

namespace pag {

#define MaxBezierTValue 0x3FFFFFFF

inline bool TSpanBigEnough(int tSpan) {
  return (tSpan >> 10) != 0;
}

inline float MaxFloat(float a, float b, float c) {
  auto t = a > b ? a : b;
  return t > c ? t : c;
}

static float Distance(const Point3D& a, const Point3D& b) {
  auto dx = a.x - b.x;
  auto dy = a.y - b.y;
  auto dz = a.z - b.z;
  return sqrtf(dx * dx + dy * dy + dz * dz);
}

inline bool PointOnLine(const float x1, const float y1, const float x2, const float y2,
                        const float x3, const float y3, const float& precision) {
  auto distance = x1 * y2 + x3 * y1 + x2 * y3 - x3 * y2 - x1 * y3 - x2 * y1;
  return fabs(distance) < precision;
}

inline bool Point3DOnLine(const Point3D& point1, const Point3D& point2, const Point3D& point3,
                          const float& precision) {
  return PointOnLine(point1.x, point1.y, point2.x, point2.y, point3.x, point3.y, precision) &&
         PointOnLine(point1.x, point1.z, point2.x, point2.z, point3.x, point3.z, precision) &&
         PointOnLine(point1.y, point1.z, point2.y, point2.z, point3.y, point3.z, precision);
}

/**
 * Given the cubic bezier points, split it at the specified t value, where 0 < t < 1, and return the
 * two new cubics in result: result[0..3] and result[3..6]
 */
static void SplitCubicCurveAt(const Point3D points[4], Point3D result[7], float t) {
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

static bool DistanceExceedsLimit(const Point3D& pt1, const Point3D& pt2, const float& precision) {
  return MaxFloat(fabsf(pt2.x - pt1.x), fabsf(pt2.y - pt1.y), fabsf(pt2.z - pt1.z)) > precision;
}

static bool CubicTooCurvy(const Point3D pts[4], const float& precision) {
  static float oneOfThird = 1.0f / 3;
  static float twoOfThird = 2.0f / 3;
  auto pt1 = Interpolate(pts[0], pts[3], oneOfThird);
  auto pt2 = Interpolate(pts[0], pts[3], twoOfThird);
  return DistanceExceedsLimit(pts[1], pt1, precision) ||
         DistanceExceedsLimit(pts[2], pt2, precision);
}

static float BuildCubicSegments(const Point3D points[4], float distance, unsigned minT,
                                unsigned maxT, std::vector<BezierSegment3D>& segments,
                                const float& precision) {
  if (TSpanBigEnough(maxT - minT) && CubicTooCurvy(points, precision)) {
    auto halfT = (minT + maxT) >> 1;
    Point3D result[7];
    SplitCubicCurveAt(points, result, 0.5f);
    distance = BuildCubicSegments(result, distance, minT, halfT, segments, precision);
    distance = BuildCubicSegments(&result[3], distance, halfT, maxT, segments, precision);
  } else {
    distance += Distance(points[0], points[3]);
    BezierSegment3D segment = {points[3], distance, maxT};
    segments.push_back(segment);
  }
  return distance;
}

BezierKey3D BezierKey3D::Make(const Point3D points[4], float precision) {
  BezierKey3D bezierKey = {};
  auto values = reinterpret_cast<const float*>(points);
  for (int i = 0; i < 12; i++) {
    bezierKey.values[i] = static_cast<int32_t>(roundf(values[i] / precision));
  }
  bezierKey.values[12] = static_cast<int32_t>(roundf(1 / precision));
  return bezierKey;
}

bool BezierKey3D::operator==(const pag::BezierKey3D& other) const {
  for (int i = 0; i < 13; i++) {
    if (values[i] != other.values[i]) {
      return false;
    }
  }
  return true;
}

size_t BezierHasher3D::operator()(const pag::BezierKey3D& key) const {
  size_t hash = 0;
  auto& values = key.values;
  for (int i = 0; i < 13; i++) {
    hash ^= values[i] + 0x9e3779b9 + (hash << 6) + (hash >> 2);
  }
  return hash;
}

static std::unordered_map<BezierKey3D, std::weak_ptr<BezierPath3D>, BezierHasher3D> BezierCacheMap =
    std::unordered_map<BezierKey3D, std::weak_ptr<BezierPath3D>, BezierHasher3D>{};
static std::mutex locker = {};

std::shared_ptr<BezierPath3D> BezierPath3D::Build(const pag::Point3D& start,
                                                  const pag::Point3D& control1,
                                                  const pag::Point3D& control2,
                                                  const pag::Point3D& end, float precision) {
  Point3D points[] = {start, control1, control2, end};
  auto bezierKey = BezierKey3D::Make(points, precision);
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

  auto bezierPath = std::shared_ptr<BezierPath3D>(new BezierPath3D());
  BezierSegment3D segment = {points[0], 0, 0};
  bezierPath->segments.push_back(segment);
  if (Point3DOnLine(points[0], points[3], points[1], precision) &&
      Point3DOnLine(points[0], points[3], points[2], precision)) {
    bezierPath->length = Distance(points[0], points[3]);
    segment = {points[3], bezierPath->length, MaxBezierTValue};
    bezierPath->segments.push_back(segment);
  } else {
    bezierPath->length =
        BuildCubicSegments(points, 0, 0, MaxBezierTValue, bezierPath->segments, precision);
  }
  {
    std::lock_guard<std::mutex> autoLock(locker);
    std::weak_ptr<BezierPath3D> weak = bezierPath;
    BezierCacheMap.insert(std::make_pair(bezierKey, std::move(weak)));
  }
  return bezierPath;
}

Point3D BezierPath3D::getPosition(float percent) const {
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

float BezierPath3D::getT(float percent) const {
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

float BezierPath3D::getY(float x) const {
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

float BezierPath3D::getX(float y) const {
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

float BezierPath3D::getLength() const {
  return length;
}

void BezierPath3D::findSegmentAtDistance(float distance, int& startIndex, int& endIndex,
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
