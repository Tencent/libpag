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

#pragma once

#include <cmath>
#include <vector>
#include "Interpolate.h"

namespace pag {
struct BezierKey {
  int32_t values[9];

  static BezierKey Make(const Point points[4], float precision);

  bool operator==(const BezierKey& other) const;
};

struct BezierHasher {
  size_t operator()(const BezierKey& key) const;
};

struct BezierSegment {
  Point position;
  /**
   * Total distance up to this point
   */
  float distance;
  /**
   * Total t value up to this point
   */
  unsigned tValue;
};

class BezierPath {
 public:
  /**
   * Creates a cubic bezier path.
   */
  static std::shared_ptr<BezierPath> Build(const Point& start, const Point& control1,
                                           const Point& control2, const Point& end,
                                           float precision);

  /**
   * Calculates a point on the curve, for a given value between 0 and 1.0 indicating the percent of
   * the curve length where 0 represents the start and 1.0 represents the end.
   */
  Point getPosition(float percent) const;

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
  std::vector<BezierSegment> segments;

  BezierPath() = default;
  void findSegmentAtDistance(float distance, int& startIndex, int& endIndex, float& fraction) const;
};
}  // namespace pag
