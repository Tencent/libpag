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

#include "tgfx/core/Path.h"
#include "core/PathRef.h"
#include "core/utils/MathExtra.h"

namespace tgfx {
using namespace pk;

static SkPathDirection ToSkDirection(bool reversed) {
  return reversed ? SkPathDirection::kCCW : SkPathDirection::kCW;
}

#define DistanceToControlPoint(angleStep) (4 * tanf((angleStep)*0.25f) / 3)

class PointIterator {
 public:
  PointIterator(const std::vector<Point>& points, bool reversed, unsigned startIndex)
      : points(points),
        index(startIndex % points.size()),
        advance(reversed ? points.size() - 1 : 1) {
  }

  const SkPoint& current() const {
    return *reinterpret_cast<const SkPoint*>(&points[index]);
  }

  const SkPoint& next() {
    index = (index + advance) % points.size();
    return this->current();
  }

 protected:
  std::vector<Point> points = {};

 private:
  size_t index = 0;
  size_t advance = 0;
};

static SkRect ToSkRect(const Rect& rect) {
  return {rect.left, rect.top, rect.right, rect.bottom};
}

Path::Path() : pathRef(std::make_shared<PathRef>()) {
}

bool operator==(const Path& a, const Path& b) {
  return a.pathRef->path == b.pathRef->path;
}

bool operator!=(const Path& a, const Path& b) {
  return a.pathRef->path != b.pathRef->path;
}

PathFillType Path::getFillType() const {
  auto fillType = pathRef->path.getFillType();
  switch (fillType) {
    case SkPathFillType::kEvenOdd:
      return PathFillType::EvenOdd;
    case SkPathFillType::kInverseEvenOdd:
      return PathFillType::InverseEvenOdd;
    case SkPathFillType::kInverseWinding:
      return PathFillType::InverseWinding;
    default:
      return PathFillType::Winding;
  }
}

void Path::setFillType(PathFillType fillType) {
  SkPathFillType type;
  switch (fillType) {
    case PathFillType::EvenOdd:
      type = SkPathFillType::kEvenOdd;
      break;
    case PathFillType::InverseEvenOdd:
      type = SkPathFillType::kInverseEvenOdd;
      break;
    case PathFillType::InverseWinding:
      type = SkPathFillType::kInverseWinding;
      break;
    default:
      type = SkPathFillType::kWinding;
      break;
  }
  writableRef()->path.setFillType(type);
}

bool Path::isInverseFillType() const {
  auto fillType = pathRef->path.getFillType();
  return fillType == SkPathFillType::kInverseWinding || fillType == SkPathFillType::kInverseEvenOdd;
}

void Path::toggleInverseFillType() {
  auto& path = writableRef()->path;
  switch (path.getFillType()) {
    case SkPathFillType::kWinding:
      path.setFillType(SkPathFillType::kInverseWinding);
      break;
    case SkPathFillType::kEvenOdd:
      path.setFillType(SkPathFillType::kInverseEvenOdd);
      break;
    case SkPathFillType::kInverseWinding:
      path.setFillType(SkPathFillType::kWinding);
      break;
    case SkPathFillType::kInverseEvenOdd:
      path.setFillType(SkPathFillType::kEvenOdd);
      break;
  }
}

bool Path::asRect(Rect* rect) const {
  SkRect skRect = {};
  if (!pathRef->path.isRect(&skRect)) {
    return false;
  }
  if (rect) {
    rect->setLTRB(skRect.fLeft, skRect.fTop, skRect.fRight, skRect.fBottom);
  }
  return true;
}

bool Path::asRRect(RRect* rRect) const {
  SkRRect skRRect = {};
  SkRect skRect = {};
  if (pathRef->path.isOval(&skRect)) {
    skRRect.setOval(skRect);
  } else if (pathRef->path.isRRect(&skRRect)) {
    if (skRRect.isComplex() || skRRect.isNinePatch()) {
      return false;
    }
  } else {
    return false;
  }
  if (rRect) {
    const auto& rect = skRRect.rect();
    rRect->rect.setLTRB(rect.fLeft, rect.fTop, rect.fRight, rect.fBottom);
    auto radii = skRRect.getSimpleRadii();
    rRect->radii.set(radii.fX, radii.fY);
  }
  return true;
}

Rect Path::getBounds() const {
  // Internally, SkPath lazily computes bounds. Use this function instead of path.getBounds()
  // for thread safety.
  const auto& path = pathRef->path;
  auto count = path.countPoints();
  auto points = new SkPoint[count];
  path.getPoints(points, count);
  path.getBounds();
  auto rect = SkRect::MakeEmpty();
  rect.setBounds(points, count);
  delete[] points;
  return {rect.fLeft, rect.fTop, rect.fRight, rect.fBottom};
}

bool Path::isEmpty() const {
  return pathRef->path.isEmpty();
}

bool Path::contains(float x, float y) const {
  return pathRef->path.contains(x, y);
}

bool Path::contains(const Rect& rect) const {
  return pathRef->path.conservativelyContainsRect(ToSkRect(rect));
}

void Path::moveTo(float x, float y) {
  writableRef()->path.moveTo(x, y);
}

void Path::moveTo(const Point& point) {
  moveTo(point.x, point.y);
}

void Path::lineTo(float x, float y) {
  writableRef()->path.lineTo(x, y);
}

void Path::lineTo(const Point& point) {
  lineTo(point.x, point.y);
}

void Path::quadTo(float controlX, float controlY, float x, float y) {
  writableRef()->path.quadTo(controlX, controlY, x, y);
}

void Path::quadTo(const Point& control, const Point& point) {
  quadTo(control.x, control.y, point.x, point.y);
}

void Path::cubicTo(float controlX1, float controlY1, float controlX2, float controlY2, float x,
                   float y) {
  writableRef()->path.cubicTo(controlX1, controlY1, controlX2, controlY2, x, y);
}

void Path::cubicTo(const Point& control1, const Point& control2, const Point& point) {
  cubicTo(control1.x, control1.y, control2.x, control2.y, point.x, point.y);
}

void Path::close() {
  writableRef()->path.close();
}

void Path::addRect(const Rect& rect, bool reversed, unsigned startIndex) {
  addRect(rect.left, rect.top, rect.right, rect.bottom, reversed, startIndex);
}

void Path::addRect(float left, float top, float right, float bottom, bool reversed,
                   unsigned startIndex) {
  std::vector<Point> points = {};
  points.push_back({left, top});
  points.push_back({right, top});
  points.push_back({right, bottom});
  points.push_back({left, bottom});
  PointIterator iter(points, reversed, startIndex);
  auto path = &(writableRef()->path);
  path->moveTo(iter.current());
  path->lineTo(iter.next());
  path->lineTo(iter.next());
  path->lineTo(iter.next());
  path->close();
}

static std::vector<Point> GetArcPoints(float centerX, float centerY, float radiusX, float radiusY,
                                       float startAngle, float endAngle, int* numBeziers) {
  std::vector<Point> points = {};
  float start = startAngle;
  float end = std::min(endAngle, start + M_PI_2_F);
  float currentX, currentY;
  *numBeziers = 0;
  for (int i = 0; i < 4; i++) {
    auto angleStep = end - start;
    auto distance = DistanceToControlPoint(angleStep);
    currentX = centerX + cosf(start) * radiusX;
    currentY = centerY + sinf(start) * radiusY;
    points.push_back({currentX, currentY});
    auto u = cosf(start);
    auto v = sinf(start);
    auto x1 = currentX - v * distance * radiusX;
    auto y1 = currentY + u * distance * radiusY;
    points.push_back({x1, y1});
    u = cosf(end);
    v = sinf(end);
    currentX = centerX + u * radiusX;
    currentY = centerY + v * radiusY;
    auto x2 = currentX + v * distance * radiusX;
    auto y2 = currentY - u * distance * radiusY;
    points.push_back({x2, y2});
    (*numBeziers)++;
    if (end == endAngle) {
      break;
    }
    start = end;
    end = std::min(endAngle, start + M_PI_2_F);
  }
  return points;
}

void Path::addOval(const Rect& oval, bool reversed, unsigned startIndex) {
  writableRef()->path.addOval(ToSkRect(oval), ToSkDirection(reversed), startIndex);
}

void Path::addArc(float centerX, float centerY, float radius, float startAngle, float endAngle,
                  bool reversed) {
  int numBeziers = 0;
  auto points = GetArcPoints(centerX, centerY, radius, radius, startAngle, endAngle, &numBeziers);
  PointIterator iter(points, reversed, 0);
  auto path = &(writableRef()->path);
  path->moveTo(iter.current());
  for (int i = 0; i < numBeziers; i++) {
    path->cubicTo(iter.next(), iter.next(), iter.next());
  }
  path->close();
}

void Path::addRoundRect(const Rect& rect, float radiusX, float radiusY, bool reversed,
                        unsigned startIndex) {
  writableRef()->path.addRRect(SkRRect::MakeRectXY(ToSkRect(rect), radiusX, radiusY),
                               ToSkDirection(reversed), startIndex);
}

void Path::addPath(const Path& src, PathOp op) {
  auto& path = writableRef()->path;
  const auto& newPath = src.pathRef->path;
  if (op == PathOp::Append) {
    if (path.isEmpty()) {
      path = newPath;
    } else {
      path.addPath(newPath);
    }
    return;
  }
  SkPathOp pathOp;
  switch (op) {
    case PathOp::Intersect:
      pathOp = SkPathOp::kIntersect_SkPathOp;
      break;
    case PathOp::Difference:
      pathOp = SkPathOp::kDifference_SkPathOp;
      break;
    case PathOp::XOR:
      pathOp = SkPathOp::kXOR_SkPathOp;
      break;
    default:
      pathOp = SkPathOp::kUnion_SkPathOp;
      break;
  }
  Op(path, newPath, pathOp, &path);
}

void Path::reset() {
  pathRef = std::make_shared<PathRef>();
}

void Path::transform(const Matrix& matrix) {
  if (matrix.isIdentity()) {
    return;
  }
  float values[9] = {};
  matrix.get9(values);
  SkMatrix skMatrix = {};
  skMatrix.set9(values);
  writableRef()->path.transform(skMatrix);
}

void Path::reverse() {
  auto& path = writableRef()->path;
  SkPath tempPath;
  tempPath.reverseAddPath(path);
  path = tempPath;
}

void Path::decompose(const PathIterator& iterator, void* info) const {
  if (iterator == nullptr) {
    return;
  }
  const auto& skPath = pathRef->path;
  SkPath::Iter iter(skPath, false);
  SkPoint points[4];
  SkPoint quads[5];
  SkPath::Verb verb;
  while ((verb = iter.next(points)) != SkPath::kDone_Verb) {
    switch (verb) {
      case SkPath::kMove_Verb:
        iterator(PathVerb::Move, reinterpret_cast<Point*>(points), info);
        break;
      case SkPath::kLine_Verb:
        iterator(PathVerb::Line, reinterpret_cast<Point*>(points), info);
        break;
      case SkPath::kQuad_Verb:
        iterator(PathVerb::Quad, reinterpret_cast<Point*>(points), info);
        break;
      case SkPath::kConic_Verb:
        // approximate with 2^1=2 quads.
        SkPath::ConvertConicToQuads(points[0], points[1], points[2], iter.conicWeight(), quads, 1);
        iterator(PathVerb::Quad, reinterpret_cast<Point*>(quads), info);
        iterator(PathVerb::Quad, reinterpret_cast<Point*>(quads) + 2, info);
        break;
      case SkPath::kCubic_Verb:
        iterator(PathVerb::Cubic, reinterpret_cast<Point*>(points), info);
        break;
      case SkPath::kClose_Verb:
        iterator(PathVerb::Close, reinterpret_cast<Point*>(points), info);
        break;
      default:
        break;
    }
  }
}

PathRef* Path::writableRef() {
  if (!pathRef.unique()) {
    pathRef = std::make_shared<PathRef>(pathRef->path);
  }
  return pathRef.get();
}

int Path::hash() const {
  return pathRef->path.countPoints();
}

size_t PathHash::operator()(const Path& path) const {
  return static_cast<size_t>(path.hash());
}
}  // namespace tgfx
