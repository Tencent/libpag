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

#include "ShapeRenderer.h"
#include <algorithm>
#include <functional>
#include "base/utils/EnumClassHash.h"
#include "base/utils/Interpolate.h"
#include "base/utils/MathExtra.h"
#include "core/PathEffect.h"
#include "core/PathMeasure.h"
#include "rendering/graphics/Graphic.h"
#include "rendering/graphics/Shape.h"
#include "rendering/utils/PathUtil.h"

namespace pag {

enum class ElementDataType { Paint, Path, Group };

class ElementData {
 public:
  virtual ~ElementData() = default;
  virtual ElementDataType type() const = 0;
  virtual std::unique_ptr<ElementData> clone() = 0;
  virtual void applyMatrix(const Matrix& matrix) = 0;
};

enum class PaintType { Fill, Stroke, GradientFill, GradientStroke };

/**
 * Defines attributes for drawing strokes.
 */
struct StrokePaint {
  Stroke getStroke() const {
    return Stroke(strokeWidth, lineCap, lineJoin, miterLimit);
  }

  float strokeWidth;
  Enum lineCap;
  Enum lineJoin;
  float miterLimit;
  std::vector<float> dashes;
  float dashOffset;
};

class PaintElement : public ElementData {
 public:
  explicit PaintElement(PaintType paintType) : paintType(paintType) {
  }

  ElementDataType type() const override {
    return ElementDataType::Paint;
  }

  std::unique_ptr<ElementData> clone() override {
    auto newPaint = new PaintElement(paintType);
    newPaint->blendMode = blendMode;
    newPaint->opacity = opacity;
    newPaint->color = color;
    newPaint->gradient = gradient;
    newPaint->stroke = stroke;
    newPaint->pathFillType = pathFillType;
    newPaint->compositeOrder = compositeOrder;
    return std::unique_ptr<ElementData>(newPaint);
  }

  void applyMatrix(const Matrix& matrix) override {
    if (paintType == PaintType::Fill || paintType == PaintType::GradientFill) {
      return;
    }
    stroke.strokeWidth *= fabsf(matrix.getMaxScale());
  }

  PaintType paintType = PaintType::Fill;
  Enum blendMode = BlendMode::Normal;
  Opacity opacity = Opaque;
  Enum compositeOrder = CompositeOrder::BelowPreviousInSameGroup;
  PathFillType pathFillType = PathFillType::Winding;
  Color color = White;
  GradientPaint gradient;
  StrokePaint stroke;
};

class PathElement : public ElementData {
 public:
  ElementDataType type() const override {
    return ElementDataType::Path;
  }

  std::unique_ptr<ElementData> clone() override {
    auto newPath = new PathElement();
    newPath->path = path;
    return std::unique_ptr<ElementData>(newPath);
  }

  void applyMatrix(const Matrix& matrix) override {
    path.transform(matrix);
  }

  Path path;
};

class GroupElement : public ElementData {
 public:
  ~GroupElement() override {
    for (auto& element : elements) {
      delete element;
    }
  }

  ElementDataType type() const override {
    return ElementDataType::Group;
  }

  std::unique_ptr<ElementData> clone() override {
    auto newGroup = new GroupElement();
    newGroup->blendMode = blendMode;
    newGroup->opacity = opacity;
    for (auto& data : elements) {
      auto element = data->clone().release();
      newGroup->elements.push_back(element);
    }
    return std::unique_ptr<ElementData>(newGroup);
  }

  void applyMatrix(const Matrix& matrix) override {
    for (auto& element : elements) {
      element->applyMatrix(matrix);
    }
  }

  std::vector<Path*> pathList() const {
    std::vector<Path*> list;
    for (auto& element : elements) {
      switch (element->type()) {
        case ElementDataType::Path: {
          auto pathElement = reinterpret_cast<PathElement*>(element);
          list.push_back(&pathElement->path);
        } break;
        case ElementDataType::Group: {
          auto group = reinterpret_cast<GroupElement*>(element);
          auto pathList = group->pathList();
          list.insert(list.end(), pathList.begin(), pathList.end());
        } break;
        default:
          break;
      }
    }
    return list;
  }

  void clear() {
    for (auto& element : elements) {
      delete element;
    }
    elements.clear();
  }

