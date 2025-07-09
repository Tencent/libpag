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

#pragma once

#include <cmath>
#include <vector>
#include "Interpolate.h"

namespace pag {
struct BezierKey3D {
  int32_t values[13];

  static BezierKey3D Make(const Point3D points[4], float precision);

  bool operator==(const BezierKey3D& other) const;
};

struct BezierHasher3D {
  size_t operator()(const BezierKey3D& key) const;
};

struct BezierSegment3D {
  Point3D position;
  /**
   * Total distance up to this point
   */
  float distance;
  /**
   * Total t value up to this point
   */
  unsigned tValue;
};

class BezierPath3D {
 public:
  /**
   * Creates a cubic bezier path.
   */
  static std::shared_ptr<BezierPath3D> Build(const Point3D& start, const Point3D& control1,
                                             const Point3D& control2, const Point3D& end,
                                             float precision);

  /**
   * Calculates a point on the curve, for a given value between 0 and 1.0 indicating the percent of
   * the curve length where 0 represents the start and 1.0 represents the end.
   */
  Point3D getPosition(float percent) const;

  /**
   * Calculates a t value of the curve, for a given value between 0 and 1.0 indicating the percent
   * of the curve length where 0 represents the start and 1.0 represents the end.
   */
  float getT(float percent) const;

  /**
   * Calculates a y point value on the curve, for a given x point value.
   */
  float getY(float x) const;

  /**
   * Calculates a x point value on the curve, for a given y point value.
   */
  float getX(float y) const;

  /**
   * Returns the length of the bezier path in pixels.
   */
  float getLength() const;

 private:
  float length = 0;
  std::vector<BezierSegment3D> segments;

  BezierPath3D() = default;
  void findSegmentAtDistance(float distance, int& startIndex, int& endIndex, float& fraction) const;
};
}  // namespace pag
