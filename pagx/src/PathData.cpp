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

Rect PathData::getBounds() {
  if (!_boundsDirty) {
    return _cachedBounds;
  }

  _cachedBounds = {};
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
