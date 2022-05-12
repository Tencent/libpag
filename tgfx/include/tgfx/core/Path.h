/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "tgfx/core/Matrix.h"
#include "tgfx/core/PathTypes.h"

namespace tgfx {
class PathRef;

/**
 * Path contain geometry. Path may be empty, or contain one or more verbs that outline a figure.
 * Path always starts with a move verb to a Cartesian coordinate, and may be followed by additional
 * verbs that add lines or curves. dding a close verb makes the geometry into a continuous loop, a
 * closed contour. Path may contain any number of contours, each beginning with a move verb.
 */
class Path {
 public:
  /**
   * Creates an empty path.
   */
  Path();

  /**
   * Compares a and b; returns true if they are equivalent.
   */
  friend bool operator==(const Path& a, const Path& b);

  /**
   * Compares a and b; returns true if they are not equivalent.
   */
  friend bool operator!=(const Path& a, const Path& b);

  /**
   * Returns PathFillType, the rule used to fill Path. PathFillType of a new Path is Path
   * FillType::Winding.
   */
  PathFillType getFillType() const;

  /**
   * Sets PathFillType, the rule used to fill Path.
   */
  void setFillType(PathFillType fillType);

  /**
   * Returns if PathFillType is InverseWinding or InverseEvenOdd.
   */
  bool isInverseFillType() const;

  /**
   * Replaces PathFillType with its inverse.
   */
  void toggleInverseFillType();

  /**
   * Returns true if Path is equivalent to Rect when filled, Otherwise returns false, and leaves
   * rect unchanged. The rect may be smaller than the Path bounds. Path bounds may include
   * PathVerb::Move points that do not alter the area drawn by the returned rect.
   */
  bool asRect(Rect* rect) const;

  bool asRRect(RRect* rRect) const;

  /**
   * Returns the bounds of the path's points. If the path contains 0 or 1 points, the bounds is set
   * to (0,0,0,0), and isEmpty() will return true. Note: this bounds may be larger than the actual
   * shape, since curves do not extend as far as their control points.
   */
  Rect getBounds() const;

  /**
   * Returns true if Path is empty.
   */
  bool isEmpty() const;

  /**
   * Returns true if the point (x, y) is contained by Path, taking into account PathFillType.
   */
  bool contains(float x, float y) const;

  /**
   * Returns true if rect is contained by Path. This method is conservative. it may return false
   * when rect is actually contained by Path. For now, only returns true if Path has one contour.
   */
  bool contains(const Rect& rect) const;

  /**
   * Adds beginning of contour at Point (x, y).
   */
  void moveTo(float x, float y);

  /**
   * Adds beginning of contour at point.
   */
  void moveTo(const Point& point);

  /**
   * Adds a line from last point to (x, y).
   */
  void lineTo(float x, float y);

  /**
   * Adds a line from last point to new point.
   */
  void lineTo(const Point& point);

  /**
   * Adds a quad curve from last point towards (controlX, controlY), ending at (x, y).
   */
  void quadTo(float controlX, float controlY, float x, float y);

  /**
   * Adds a quad curve from last point towards control, ending at point.
   */
  void quadTo(const Point& control, const Point& point);

  /**
   * Adds a cubic curve from last point towards (controlX1, controlY1), then towards
   * (controlX2, controlY2), ending at (x, y).
   */
  void cubicTo(float controlX1, float controlY1, float controlX2, float controlY2, float x,
               float y);

  /**
   * Adds a cubic curve from last point towards control1, then towards control2, ending at (x, y).
   */
  void cubicTo(const Point& control1, const Point& control2, const Point& point);

  /**
   * Closes the current contour of Path. A closed contour connects the first and last Point with
   * line, forming a continuous loop.
   */
  void close();

  /**
   * Adds a rect to Path. If reversed is false, Rect begins at startIndex point and continues
   * clockwise if reversed is false, counterclockwise if reversed is true. The indices of all points
   * are as follows:
   *  0         1
   *   *-------*
   *   |       |
   *   *-------*
   *  3         2
   */
  void addRect(const Rect& rect, bool reversed = false, unsigned startIndex = 0);

  /**
   * Adds a rect to Path. If reversed is false, Rect begins at startIndex point and continues
   * clockwise if reversed is false, counterclockwise if reversed is true. The indices of all points
   * are as follows:
   *  0         1
   *   *-------*
   *   |       |
   *   *-------*
   *  3         2
   */
  void addRect(float left, float top, float right, float bottom, bool reversed = false,
               unsigned startIndex = 0);

  /**
   * Adds a oval to path. Oval is upright ellipse bounded by Rect oval with radius equal to half
   * oval width and half oval height. Oval begins at startIndex point and continues clockwise if
   * reversed is false, counterclockwise if reversed is true. The indices of all points are as
   * follows:
   *         0
   *       --*--
   *     |       |
   *   3 *       * 1
   *     |       |
   *       --*--
   *         2
   */
  void addOval(const Rect& oval, bool reversed = false, unsigned startIndex = 0);

  /**
   * Adds an arc to the path which is centered at (x, y) position with radius r starting at
   * startAngle (in radians) and ending at endAngle. The arc goes in the direction by reversed,
   * which is clockwise if reversed is false, counterclockwise if reversed is true.
   */
  void addArc(float centerX, float centerY, float radius, float startAngle, float endAngle,
              bool reversed = false);

  /**
   * Adds a round rect to path. creating a new closed contour, each corner is 90 degrees of an
   * ellipse with radius (radiusX, radiusY). The round rect begins at startIndex point and continues
   * clockwise if reversed is false, counterclockwise if reversed is true. The indices of all points
   * are as follows:
   *      0      1
   *      *------*
   *   7 *        * 2
   *     |        |
   *   6 *        * 3
   *      *------*
   *      5      4
   */
  void addRoundRect(const Rect& rect, float radiusX, float radiusY, bool reversed = false,
                    unsigned startIndex = 0);

  /**
   * Adds a src to this Path.
   */
  void addPath(const Path& src, PathOp op = PathOp::Append);

  /**
   * Sets Path to its initial state. Internal storage associated with Path is released.
   */
  void reset();

  /**
   * Applies matrix to this Path, this transforms verb array, Point array, and weight.
   */
  void transform(const Matrix& matrix);

  /**
   * Reveres this path from back to front.
   */
  void reverse();

  /**
   * Iterates through verb array and associated Point array.
   */
  void decompose(const PathIterator& iterator, void* info = nullptr) const;

  int hash() const;

 private:
  std::shared_ptr<PathRef> pathRef = nullptr;

  PathRef* writableRef();

  friend class PathRef;

  friend struct PathHash;
};

struct PathHash {
  size_t operator()(const Path& path) const;
};
}  // namespace tgfx