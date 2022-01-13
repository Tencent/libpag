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

#include "FTPath.h"

namespace pag {
bool FTPath::isEmpty() const {
  return points.empty();
}

void FTPath::moveTo(const Point& point) {
  if (!points.empty()) {
    contours.push_back(static_cast<int>(points.size()) - 1);
  }
  verbs.push_back(PathVerb::Move);
  points.push_back({FloatToFDot6(point.x), FloatToFDot6(point.y)});
  tags.push_back(FT_CURVE_TAG_ON);
}

void FTPath::lineTo(const Point& point) {
  verbs.push_back(PathVerb::Line);
  points.push_back({FloatToFDot6(point.x), FloatToFDot6(point.y)});
  tags.push_back(FT_CURVE_TAG_ON);
}

void FTPath::quadTo(const Point& control, const Point& end) {
  verbs.push_back(PathVerb::Quad);
  points.push_back({FloatToFDot6(control.x), FloatToFDot6(control.y)});
  tags.push_back(FT_CURVE_TAG_CONIC);
  points.push_back({FloatToFDot6(end.x), FloatToFDot6(end.y)});
  tags.push_back(FT_CURVE_TAG_ON);
}

void FTPath::cubicTo(const Point& control1, const Point& control2, const Point& end) {
  verbs.push_back(PathVerb::Cubic);
  points.push_back({FloatToFDot6(control1.x), FloatToFDot6(control1.y)});
  tags.push_back(FT_CURVE_TAG_CUBIC);
  points.push_back({FloatToFDot6(control2.x), FloatToFDot6(control2.y)});
  tags.push_back(FT_CURVE_TAG_CUBIC);
  points.push_back({FloatToFDot6(end.x), FloatToFDot6(end.y)});
  tags.push_back(FT_CURVE_TAG_ON);
}

void FTPath::close() {
  if (!verbs.empty() && verbs[verbs.size() - 1] == PathVerb::Close) {
    return;
  }
  auto lastContourPointIndex = contours.empty() ? -1 : contours[contours.size() - 1];
  if (points.size() - lastContourPointIndex < 2) {
    return;
  }
  auto startPoint = points[lastContourPointIndex + 1];
  auto endPoint = points[points.size() - 1];
  if (startPoint.x != endPoint.x || startPoint.y != endPoint.y) {
    verbs.push_back(PathVerb::Line);
    points.push_back(startPoint);
    tags.push_back(FT_CURVE_TAG_ON);
  }
  verbs.push_back(PathVerb::Close);
}

std::vector<std::shared_ptr<FreetypeOutline>> FTPath::getOutlines() const {
  if (points.empty()) {
    return {};
  }
  std::vector<std::shared_ptr<FreetypeOutline>> outlines = {};
  auto outline = std::make_shared<FreetypeOutline>();
  auto contourCount = contours.size() + 1;
  auto startPointIndex = 0;
  for (size_t i = 0; i < contourCount; i++) {
    auto contourPointIndex = contours.size() > i ? contours[i] : points.size() - 1;
    if (contourPointIndex - startPointIndex >= FT_OUTLINE_POINTS_MAX) {
      if (!finalizeOutline(outline.get(), startPointIndex)) {
        return {};
      }
      outlines.push_back(outline);
      outline = std::make_shared<FreetypeOutline>();
      startPointIndex = contours[i - 1] + 1;
    }
    outline->contours.push_back(static_cast<int16_t>(contourPointIndex - startPointIndex));
  }
  if (finalizeOutline(outline.get(), startPointIndex)) {
    outlines.push_back(outline);
  }
  return outlines;
}

bool FTPath::finalizeOutline(FreetypeOutline* outline, int startPointIndex) const {
  if (outline->contours.empty()) {
    return false;
  }
  auto endPointIndex = outline->contours[outline->contours.size() - 1];
  outline->outline.points = const_cast<FT_Vector*>(&(points[startPointIndex]));
  outline->outline.tags = const_cast<char*>(&(tags[startPointIndex]));
  outline->outline.contours = const_cast<int16_t*>(&(outline->contours[0]));
  outline->outline.n_points = static_cast<int16_t>(endPointIndex + 1);
  outline->outline.n_contours = static_cast<int16_t>(outline->contours.size());
  switch (getFillType()) {
    case PathFillType::EvenOdd:
      outline->outline.flags = FT_OUTLINE_EVEN_ODD_FILL;
      break;
    case PathFillType::InverseEvenOdd:
      outline->outline.flags = FT_OUTLINE_EVEN_ODD_FILL | FT_OUTLINE_REVERSE_FILL;
      break;
    case PathFillType::Winding:
      outline->outline.flags = FT_OUTLINE_NONE;
      break;
    case PathFillType::InverseWinding:
      outline->outline.flags = FT_OUTLINE_REVERSE_FILL;
      break;
  }
  return true;
}

}  // namespace pag