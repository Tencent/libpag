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
#include <cctype>
#include <cmath>
#include <sstream>

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

std::string PathData::toSVGString() const {
  std::ostringstream oss;
  oss.precision(6);

  size_t pointIndex = 0;
  for (auto verb : _verbs) {
    const float* pts = _points.data() + pointIndex;
    switch (verb) {
      case PathVerb::Move:
        oss << "M" << pts[0] << " " << pts[1];
        break;
      case PathVerb::Line:
        oss << "L" << pts[0] << " " << pts[1];
        break;
      case PathVerb::Quad:
        oss << "Q" << pts[0] << " " << pts[1] << " " << pts[2] << " " << pts[3];
        break;
      case PathVerb::Cubic:
        oss << "C" << pts[0] << " " << pts[1] << " " << pts[2] << " " << pts[3] << " " << pts[4]
            << " " << pts[5];
        break;
      case PathVerb::Close:
        oss << "Z";
        break;
    }
    pointIndex += PointsPerVerb(verb) * 2;
  }

  return oss.str();
}

// SVG path parser helper functions
static void SkipWhitespaceAndCommas(const char*& ptr, const char* end) {
  while (ptr < end && (std::isspace(*ptr) || *ptr == ',')) {
    ++ptr;
  }
}

static bool ParseNumber(const char*& ptr, const char* end, float& result) {
  SkipWhitespaceAndCommas(ptr, end);
  if (ptr >= end) {
    return false;
  }

  const char* start = ptr;
  if (*ptr == '-' || *ptr == '+') {
    ++ptr;
  }
  bool hasDigits = false;
  while (ptr < end && std::isdigit(*ptr)) {
    ++ptr;
    hasDigits = true;
  }
  if (ptr < end && *ptr == '.') {
    ++ptr;
    while (ptr < end && std::isdigit(*ptr)) {
      ++ptr;
      hasDigits = true;
    }
  }
  if (ptr < end && (*ptr == 'e' || *ptr == 'E')) {
    ++ptr;
    if (ptr < end && (*ptr == '-' || *ptr == '+')) {
      ++ptr;
    }
    while (ptr < end && std::isdigit(*ptr)) {
      ++ptr;
    }
  }

  if (!hasDigits) {
    ptr = start;
    return false;
  }

  std::string numStr(start, ptr);
  result = std::stof(numStr);
  return true;
}

static bool ParseFlag(const char*& ptr, const char* end, bool& result) {
  SkipWhitespaceAndCommas(ptr, end);
  if (ptr >= end) {
    return false;
  }
  if (*ptr == '0') {
    result = false;
    ++ptr;
    return true;
  }
  if (*ptr == '1') {
    result = true;
    ++ptr;
    return true;
  }
  return false;
}

