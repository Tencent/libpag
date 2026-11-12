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
//  license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/utils/PolystarPath.h"
#include <cmath>
#include "pagx/nodes/Polystar.h"

namespace pagx {

namespace {

void AddPolystarCurve(tgfx::Path* path, float centerX, float centerY, float angleDelta, float dx1,
                      float dy1, float roundness1, float dx2, float dy2, float roundness2) {
  float control1X = dx1 - dy1 * roundness1 * angleDelta + centerX;
  float control1Y = dy1 + dx1 * roundness1 * angleDelta + centerY;
  float control2X = dx2 + dy2 * roundness2 * angleDelta + centerX;
  float control2Y = dy2 - dx2 * roundness2 * angleDelta + centerY;
  path->cubicTo(control1X, control1Y, control2X, control2Y, dx2 + centerX, dy2 + centerY);
}

tgfx::Path StarToTGFXPath(const PolystarSample& sample) {
  tgfx::Path path = {};
  float points = sample.pointCount;
  if (points <= 0.0f) {
    return path;
  }
  float centerX = sample.center.x;
  float centerY = sample.center.y;
  float innerRadius = sample.innerRadius * sample.scale;
  float outerRadius = sample.outerRadius * sample.scale;
  float innerRoundness = sample.innerRoundness;
  float outerRoundness = sample.outerRoundness;
  float direction = sample.reversed ? -1.0f : 1.0f;
  float angleStep = static_cast<float>(M_PI) / points;
  float currentAngle = (sample.rotation - 90.0f) * static_cast<float>(M_PI) / 180.0f;
  int numPoints = static_cast<int>(std::ceil(points)) * 2;
  float decimalPart = points - std::floor(points);
  int decimalIndex = -2;
  if (decimalPart != 0.0f) {
    decimalIndex = direction > 0.0f ? 1 : numPoints - 3;
    currentAngle -= angleStep * decimalPart * 2.0f;
  }

  float firstDx = outerRadius * std::cos(currentAngle);
  float firstDy = outerRadius * std::sin(currentAngle);
  float lastDx = firstDx;
  float lastDy = firstDy;
  path.moveTo(lastDx + centerX, lastDy + centerY);

  bool outerFlag = false;
  for (int i = 0; i < numPoints; ++i) {
    float angleDelta = angleStep * direction;
    float dx;
    float dy;
    if (i == numPoints - 1) {
      dx = firstDx;
      dy = firstDy;
    } else {
      float radius = outerFlag ? outerRadius : innerRadius;
      if (i == decimalIndex || i == decimalIndex + 1) {
        radius = innerRadius + decimalPart * (radius - innerRadius);
        angleDelta *= decimalPart;
      }
      currentAngle += angleDelta;
      dx = radius * std::cos(currentAngle);
      dy = radius * std::sin(currentAngle);
    }
    if (innerRoundness != 0.0f || outerRoundness != 0.0f || sample.forceCurves) {
      float lastRoundness;
      float roundness;
      if (outerFlag) {
        lastRoundness = innerRoundness;
        roundness = outerRoundness;
      } else {
        lastRoundness = outerRoundness;
        roundness = innerRoundness;
      }
      AddPolystarCurve(&path, centerX, centerY, angleDelta * 0.5f, lastDx, lastDy, lastRoundness,
                       dx, dy, roundness);
      lastDx = dx;
      lastDy = dy;
    } else {
      path.lineTo(dx + centerX, dy + centerY);
    }
    outerFlag = !outerFlag;
  }
  path.close();
  return path;
}

tgfx::Path PolygonToTGFXPath(const PolystarSample& sample) {
  tgfx::Path path = {};
  float points = sample.pointCount;
  if (points <= 0.0f) {
    return path;
  }
  int numPoints = static_cast<int>(std::floor(points));
  if (numPoints < 3) {
    return path;
  }
  float centerX = sample.center.x;
  float centerY = sample.center.y;
  float radius = sample.outerRadius * sample.scale;
  float roundness = sample.outerRoundness;
  float direction = sample.reversed ? -1.0f : 1.0f;
  float angleStep = 2.0f * static_cast<float>(M_PI) / static_cast<float>(numPoints);
  float currentAngle = (sample.rotation - 90.0f) * static_cast<float>(M_PI) / 180.0f;

  float firstDx = radius * std::cos(currentAngle);
  float firstDy = radius * std::sin(currentAngle);
  float lastDx = firstDx;
  float lastDy = firstDy;
  path.moveTo(lastDx + centerX, lastDy + centerY);

  for (int i = 0; i < numPoints; ++i) {
    float angleDelta = angleStep * direction;
    float dx;
    float dy;
    if (i == numPoints - 1) {
      dx = firstDx;
      dy = firstDy;
    } else {
      currentAngle += angleDelta;
      dx = radius * std::cos(currentAngle);
      dy = radius * std::sin(currentAngle);
    }
    if (roundness != 0.0f || sample.forceCurves) {
      AddPolystarCurve(&path, centerX, centerY, angleDelta * 0.25f, lastDx, lastDy, roundness, dx,
                       dy, roundness);
      lastDx = dx;
      lastDy = dy;
    } else {
      path.lineTo(dx + centerX, dy + centerY);
    }
  }
  path.close();
  return path;
}

}  // namespace

PolystarSample SamplePolystar(const Polystar* star) {
  PolystarSample sample = {};
  if (star == nullptr) {
    return sample;
  }
  sample.polygon = star->type == PolystarType::Polygon;
  sample.pointCount = star->pointCount;
  sample.outerRadius = star->outerRadius;
  sample.innerRadius = star->innerRadius;
  sample.rotation = star->rotation;
  sample.outerRoundness = star->outerRoundness;
  sample.innerRoundness = star->innerRoundness;
  sample.reversed = star->reversed;
  sample.center = star->renderPosition();
  sample.scale = star->renderScale();
  return sample;
}

tgfx::Path PolystarToTGFXPath(const PolystarSample& sample) {
  return sample.polygon ? PolygonToTGFXPath(sample) : StarToTGFXPath(sample);
}

}  // namespace pagx
