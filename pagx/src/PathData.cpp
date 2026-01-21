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

namespace pagx {

// Constant for approximating a quarter circle with cubic Bezier curves
static constexpr float CUBIC_ARC_FACTOR = 0.5522847498307936f;

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
  }
  return 0;
}

void PathData::moveTo(float x, float y) {
  _verbs.push_back(PathVerb::Move);
  _points.push_back(x);
  _points.push_back(y);
  _boundsDirty = true;
}

void PathData::lineTo(float x, float y) {
  _verbs.push_back(PathVerb::Line);
  _points.push_back(x);
  _points.push_back(y);
  _boundsDirty = true;
}

void PathData::quadTo(float cx, float cy, float x, float y) {
  _verbs.push_back(PathVerb::Quad);
  _points.push_back(cx);
  _points.push_back(cy);
  _points.push_back(x);
  _points.push_back(y);
  _boundsDirty = true;
}

void PathData::cubicTo(float c1x, float c1y, float c2x, float c2y, float x, float y) {
  _verbs.push_back(PathVerb::Cubic);
  _points.push_back(c1x);
  _points.push_back(c1y);
  _points.push_back(c2x);
  _points.push_back(c2y);
  _points.push_back(x);
  _points.push_back(y);
  _boundsDirty = true;
}

void PathData::close() {
  _verbs.push_back(PathVerb::Close);
}

void PathData::addRect(const Rect& rect) {
  moveTo(rect.left(), rect.top());
  lineTo(rect.right(), rect.top());
  lineTo(rect.right(), rect.bottom());
  lineTo(rect.left(), rect.bottom());
  close();
}

void PathData::addOval(const Rect& rect) {
  float cx = rect.left() + rect.width * 0.5f;
  float cy = rect.top() + rect.height * 0.5f;
  float rx = rect.width * 0.5f;
  float ry = rect.height * 0.5f;

  float dx = rx * CUBIC_ARC_FACTOR;
  float dy = ry * CUBIC_ARC_FACTOR;

  moveTo(cx + rx, cy);
  cubicTo(cx + rx, cy + dy, cx + dx, cy + ry, cx, cy + ry);
  cubicTo(cx - dx, cy + ry, cx - rx, cy + dy, cx - rx, cy);
  cubicTo(cx - rx, cy - dy, cx - dx, cy - ry, cx, cy - ry);
  cubicTo(cx + dx, cy - ry, cx + rx, cy - dy, cx + rx, cy);
  close();
}

void PathData::addRoundRect(const Rect& rect, float radiusX, float radiusY) {
  if (radiusX <= 0 || radiusY <= 0) {
    addRect(rect);
    return;
  }

  float maxRadiusX = rect.width * 0.5f;
  float maxRadiusY = rect.height * 0.5f;
  radiusX = std::min(radiusX, maxRadiusX);
  radiusY = std::min(radiusY, maxRadiusY);

  float dx = radiusX * CUBIC_ARC_FACTOR;
  float dy = radiusY * CUBIC_ARC_FACTOR;

  moveTo(rect.left() + radiusX, rect.top());
  lineTo(rect.right() - radiusX, rect.top());
  cubicTo(rect.right() - radiusX + dx, rect.top(), rect.right(), rect.top() + radiusY - dy,
          rect.right(), rect.top() + radiusY);
  lineTo(rect.right(), rect.bottom() - radiusY);
  cubicTo(rect.right(), rect.bottom() - radiusY + dy, rect.right() - radiusX + dx, rect.bottom(),
          rect.right() - radiusX, rect.bottom());
  lineTo(rect.left() + radiusX, rect.bottom());
  cubicTo(rect.left() + radiusX - dx, rect.bottom(), rect.left(), rect.bottom() - radiusY + dy,
          rect.left(), rect.bottom() - radiusY);
  lineTo(rect.left(), rect.top() + radiusY);
  cubicTo(rect.left(), rect.top() + radiusY - dy, rect.left() + radiusX - dx, rect.top(),
          rect.left() + radiusX, rect.top());
  close();
}

void PathData::clear() {
  _verbs.clear();
  _points.clear();
  _cachedBounds.setEmpty();
  _boundsDirty = true;
}

void PathData::transform(const Matrix& matrix) {
  if (matrix.isIdentity()) {
    return;
  }
  for (size_t i = 0; i < _points.size(); i += 2) {
    Point p = {_points[i], _points[i + 1]};
    Point transformed = matrix.mapPoint(p);
    _points[i] = transformed.x;
    _points[i + 1] = transformed.y;
  }
  _boundsDirty = true;
}

Rect PathData::getBounds() {
  if (!_boundsDirty) {
    return _cachedBounds;
  }

  _cachedBounds.setEmpty();
  if (_points.empty()) {
    _boundsDirty = false;
    return _cachedBounds;
  }

  float minX = _points[0];
  float minY = _points[1];
  float maxX = _points[0];
  float maxY = _points[1];

  for (size_t i = 2; i < _points.size(); i += 2) {
    float x = _points[i];
    float y = _points[i + 1];
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
