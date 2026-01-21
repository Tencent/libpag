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

#include <string>
#include <vector>
#include "pagx/model/types/Types.h"

namespace pagx {

/**
 * Path command types.
 */
enum class PathVerb : uint8_t {
  Move,   // 1 point: destination
  Line,   // 1 point: end point
  Quad,   // 2 points: control point, end point
  Cubic,  // 3 points: control point 1, control point 2, end point
  Close   // 0 points
};

/**
 * PathData stores path commands in a format optimized for fast iteration
 * and serialization. Unlike tgfx::Path, it exposes raw data arrays directly.
 */
class PathData {
 public:
  PathData() = default;

  /**
   * Creates a PathData from an SVG path data string (d attribute).
   */
  static PathData FromSVGString(const std::string& d);

  /**
   * Starts a new contour at the specified point.
   */
  void moveTo(float x, float y);

  /**
   * Adds a line from the current point to the specified point.
   */
  void lineTo(float x, float y);

  /**
   * Adds a quadratic Bezier curve from the current point.
   * @param cx control point x
   * @param cy control point y
   * @param x end point x
   * @param y end point y
   */
  void quadTo(float cx, float cy, float x, float y);

  /**
   * Adds a cubic Bezier curve from the current point.
   * @param c1x first control point x
   * @param c1y first control point y
   * @param c2x second control point x
   * @param c2y second control point y
   * @param x end point x
   * @param y end point y
   */
  void cubicTo(float c1x, float c1y, float c2x, float c2y, float x, float y);

  /**
   * Closes the current contour by connecting to the starting point.
   */
  void close();

  /**
   * Adds a rectangle to the path.
   */
  void addRect(const Rect& rect);

  /**
   * Adds an oval inscribed in the specified rectangle.
   */
  void addOval(const Rect& rect);

  /**
   * Adds a rounded rectangle to the path.
   */
  void addRoundRect(const Rect& rect, float radiusX, float radiusY);

  /**
   * Returns the array of path commands.
   */
  const std::vector<PathVerb>& verbs() const {
    return _verbs;
  }

  /**
   * Returns the array of point coordinates.
   * Points are stored as [x0, y0, x1, y1, ...].
   */
  const std::vector<float>& points() const {
    return _points;
  }

  /**
   * Returns the number of point coordinates.
   */
  size_t countPoints() const {
    return _points.size() / 2;
  }

  /**
   * Iterates over all path commands with a visitor function.
   * The visitor receives the verb and a pointer to the point data.
   */
  template <typename Visitor>
  void forEach(Visitor&& visitor) const {
    size_t pointIndex = 0;
    for (auto verb : _verbs) {
      const float* pts = _points.data() + pointIndex;
      visitor(verb, pts);
      pointIndex += PointsPerVerb(verb) * 2;
    }
  }

  /**
   * Converts the path to an SVG path data string.
   */
  std::string toSVGString() const;

  /**
   * Returns the bounding rectangle of the path.
   */
  Rect getBounds() const;

  /**
   * Returns true if the path contains no commands.
   */
  bool isEmpty() const {
    return _verbs.empty();
  }

  /**
   * Clears all path data.
   */
  void clear();

  /**
   * Transforms all points in the path by the given matrix.
   */
  void transform(const Matrix& matrix);

  /**
   * Returns the number of points used by the given verb.
   */
  static int PointsPerVerb(PathVerb verb);

 private:
  std::vector<PathVerb> _verbs = {};
  std::vector<float> _points = {};
  mutable Rect _cachedBounds = {};
  mutable bool _boundsDirty = true;
};

}  // namespace pagx