  Enum blendMode = BlendMode::Normal;
  Opacity opacity = Opaque;
  std::vector<ElementData*> elements;
};

void RectangleToPath(RectangleElement* rectangle, Path* path, Frame frame) {
  auto size = rectangle->size->getValueAt(frame);
  auto position = rectangle->position->getValueAt(frame);
  auto radius = rectangle->roundness->getValueAt(frame);
  auto hw = size.x * 0.5f;
  auto hh = size.y * 0.5f;
  if (radius > hw) {
    radius = hw;
  }
  if (radius > hh) {
    radius = hh;
  }
  auto rect =
      Rect::MakeXYWH(position.x - size.x * 0.5f, position.y - size.y * 0.5f, size.x, size.y);
  path->addRoundRect(rect, radius, radius, rectangle->reversed, 2);
}

void EllipseToPath(EllipseElement* ellipse, Path* path, Frame frame) {
  auto size = ellipse->size->getValueAt(frame);
  auto position = ellipse->position->getValueAt(frame);
  auto rect =
      Rect::MakeXYWH(position.x - size.x * 0.5f, position.y - size.y * 0.5f, size.x, size.y);
  path->addOval(rect, ellipse->reversed);
}

static void AddCurveToPath(Path* path, float centerX, float centerY, float angleDelta, float dx1,
                           float dy1, float roundness1, float dx2, float dy2, float roundness2) {
  auto control1X = dx1 - dy1 * roundness1 * angleDelta + centerX;
  auto control1Y = dy1 + dx1 * roundness1 * angleDelta + centerY;
  auto control2X = dx2 + dy2 * roundness2 * angleDelta + centerX;
  auto control2Y = dy2 - dx2 * roundness2 * angleDelta + centerY;
  path->cubicTo(control1X, control1Y, control2X, control2Y, dx2 + centerX, dy2 + centerY);
}

static void ConvertPolyStartToPath(Path* path, float centerX, float centerY, float points,
                                   float rotation, float innerRadius, float outerRadius,
                                   float innerRoundness, float outerRoundness, bool reversed) {
  float direction = reversed ? -1 : 1;
  auto angleStep = static_cast<float>(M_PI) / points;
  auto currentAngle = (rotation - 90) * static_cast<float>(M_PI) / 180;
  auto numPoints = static_cast<int>(ceilf(points)) * 2;
  auto decimalPart = points - floorf(points);
  int decimalIndex = -2;
  if (decimalPart != 0) {
    decimalIndex = direction > 0 ? 1 : numPoints - 3;
    currentAngle -= angleStep * decimalPart * 2.0f;
  }

  // Move to the first point.
  auto lastDx = outerRadius * cosf(currentAngle);
  auto lastDy = outerRadius * sinf(currentAngle);
  path->moveTo(lastDx + centerX, lastDy + centerY);

  auto outerFlag = false;
  for (int i = 0; i < numPoints; i++) {
    auto radius = outerFlag ? outerRadius : innerRadius;
    auto angleDelta = angleStep * direction;
    if (i == decimalIndex || i == decimalIndex + 1) {
      radius = innerRadius + decimalPart * (radius - innerRadius);
      angleDelta *= decimalPart;
    }
    currentAngle += angleDelta;
    auto dx = radius * cosf(currentAngle);
    auto dy = radius * sinf(currentAngle);
    if (innerRoundness != 0 || outerRoundness != 0) {
      float lastRoundness, roundness;
      if (outerFlag) {
        lastRoundness = innerRoundness;
        roundness = outerRoundness;
      } else {
        lastRoundness = outerRoundness;
        roundness = innerRoundness;
      }
      AddCurveToPath(path, centerX, centerY, angleDelta * 0.5f, lastDx, lastDy, lastRoundness, dx,
                     dy, roundness);
      lastDx = dx;
      lastDy = dy;
    } else {
      path->lineTo(dx + centerX, dy + centerY);
    }
    outerFlag = !outerFlag;
  }
  path->close();
}

static void ConvertPolygonToPath(Path* path, float centerX, float centerY, float points,
                                 float rotation, float radius, float roundness, bool reversed) {
  auto numPoints = static_cast<int>(floorf(points));
  float direction = reversed ? -1 : 1;
  auto angleStep = static_cast<float>(M_PI) * 2 / static_cast<float>(numPoints);
  auto currentAngle = (rotation - 90) * static_cast<float>(M_PI) / 180;

  // Move to the first point.
  auto lastDx = radius * cosf(currentAngle);
  auto lastDy = radius * sinf(currentAngle);
  path->moveTo(lastDx + centerX, lastDy + centerY);

  auto outerFlag = false;
  for (int i = 0; i < numPoints; i++) {
    auto angleDelta = angleStep * direction;
    currentAngle += angleDelta;
    auto dx = radius * cosf(currentAngle);
    auto dy = radius * sinf(currentAngle);
    if (roundness != 0) {
      AddCurveToPath(path, centerX, centerY, angleDelta * 0.5f, lastDx, lastDy, roundness, dx, dy,
                     roundness);
      lastDx = dx;
      lastDy = dy;
    } else {
      path->lineTo(dx + centerX, dy + centerY);
    }
    outerFlag = !outerFlag;
  }
  path->close();
}

void PolyStarToPath(PolyStarElement* polyStar, Path* path, Frame frame) {
  auto points = polyStar->points->getValueAt(frame);
  auto position = polyStar->position->getValueAt(frame);
  auto rotation = polyStar->rotation->getValueAt(frame);
  auto innerRadius = polyStar->innerRadius->getValueAt(frame);
  auto outerRadius = polyStar->outerRadius->getValueAt(frame);
  auto innerRoundness = polyStar->innerRoundness->getValueAt(frame);
  auto outerRoundness = polyStar->outerRoundness->getValueAt(frame);
  if (polyStar->polyType == PolyStarType::Star) {
    ConvertPolyStartToPath(path, position.x, position.y, points, rotation, innerRadius, outerRadius,
                           innerRoundness, outerRoundness, polyStar->reversed);
  } else {
    ConvertPolygonToPath(path, position.x, position.y, points, rotation, outerRadius,
                         outerRoundness, polyStar->reversed);
  }
}

void ShapePathToPath(ShapePathElement* shapePath, Path* path, Frame frame) {
  auto pathData = shapePath->shapePath->getValueAt(frame);
  if (pathData == nullptr) {
    return;
  }
  path->addPath(ToPath(*pathData));
}

PathFillType ToPathFillType(Enum rule) {
  return rule == FillRule::EvenOdd ? PathFillType::EvenOdd : PathFillType::Winding;
}

PaintElement* FillToPaint(FillElement* fill, Frame frame) {
  if (fill->opacity->getValueAt(frame) <= 0) {
    return nullptr;
  }
  auto paint = new PaintElement(PaintType::Fill);
  paint->blendMode = fill->blendMode;
  paint->opacity = fill->opacity->getValueAt(frame);
  paint->color = fill->color->getValueAt(frame);
  paint->pathFillType = ToPathFillType(fill->fillRule);
  paint->compositeOrder = fill->composite;
  return paint;
}

PaintElement* StrokeToPaint(StrokeElement* stroke, Frame frame) {
  if (stroke->opacity->getValueAt(frame) <= 0 || stroke->strokeWidth->getValueAt(frame) <= 0) {
    return nullptr;
  }
  auto paint = new PaintElement(PaintType::Stroke);
  paint->blendMode = stroke->blendMode;
  paint->opacity = stroke->opacity->getValueAt(frame);
  paint->compositeOrder = stroke->composite;
  paint->color = stroke->color->getValueAt(frame);
  paint->stroke.strokeWidth = stroke->strokeWidth->getValueAt(frame);
  paint->stroke.lineCap = stroke->lineCap;
  paint->stroke.lineJoin = stroke->lineJoin;
  paint->stroke.miterLimit = stroke->miterLimit->getValueAt(frame);
  if (!stroke->dashes.empty()) {
    for (auto& dash : stroke->dashes) {
      paint->stroke.dashes.push_back(dash->getValueAt(frame));
    }
    paint->stroke.dashOffset = stroke->dashOffset->getValueAt(frame);
  }
  return paint;
}

void ConvertColorStop(const GradientColorHandle& gradientColor, std::vector<Color>& colorValues,
                      std::vector<float>& colorPositions) {
  auto colorStops = gradientColor->colorStops;
  for (size_t i = 0; i < colorStops.size(); i++) {
    const auto& stop = colorStops[i];
    colorValues.push_back(stop.color);
    colorPositions.push_back(stop.position);
    if (stop.midpoint != 0.5f && i < colorStops.size() - 1) {
      const auto& nextStop = colorStops[i + 1];
      auto midColor = Interpolate(stop.color, nextStop.color, 0.5f);
      colorValues.push_back(midColor);
      auto position = stop.position + (nextStop.position - stop.position) * stop.midpoint;
      colorPositions.push_back(position);
    }
  }
}

void ConvertAlphaStop(const GradientColorHandle& gradientColor, std::vector<Opacity>& alphaValues,
                      std::vector<float>& alphaPositions) {
  auto alphaStops = gradientColor->alphaStops;
  for (size_t i = 0; i < alphaStops.size(); i++) {
    const auto& stop = alphaStops[i];
    alphaValues.push_back(stop.opacity);
    alphaPositions.push_back(stop.position);
    if (stop.midpoint != 0.5f && i < alphaStops.size() - 1) {
      const auto& nextStop = alphaStops[i + 1];
      auto midAlpha = Interpolate(stop.opacity, nextStop.opacity, 0.5f);
      alphaValues.push_back(midAlpha);
      auto position = stop.position + (nextStop.position - stop.position) * stop.midpoint;
      alphaPositions.push_back(position);
    }
  }
}

GradientPaint MakeGradientPaint(Enum fillType, Point startPoint, Point endPoint,
                                const GradientColorHandle& gradientColor, const Matrix& matrix) {
  Point points[2] = {Point::Make(startPoint.x, startPoint.y), Point::Make(endPoint.x, endPoint.y)};
  matrix.mapPoints(points, 2);
  GradientPaint gradient = {};
  gradient.gradientType = fillType;
  gradient.startPoint = points[0];
  gradient.endPoint = points[1];

  std::vector<Color> colorValues;
  std::vector<float> colorPositions;
  ConvertColorStop(gradientColor, colorValues, colorPositions);

  std::vector<Opacity> alphaValues;
  std::vector<float> alphaPositions;
  ConvertAlphaStop(gradientColor, alphaValues, alphaPositions);

  size_t colorIndex = 0;
  size_t alphaIndex = 0;
  while (colorIndex < colorValues.size() && alphaIndex < alphaValues.size()) {
    auto colorPosition = colorPositions[colorIndex];
    auto alphaPosition = alphaPositions[alphaIndex];
    if (colorPosition == alphaPosition) {
      gradient.positions.push_back(colorPosition);
      gradient.colors.push_back(colorValues[colorIndex++]);
      gradient.alphas.push_back(alphaValues[alphaIndex++]);
    } else if (colorPosition < alphaPosition) {
      gradient.positions.push_back(colorPosition);
      Opacity alpha;
      if (alphaIndex > 0) {
        auto lastPosition = alphaPositions[alphaIndex - 1];
        auto factor = (colorPosition - lastPosition) * 1.0f / (alphaPosition - lastPosition);
        alpha = Interpolate(alphaValues[alphaIndex - 1], alphaValues[alphaIndex], factor);
      } else {
        alpha = alphaValues[alphaIndex];
      }
      gradient.colors.push_back(colorValues[colorIndex++]);
      gradient.alphas.push_back(alpha);
    } else {
      gradient.positions.push_back(alphaPosition);
      Color color = {};
      if (colorIndex > 0) {
        auto lastPosition = colorPositions[colorIndex - 1];
        auto factor = (alphaPosition - lastPosition) * 1.0f / (colorPosition - lastPosition);
        auto lastColor = colorValues[colorIndex - 1];
        auto currentColor = colorValues[colorIndex];
        color = Interpolate(lastColor, currentColor, factor);
      } else {
        color = colorValues[colorIndex];
      }
      gradient.colors.push_back(color);
      gradient.alphas.push_back(alphaValues[alphaIndex++]);
    }
  }

  auto lastAlpha = alphaValues.back();
  while (colorIndex < colorValues.size()) {
    gradient.positions.push_back(colorPositions[colorIndex]);
    gradient.colors.push_back(colorValues[colorIndex++]);
    gradient.alphas.push_back(lastAlpha);
  }

  auto lastColor = colorValues.back();
  while (alphaIndex < alphaValues.size()) {
    gradient.positions.push_back(alphaPositions[alphaIndex]);
    gradient.colors.push_back(lastColor);
    gradient.alphas.push_back(alphaValues[alphaIndex++]);
  }
  return gradient;
}

PaintElement* GradientFillToPaint(GradientFillElement* fill, const Matrix& matrix, Frame frame) {
  if (fill->opacity->getValueAt(frame) <= 0) {
    return nullptr;
  }
  auto paint = new PaintElement(PaintType::GradientFill);
  paint->blendMode = fill->blendMode;
  paint->opacity = fill->opacity->getValueAt(frame);
  paint->compositeOrder = fill->composite;
  paint->gradient =
      MakeGradientPaint(fill->fillType, fill->startPoint->getValueAt(frame),
                        fill->endPoint->getValueAt(frame), fill->colors->getValueAt(frame), matrix);
  paint->pathFillType = ToPathFillType(fill->fillRule);

  return paint;
}

PaintElement* GradientStrokeToPaint(GradientStrokeElement* stroke, const Matrix& matrix,
                                    Frame frame) {
  if (stroke->opacity->getValueAt(frame) <= 0 || stroke->strokeWidth->getValueAt(frame) <= 0) {
    return nullptr;
  }
  auto paint = new PaintElement(PaintType::GradientStroke);
  paint->blendMode = stroke->blendMode;
  paint->opacity = stroke->opacity->getValueAt(frame);
  paint->compositeOrder = stroke->composite;
  paint->stroke.strokeWidth = stroke->strokeWidth->getValueAt(frame);
  paint->stroke.lineCap = stroke->lineCap;
  paint->stroke.lineJoin = stroke->lineJoin;
  paint->stroke.miterLimit = stroke->miterLimit->getValueAt(frame);
  if (!stroke->dashes.empty()) {
    for (auto& dash : stroke->dashes) {
      paint->stroke.dashes.push_back(dash->getValueAt(frame));
    }
    paint->stroke.dashOffset = stroke->dashOffset->getValueAt(frame);
  }
  paint->gradient = MakeGradientPaint(stroke->fillType, stroke->startPoint->getValueAt(frame),
                                      stroke->endPoint->getValueAt(frame),
                                      stroke->colors->getValueAt(frame), matrix);
  return paint;
}

struct TrimSegment {
  float start;
  float end;
};

void ApplyTrimPathIndividually(const std::vector<Path*>& pathList,
                               std::vector<TrimSegment> segments) {
  float totalLength = 0;
  std::vector<std::unique_ptr<PathMeasure>> measureList;
  for (auto& path : pathList) {
    auto pathMeasure = PathMeasure::MakeFrom(*path);
    totalLength += pathMeasure->getLength();
    measureList.push_back(std::move(pathMeasure));
  }
  for (auto& segment : segments) {
    segment.start *= totalLength;
    segment.end *= totalLength;
  }
  float addedLength = 0;
  int index = 0;
  Path tempPath = {};
  for (auto& pathMeasure : measureList) {
    auto& path = pathList[index++];
    auto pathLength = pathMeasure->getLength();
    if (pathLength == 0) {
      continue;
    }
    bool intersect = false;
    for (auto& segment : segments) {
      if (addedLength >= segment.end || addedLength + pathLength <= segment.start) {
        continue;
      }
      intersect = true;
      pathMeasure->getSegment(segment.start - addedLength, segment.end - addedLength, &tempPath);
    }
    if (intersect) {
      *path = tempPath;
    } else {
      path->reset();
    }
    tempPath.reset();
    addedLength += pathLength;
  }
}

void ApplyTrimPaths(TrimPathsElement* trimPaths, std::vector<Path*>& pathList, float start,
                    float end, bool reversed) {
  if (start == 0 && end == 1) {
    return;
  }
  std::vector<TrimSegment> segments;
  if (start < 0) {
    // [start...0...end...1]
    segments.push_back({start + 1, 1});
    segments.push_back({0, end});
  } else if (end > 1) {
    // [0...start...1...end]
    segments.push_back({start, 1});
    segments.push_back({0, end - 1});
  } else {
    // [0...start...end...1]
    segments.push_back({start, end});
  }
  if (trimPaths->trimType == TrimPathsType::Simultaneously) {
    Path tempPath = {};
    for (auto& path : pathList) {
      auto pathMeasure = PathMeasure::MakeFrom(*path);
      auto length = pathMeasure->getLength();
      if (length == 0) {
        continue;
      }
      for (auto segment : segments) {
        auto startD = length * segment.start;
        auto endD = length * segment.end;
        pathMeasure->getSegment(startD, endD, &tempPath);
      }
      *path = tempPath;
      tempPath.reset();
    }
  } else {
    auto list = pathList;
    if (reversed) {
      std::reverse(list.begin(), list.end());
    }
    ApplyTrimPathIndividually(list, segments);
  }
}

void ApplyTrimPaths(TrimPathsElement* trimPaths, std::vector<Path*> pathList, Frame frame) {
  auto start = trimPaths->start->getValueAt(frame);
  auto end = trimPaths->end->getValueAt(frame);
  auto offset = fmodf(trimPaths->offset->getValueAt(frame), 360.0f) / 360.0f;
  start += offset;
  end += offset;
  if (fabsf(start - end) < FLT_EPSILON) {
    for (auto& path : pathList) {
      path->reset();
    }
    return;
  }
  bool reversed = start > end;
  if (reversed) {
    start = 1 - start;
    end = 1 - end;
    for (auto& path : pathList) {
      path->reverse();
    }
  }
  if (start > 1 && end > 1) {
    start -= 1.0f;
    end -= 1.0f;
  } else if (start < 0 && end < 0) {
    start += 1.0f;
    end += 1.0f;
  }
  ApplyTrimPaths(trimPaths, pathList, start, end, reversed);
}

void ApplyMergePaths(MergePathsElement* mergePaths, GroupElement* group) {
  auto pathList = group->pathList();
  if (pathList.empty()) {
    return;
  }
  PathOp pathOp;
  switch (mergePaths->mode) {
    case MergePathsMode::Merge:
      pathOp = PathOp::Append;
      break;
    case MergePathsMode::Intersect:
      pathOp = PathOp::Intersect;
      break;
    case MergePathsMode::Subtract:
      pathOp = PathOp::Difference;
      break;
    case MergePathsMode::ExcludeIntersections:
      pathOp = PathOp::XOR;
      break;
    default:
      pathOp = PathOp::Union;
      break;
  }
  auto tempPath = *(pathList[0]);
  auto size = static_cast<int>(pathList.size());
  for (int i = 1; i < size; i++) {
    auto path = pathList[i];
    tempPath.addPath(*path, pathOp);
  }
  group->clear();
  auto pathElement = new PathElement();
  pathElement->path = tempPath;
  group->elements.push_back(pathElement);
}

void ApplyRepeater(RepeaterElement* repeater, GroupElement* group, Frame frame) {
  auto copies = repeater->copies->getValueAt(frame);
  if (copies < 0) {
    return;
  }
  if (copies == 0) {
    group->clear();
    return;
  }
  auto maxCount = ceilf(copies);
  auto offset = repeater->offset->getValueAt(frame);
  auto anchorPoint = repeater->transform->anchorPoint->getValueAt(frame);
  auto position = repeater->transform->position->getValueAt(frame);
  auto scale = repeater->transform->scale->getValueAt(frame);
  auto rotation = repeater->transform->rotation->getValueAt(frame);
  auto startOpacity = repeater->transform->startOpacity->getValueAt(frame);
  auto endOpacity = repeater->transform->endOpacity->getValueAt(frame);
  float i = 0;
  std::vector<ElementData*> elements = {};
  while (i < maxCount) {
    auto progress = i + offset;
    auto newGroup = static_cast<GroupElement*>(group->clone().release());
    if (repeater->composite == RepeaterOrder::Below) {
      elements.push_back(newGroup);
    } else {
      elements.insert(elements.begin(), newGroup);
    }
    if (i == maxCount - 1) {
      if (progress != copies + offset) {
        newGroup->opacity = static_cast<Opacity>(newGroup->opacity * (copies - i));
      }
    }
    auto matrix = Matrix::I();
    matrix.postTranslate(-anchorPoint.x, -anchorPoint.y);
    matrix.postScale(powf(scale.x, progress), powf(scale.y, progress));
    matrix.postRotate(rotation * progress);
    matrix.postTranslate(position.x * progress, position.y * progress);
    matrix.postTranslate(anchorPoint.x, anchorPoint.y);
    newGroup->applyMatrix(matrix);
    auto newOpacity = Interpolate(startOpacity, endOpacity, progress / maxCount);
    newGroup->opacity = OpacityConcat(newGroup->opacity, newOpacity);
    i += 1.0f;
  }
  group->clear();
  group->elements = elements;
}

void ApplyRoundCorners(RoundCornersElement* roundCorners, std::vector<Path*> pathList,
                       Frame frame) {
  auto radius = roundCorners->radius->getValueAt(frame);
  auto effect = PathEffect::MakeCorner(radius);
  if (effect == nullptr) {
    return;
  }
  for (auto& path : pathList) {
    effect->applyTo(path);
  }
}

void SkewFromAxis(Matrix* matrix, float skew, float skewAxis) {
  if (skew == 0 && skewAxis == 0) {
    return;
  }
  auto u = cosf(skewAxis);
  auto v = sinf(skewAxis);
  Matrix temp = {};
  temp.setAll(u, -v, 0, v, u, 0, 0, 0, 1);
  matrix->postConcat(temp);
  auto w = tanf(skew);
  temp.setAll(1, w, 0, 0, 1, 0, 0, 0, 1);
  matrix->postConcat(temp);
  temp.setAll(u, v, 0, -v, u, 0, 0, 0, 1);
  matrix->postConcat(temp);
}

Transform ShapeTransformToTransform(const ShapeTransform* transform, Frame frame) {
  auto matrix = Matrix::I();
  auto anchorPoint = transform->anchorPoint->getValueAt(frame);
  auto scale = transform->scale->getValueAt(frame);
  auto position = transform->position->getValueAt(frame);
  auto skew = transform->skew->getValueAt(frame);
  auto skewAxis = transform->skewAxis->getValueAt(frame);
  auto rotation = transform->rotation->getValueAt(frame);
  auto opacity = transform->opacity->getValueAt(frame);
  matrix.postTranslate(-anchorPoint.x, -anchorPoint.y);
  matrix.postScale(scale.x, scale.y);
  if (skew != 0) {
    SkewFromAxis(&matrix, DegreesToRadians(-skew), DegreesToRadians(skewAxis));
  }
  matrix.postRotate(rotation);
  matrix.postTranslate(position.x, position.y);
  return {matrix, opacity};
}

void RenderElements(const std::vector<ShapeElement*>& list, const Matrix& parentMatrix,
                    GroupElement* parentGroup, Frame frame);

void RenderElements_ShapeGroup(ShapeElement* element, const Matrix& parentMatrix,
                               GroupElement* parentGroup, Frame frame) {
  auto shape = reinterpret_cast<ShapeGroupElement*>(element);
  auto transform = ShapeTransformToTransform(shape->transform, frame);
  transform.matrix.postConcat(parentMatrix);
  auto group = new GroupElement();
  group->opacity = transform.opacity;
  group->blendMode = shape->blendMode;
  RenderElements(shape->elements, transform.matrix, group, frame);
  parentGroup->elements.push_back(group);
}

void RenderElements_Rectangle(ShapeElement* element, const Matrix& parentMatrix,
                              GroupElement* parentGroup, Frame frame) {
  auto pathElement = new PathElement();
  RectangleToPath(static_cast<RectangleElement*>(element), &pathElement->path, frame);
  pathElement->path.transform(parentMatrix);
  parentGroup->elements.push_back(pathElement);
}

void RenderElements_Ellipse(ShapeElement* element, const Matrix& parentMatrix,
                            GroupElement* parentGroup, Frame frame) {
  auto pathElement = new PathElement();
  EllipseToPath(static_cast<EllipseElement*>(element), &pathElement->path, frame);
  pathElement->path.transform(parentMatrix);
  parentGroup->elements.push_back(pathElement);
}

void RenderElements_PolyStar(ShapeElement* element, const Matrix& parentMatrix,
                             GroupElement* parentGroup, Frame frame) {
  auto pathElement = new PathElement();
  PolyStarToPath(static_cast<PolyStarElement*>(element), &pathElement->path, frame);
  pathElement->path.transform(parentMatrix);
  parentGroup->elements.push_back(pathElement);
}

void RenderElements_ShapePath(ShapeElement* element, const Matrix& parentMatrix,
                              GroupElement* parentGroup, Frame frame) {
  auto pathElement = new PathElement();
  ShapePathToPath(static_cast<ShapePathElement*>(element), &pathElement->path, frame);
  pathElement->path.transform(parentMatrix);
  parentGroup->elements.push_back(pathElement);
}

void RenderElements_Fill(ShapeElement* element, const Matrix&, GroupElement* parentGroup,
                         Frame frame) {
  auto fill = static_cast<FillElement*>(element);
  auto paint = FillToPaint(fill, frame);
  if (paint != nullptr) {
    parentGroup->elements.push_back(paint);
  }
}

void RenderElements_Stroke(ShapeElement* element, const Matrix&, GroupElement* parentGroup,
                           Frame frame) {
  auto stroke = static_cast<StrokeElement*>(element);
  auto paint = StrokeToPaint(stroke, frame);
  if (paint != nullptr) {
    parentGroup->elements.push_back(paint);
  }
}

void RenderElements_GradientFill(ShapeElement* element, const Matrix& parentMatrix,
                                 GroupElement* parentGroup, Frame frame) {
  auto fill = static_cast<GradientFillElement*>(element);
  auto paint = GradientFillToPaint(fill, parentMatrix, frame);
  if (paint != nullptr) {
    parentGroup->elements.push_back(paint);
  }
}

void RenderElements_GradientStroke(ShapeElement* element, const Matrix& parentMatrix,
                                   GroupElement* parentGroup, Frame frame) {
  auto stroke = static_cast<GradientStrokeElement*>(element);
  auto paint = GradientStrokeToPaint(stroke, parentMatrix, frame);
  if (paint != nullptr) {
    parentGroup->elements.push_back(paint);
  }
}

void RenderElements_MergePaths(ShapeElement* element, const Matrix&, GroupElement* parentGroup,
                               Frame) {
  ApplyMergePaths(static_cast<MergePathsElement*>(element), parentGroup);
}

void RenderElements_Repeater(ShapeElement* element, const Matrix&, GroupElement* parentGroup,
                             Frame frame) {
  ApplyRepeater(static_cast<RepeaterElement*>(element), parentGroup, frame);
}

void RenderElements_TrimPaths(ShapeElement* element, const Matrix&, GroupElement* parentGroup,
                              Frame frame) {
  ApplyTrimPaths(static_cast<TrimPathsElement*>(element), parentGroup->pathList(), frame);
}

void RenderElements_RoundCorners(ShapeElement* element, const Matrix&, GroupElement* parentGroup,
                                 Frame frame) {
  ApplyRoundCorners(static_cast<RoundCornersElement*>(element), parentGroup->pathList(), frame);
}

using RenderElementsHandler = void(ShapeElement* element, const Matrix& parentMatrix,
                                   GroupElement* parentGroup, Frame frame);

static const std::unordered_map<ShapeType, std::function<RenderElementsHandler>, EnumClassHash>
    elementHandlers = {{ShapeType::ShapeGroup, RenderElements_ShapeGroup},
                       {ShapeType::Rectangle, RenderElements_Rectangle},
                       {ShapeType::Ellipse, RenderElements_Ellipse},
                       {ShapeType::PolyStar, RenderElements_PolyStar},
                       {ShapeType::ShapePath, RenderElements_ShapePath},
                       {ShapeType::Fill, RenderElements_Fill},
                       {ShapeType::Stroke, RenderElements_Stroke},
                       {ShapeType::GradientFill, RenderElements_GradientFill},
                       {ShapeType::GradientStroke, RenderElements_GradientStroke},
                       {ShapeType::MergePaths, RenderElements_MergePaths},
                       {ShapeType::Repeater, RenderElements_Repeater},
                       {ShapeType::TrimPaths, RenderElements_TrimPaths},
                       {ShapeType::RoundCorners, RenderElements_RoundCorners}};

void RenderElements(const std::vector<ShapeElement*>& list, const Matrix& parentMatrix,
                    GroupElement* parentGroup, Frame frame) {
  for (auto& element : list) {
    auto iter = elementHandlers.find(element->type());
    if (iter != elementHandlers.end()) {
      iter->second(element, parentMatrix, parentGroup, frame);
    }
  }
}

std::unique_ptr<PathEffect> CreateDashEffect(const std::vector<float>& dashes, float dashOffset) {
  auto dashCount = dashes.size();
  auto size = static_cast<int>(dashCount);
  int times = 1;
  if (size % 2 == 1) {
    size *= 2;
    times = 2;
  }
  auto dashList = new float[size];
  int index = 0;
  for (int i = 0; i < times; i++) {
    for (size_t j = 0; j < dashCount; j++) {
      dashList[index++] = static_cast<float>(dashes[j]);
    }
  }
  auto dashEffect = PathEffect::MakeDash(dashList, size, dashOffset);
  delete[] dashList;
  return dashEffect;
}

void ApplyStrokeToPath(Path* path, const StrokePaint& stroke) {
  if (!stroke.dashes.empty()) {
    auto dashEffect = CreateDashEffect(stroke.dashes, stroke.dashOffset);
    if (dashEffect) {
      dashEffect->applyTo(path);
    }
  }
  auto strokeEffect = PathEffect::MakeStroke(stroke.getStroke());
  if (strokeEffect) {
    strokeEffect->applyTo(path);
  }
}

std::shared_ptr<Graphic> RenderShape(PaintElement* paint, Path* path) {
  Path shapePath = *path;
  auto paintType = paint->paintType;
  if (paintType == PaintType::Stroke || paintType == PaintType::GradientStroke) {
    ApplyStrokeToPath(&shapePath, paint->stroke);
  }
  if (shapePath.getFillType() == PathFillType::Winding) {
    shapePath.setFillType(paint->pathFillType);
  }
  std::shared_ptr<Graphic> shape = nullptr;
  if (paintType == PaintType::GradientStroke || paintType == PaintType::GradientFill) {
    shape = Shape::MakeFrom(shapePath, paint->gradient);
  } else {
    shape = Shape::MakeFrom(shapePath, paint->color);
  }
  auto modifier = Modifier::MakeBlend(paint->opacity, paint->blendMode);
  return Graphic::MakeCompose(shape, modifier);
}

std::shared_ptr<Graphic> RenderShape(GroupElement* group, Path* path) {
  std::vector<std::shared_ptr<Graphic>> contents = {};
  for (auto& element : group->elements) {
    switch (element->type()) {
      case ElementDataType::Path: {
        auto pathElement = reinterpret_cast<PathElement*>(element);
        path->addPath(pathElement->path);
      } break;
      case ElementDataType::Paint: {
        auto paint = reinterpret_cast<PaintElement*>(element);
        auto shape = RenderShape(paint, path);
        if (shape) {
          if (paint->compositeOrder == CompositeOrder::AbovePreviousInSameGroup) {
            contents.push_back(shape);
          } else {
            contents.insert(contents.begin(), shape);
          }
        }
      } break;
      case ElementDataType::Group: {
        Path tempPath = {};
        auto shape = RenderShape(static_cast<GroupElement*>(element), &tempPath);
        path->addPath(tempPath);
        if (shape) {
          contents.insert(contents.begin(), shape);
        }
      } break;
    }
  }
  auto shape = Graphic::MakeCompose(contents);
  auto modifier = Modifier::MakeBlend(group->opacity, group->blendMode);
  return Graphic::MakeCompose(shape, modifier);
}

std::shared_ptr<Graphic> RenderShapes(const std::vector<ShapeElement*>& contents,
                                      Frame layerFrame) {
  GroupElement rootGroup;
  auto matrix = Matrix::I();
  RenderElements(contents, matrix, &rootGroup, layerFrame);
  Path tempPath = {};
  return RenderShape(&rootGroup, &tempPath);
}
}  // namespace pag