// Convert arc to cubic Bezier curves
static void ArcToCubics(PathData& path, float x1, float y1, float rx, float ry, float angle,
                        bool largeArc, bool sweep, float x2, float y2) {
  if (rx == 0 || ry == 0) {
    path.lineTo(x2, y2);
    return;
  }

  rx = std::abs(rx);
  ry = std::abs(ry);

  float radians = angle * 3.14159265358979323846f / 180.0f;
  float cosAngle = std::cos(radians);
  float sinAngle = std::sin(radians);

  float dx2 = (x1 - x2) / 2.0f;
  float dy2 = (y1 - y2) / 2.0f;

  float x1p = cosAngle * dx2 + sinAngle * dy2;
  float y1p = -sinAngle * dx2 + cosAngle * dy2;

  float lambda = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
  if (lambda > 1.0f) {
    float sqrtLambda = std::sqrt(lambda);
    rx *= sqrtLambda;
    ry *= sqrtLambda;
  }

  float rx2 = rx * rx;
  float ry2 = ry * ry;
  float x1p2 = x1p * x1p;
  float y1p2 = y1p * y1p;

  float num = rx2 * ry2 - rx2 * y1p2 - ry2 * x1p2;
  float den = rx2 * y1p2 + ry2 * x1p2;
  float sq = (num < 0 || den == 0) ? 0.0f : std::sqrt(num / den);

  if (largeArc == sweep) {
    sq = -sq;
  }

  float cxp = sq * rx * y1p / ry;
  float cyp = -sq * ry * x1p / rx;

  float cx = cosAngle * cxp - sinAngle * cyp + (x1 + x2) / 2.0f;
  float cy = sinAngle * cxp + cosAngle * cyp + (y1 + y2) / 2.0f;

  auto vectorAngle = [](float ux, float uy, float vx, float vy) -> float {
    float n = std::sqrt(ux * ux + uy * uy) * std::sqrt(vx * vx + vy * vy);
    if (n == 0) {
      return 0;
    }
    float c = (ux * vx + uy * vy) / n;
    c = std::max(-1.0f, std::min(1.0f, c));
    float angle = std::acos(c);
    if (ux * vy - uy * vx < 0) {
      angle = -angle;
    }
    return angle;
  };

  float theta1 = vectorAngle(1.0f, 0.0f, (x1p - cxp) / rx, (y1p - cyp) / ry);
  float dtheta = vectorAngle((x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx,
                             (-y1p - cyp) / ry);

  if (!sweep && dtheta > 0) {
    dtheta -= 2.0f * 3.14159265358979323846f;
  } else if (sweep && dtheta < 0) {
    dtheta += 2.0f * 3.14159265358979323846f;
  }

  int segments = static_cast<int>(std::ceil(std::abs(dtheta) / (3.14159265358979323846f / 2.0f)));
  float segmentAngle = dtheta / segments;

  float t = std::tan(segmentAngle / 2.0f);
  float alpha = std::sin(segmentAngle) * (std::sqrt(4.0f + 3.0f * t * t) - 1.0f) / 3.0f;

  float currentAngle = theta1;
  float currentX = x1;
  float currentY = y1;

  for (int i = 0; i < segments; ++i) {
    float nextAngle = currentAngle + segmentAngle;

    float cosStart = std::cos(currentAngle);
    float sinStart = std::sin(currentAngle);
    float cosEnd = std::cos(nextAngle);
    float sinEnd = std::sin(nextAngle);

    float ex = cx + rx * (cosAngle * cosEnd - sinAngle * sinEnd);
    float ey = cy + rx * (sinAngle * cosEnd + cosAngle * sinEnd);

    float dx1 = -rx * (cosAngle * sinStart + sinAngle * cosStart);
    float dy1 = -rx * (sinAngle * sinStart - cosAngle * cosStart);
    float dx2 = -rx * (cosAngle * sinEnd + sinAngle * cosEnd);
    float dy2 = -rx * (sinAngle * sinEnd - cosAngle * cosEnd);

    float c1x = currentX + alpha * dx1;
    float c1y = currentY + alpha * dy1;
    float c2x = ex - alpha * dx2;
    float c2y = ey - alpha * dy2;

    path.cubicTo(c1x, c1y, c2x, c2y, ex, ey);

    currentAngle = nextAngle;
    currentX = ex;
    currentY = ey;
  }
}

PathData PathData::FromSVGString(const std::string& d) {
  PathData path;
  if (d.empty()) {
    return path;
  }

  const char* ptr = d.c_str();
  const char* end = ptr + d.length();

  float currentX = 0;
  float currentY = 0;
  float startX = 0;
  float startY = 0;
  float lastControlX = 0;
  float lastControlY = 0;
  char lastCommand = 0;

  while (ptr < end) {
    SkipWhitespaceAndCommas(ptr, end);
    if (ptr >= end) {
      break;
    }

    char command = *ptr;
    if (std::isalpha(command)) {
      ++ptr;
    } else {
      command = lastCommand;
    }

    bool isRelative = std::islower(command);
    char upperCommand = std::toupper(command);

    switch (upperCommand) {
      case 'M': {
        float x, y;
        if (!ParseNumber(ptr, end, x) || !ParseNumber(ptr, end, y)) {
          break;
        }
        if (isRelative) {
          x += currentX;
          y += currentY;
        }
        path.moveTo(x, y);
        currentX = startX = x;
        currentY = startY = y;
        lastCommand = isRelative ? 'l' : 'L';
        break;
      }
      case 'L': {
        float x, y;
        if (!ParseNumber(ptr, end, x) || !ParseNumber(ptr, end, y)) {
          break;
        }
        if (isRelative) {
          x += currentX;
          y += currentY;
        }
        path.lineTo(x, y);
        currentX = x;
        currentY = y;
        lastCommand = command;
        break;
      }
      case 'H': {
        float x;
        if (!ParseNumber(ptr, end, x)) {
          break;
        }
        if (isRelative) {
          x += currentX;
        }
        path.lineTo(x, currentY);
        currentX = x;
        lastCommand = command;
        break;
      }
      case 'V': {
        float y;
        if (!ParseNumber(ptr, end, y)) {
          break;
        }
        if (isRelative) {
          y += currentY;
        }
        path.lineTo(currentX, y);
        currentY = y;
        lastCommand = command;
        break;
      }
      case 'C': {
        float x1, y1, x2, y2, x, y;
        if (!ParseNumber(ptr, end, x1) || !ParseNumber(ptr, end, y1) ||
            !ParseNumber(ptr, end, x2) || !ParseNumber(ptr, end, y2) ||
            !ParseNumber(ptr, end, x) || !ParseNumber(ptr, end, y)) {
          break;
        }
        if (isRelative) {
          x1 += currentX;
          y1 += currentY;
          x2 += currentX;
          y2 += currentY;
          x += currentX;
          y += currentY;
        }
        path.cubicTo(x1, y1, x2, y2, x, y);
        lastControlX = x2;
        lastControlY = y2;
        currentX = x;
        currentY = y;
        lastCommand = command;
        break;
      }
      case 'S': {
        float x2, y2, x, y;
        if (!ParseNumber(ptr, end, x2) || !ParseNumber(ptr, end, y2) ||
            !ParseNumber(ptr, end, x) || !ParseNumber(ptr, end, y)) {
          break;
        }
        if (isRelative) {
          x2 += currentX;
          y2 += currentY;
          x += currentX;
          y += currentY;
        }
        float x1 = currentX;
        float y1 = currentY;
        char lastUpper = std::toupper(lastCommand);
        if (lastUpper == 'C' || lastUpper == 'S') {
          x1 = 2 * currentX - lastControlX;
          y1 = 2 * currentY - lastControlY;
        }
        path.cubicTo(x1, y1, x2, y2, x, y);
        lastControlX = x2;
        lastControlY = y2;
        currentX = x;
        currentY = y;
        lastCommand = command;
        break;
      }
      case 'Q': {
        float x1, y1, x, y;
        if (!ParseNumber(ptr, end, x1) || !ParseNumber(ptr, end, y1) ||
            !ParseNumber(ptr, end, x) || !ParseNumber(ptr, end, y)) {
          break;
        }
        if (isRelative) {
          x1 += currentX;
          y1 += currentY;
          x += currentX;
          y += currentY;
        }
        path.quadTo(x1, y1, x, y);
        lastControlX = x1;
        lastControlY = y1;
        currentX = x;
        currentY = y;
        lastCommand = command;
        break;
      }
      case 'T': {
        float x, y;
        if (!ParseNumber(ptr, end, x) || !ParseNumber(ptr, end, y)) {
          break;
        }
        if (isRelative) {
          x += currentX;
          y += currentY;
        }
        float x1 = currentX;
        float y1 = currentY;
        char lastUpper = std::toupper(lastCommand);
        if (lastUpper == 'Q' || lastUpper == 'T') {
          x1 = 2 * currentX - lastControlX;
          y1 = 2 * currentY - lastControlY;
        }
        path.quadTo(x1, y1, x, y);
        lastControlX = x1;
        lastControlY = y1;
        currentX = x;
        currentY = y;
        lastCommand = command;
        break;
      }
      case 'A': {
        float rx, ry, angle, x, y;
        bool largeArc, sweep;
        if (!ParseNumber(ptr, end, rx) || !ParseNumber(ptr, end, ry) ||
            !ParseNumber(ptr, end, angle) || !ParseFlag(ptr, end, largeArc) ||
            !ParseFlag(ptr, end, sweep) || !ParseNumber(ptr, end, x) ||
            !ParseNumber(ptr, end, y)) {
          break;
        }
        if (isRelative) {
          x += currentX;
          y += currentY;
        }
        ArcToCubics(path, currentX, currentY, rx, ry, angle, largeArc, sweep, x, y);
        currentX = x;
        currentY = y;
        lastCommand = command;
        break;
      }
      case 'Z': {
        path.close();
        currentX = startX;
        currentY = startY;
        lastCommand = command;
        break;
      }
      default:
        ++ptr;
        break;
    }
  }

  return path;
}

}  // namespace pagx
