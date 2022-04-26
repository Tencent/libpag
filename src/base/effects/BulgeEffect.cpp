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

#include "base/utils/Verify.h"
#include "pag/file.h"
#include "tgfx/core/Path.h"

namespace pag {
static void ConvertEllipseToPath(tgfx::Path* path, float centerX, float centerY, float radiusX,
                                 float radiusY) {
  auto start = -static_cast<float>(M_PI_2);
  auto endAngle = static_cast<float>(M_PI + M_PI_2);
  auto end = 0;
  auto currentX = centerX + cosf(start) * radiusX;
  auto currentY = centerY + sinf(start) * radiusY;
  path->moveTo(currentX, currentY);
  auto u = cosf(start);
  auto v = sinf(start);
  for (int i = 0; i < 4; i++) {
    auto addAngle = end - start;
    auto a = 4 * tanf(addAngle * 0.25f) / 3;
    auto x1 = currentX - v * a * radiusX;
    auto y1 = currentY + u * a * radiusY;
    u = cosf(end);
    v = sinf(end);
    currentX = centerX + u * radiusX;
    currentY = centerY + v * radiusY;
    auto x2 = currentX + v * a * radiusX;
    auto y2 = currentY - u * a * radiusY;
    path->cubicTo(x1, y1, x2, y2, currentX, currentY);
    if (end == endAngle) {
      break;
    }
    start = end;
    end = start + static_cast<float>(M_PI_2);
    if (end > endAngle) {
      end = endAngle;
    }
  }
  path->close();
}

BulgeEffect::~BulgeEffect() {
  delete horizontalRadius;
  delete verticalRadius;
  delete bulgeCenter;
  delete bulgeHeight;
  delete taperRadius;
  delete pinning;
}

bool BulgeEffect::visibleAt(Frame layerFrame) const {
  auto horizontalRadiusValue = horizontalRadius->getValueAt(layerFrame);
  auto verticalRadiusValue = verticalRadius->getValueAt(layerFrame);
  auto height = bulgeHeight->getValueAt(layerFrame);
  return height != 0.0f && horizontalRadiusValue != 0.0f && verticalRadiusValue != 0.0f;
}

void AdjustLeft(const Point& bulgeCenterValue, float horizontalRadiusValue, float* left) {
  auto leftDiffer =
      bulgeCenterValue.x > *left ? (*left - (bulgeCenterValue.x - horizontalRadiusValue)) : 0;
  if (leftDiffer > 0) {
    *left -= leftDiffer;
  }
}

void AdjustRight(const Point& bulgeCenterValue, float horizontalRadiusValue, float* right) {
  auto rightDiffer =
      bulgeCenterValue.y < *right ? ((bulgeCenterValue.x + horizontalRadiusValue) - *right) : 0;
  if (rightDiffer > 0) {
    *right += rightDiffer;
  }
}

void AdjustTop(const Point& bulgeCenterValue, float verticalRadiusValue, float* top) {
  auto topDiffer =
      bulgeCenterValue.y > *top ? (*top - (bulgeCenterValue.y - verticalRadiusValue)) : 0;
  if (topDiffer > 0) {
    *top -= topDiffer;
  }
}

void AdjustBottom(const Point& bulgeCenterValue, float verticalRadiusValue, float* bottom) {
  auto bottomDiffer =
      (bulgeCenterValue.y < *bottom) ? ((bulgeCenterValue.y + verticalRadiusValue) - *bottom) : 0;
  if (bottomDiffer > 0) {
    *bottom += bottomDiffer;
  }
}

void BulgeEffect::transformBounds(Rect* contentBounds, const Point&, Frame layerFrame) const {
  auto horizontalRadiusValue = horizontalRadius->getValueAt(layerFrame);
  auto verticalRadiusValue = verticalRadius->getValueAt(layerFrame);
  auto bulgeCenterValue = bulgeCenter->getValueAt(layerFrame);
  auto bulgeHeightValue = bulgeHeight->getValueAt(layerFrame);
  auto pinningValue = pinning->getValueAt(layerFrame);
  // 凸出特效固定开启的话，不改变尺寸。
  if (pinningValue || bulgeHeightValue == 0) {
    return;
  }

  auto left = contentBounds->left;
  auto top = contentBounds->top;
  auto right = contentBounds->right;
  auto bottom = contentBounds->bottom;

  tgfx::Path ellipsePath = {};
  // radius 需要加 1，如果 2 者相交只有一个点时，intersectPath.isEmpty 为 true
  ConvertEllipseToPath(&ellipsePath, bulgeCenterValue.x, bulgeCenterValue.y,
                       horizontalRadiusValue + 1, verticalRadiusValue + 1);
  tgfx::Path boundsPath = {};
  boundsPath.addRect(left, top, right, bottom);
  auto intersectPath = ellipsePath;
  intersectPath.addPath(boundsPath, tgfx::PathOp::Intersect);
  // 凸出效果的椭圆与内容 bounds 没有交集，不改变尺寸
  if (intersectPath.isEmpty() && !intersectPath.isInverseFillType()) {
    return;
  }
  // 计算凸出范围
  AdjustLeft(bulgeCenterValue, horizontalRadiusValue, &left);
  AdjustRight(bulgeCenterValue, horizontalRadiusValue, &right);
  AdjustTop(bulgeCenterValue, verticalRadiusValue, &top);
  AdjustBottom(bulgeCenterValue, verticalRadiusValue, &bottom);
  contentBounds->setLTRB(left, top, right, bottom);
}

void BulgeEffect::excludeVaryingRanges(std::vector<pag::TimeRange>* timeRanges) const {
  Effect::excludeVaryingRanges(timeRanges);
  horizontalRadius->excludeVaryingRanges(timeRanges);
  verticalRadius->excludeVaryingRanges(timeRanges);
  bulgeCenter->excludeVaryingRanges(timeRanges);
  bulgeHeight->excludeVaryingRanges(timeRanges);
  taperRadius->excludeVaryingRanges(timeRanges);
  pinning->excludeVaryingRanges(timeRanges);
}

bool BulgeEffect::verify() const {
  if (!Effect::verify()) {
    VerifyFailed();
    return false;
  }
  VerifyAndReturn(horizontalRadius != nullptr && verticalRadius != nullptr &&
                  bulgeCenter != nullptr && bulgeHeight != nullptr && taperRadius != nullptr &&
                  pinning != nullptr);
}
}  // namespace pag
