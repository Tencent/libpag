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
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/utils/TextPathUtils.h"
#include <cmath>
#include "pagx/nodes/TextPath.h"
#include "renderer/ToTGFX.h"
#include "tgfx/core/PathMeasure.h"

namespace pagx {

namespace {

constexpr float PI = 3.14159265358979323846f;

bool GetPosTanExtended(tgfx::PathMeasure* pathMeasure, float distance, float pathLength,
                       tgfx::Point* position, tgfx::Point* tangent) {
  if (distance >= 0.0f && distance <= pathLength) {
    return pathMeasure->getPosTan(distance, position, tangent);
  }
  if (distance < 0.0f) {
    if (!pathMeasure->getPosTan(0.0f, position, tangent)) {
      return false;
    }
    position->x += tangent->x * distance;
    position->y += tangent->y * distance;
  } else {
    if (!pathMeasure->getPosTan(pathLength, position, tangent)) {
      return false;
    }
    auto overflow = distance - pathLength;
    position->x += tangent->x * overflow;
    position->y += tangent->y * overflow;
  }
  return true;
}

float AdjustPathOffset(float pathOffset, float pathLength, bool reversed, bool isClosed) {
  if (reversed) {
    pathOffset = pathLength - pathOffset;
  }
  if (isClosed && (pathOffset < 0.0f || pathOffset > pathLength)) {
    pathOffset = std::fmod(std::fmod(pathOffset, pathLength) + pathLength, pathLength);
  }
  return pathOffset;
}

void PlaceGlyphOnCurve(tgfx::Matrix* glyphMatrix, const tgfx::Point& anchor,
                       const tgfx::Point& curvePoint, const tgfx::Point& curveTangent,
                       bool perpendicular, bool reversed, float baselineCos, float baselineSin) {
  auto rotationScale = *glyphMatrix;
  rotationScale.setTranslateX(0);
  rotationScale.setTranslateY(0);
  auto localAnchor = rotationScale.mapXY(anchor.x, anchor.y);
  if (perpendicular) {
    float curveC = reversed ? -curveTangent.x : curveTangent.x;
    float curveS = reversed ? -curveTangent.y : curveTangent.y;
    float finalCos = curveC * baselineCos + curveS * baselineSin;
    float finalSin = curveS * baselineCos - curveC * baselineSin;
    tgfx::Matrix curveRotation = tgfx::Matrix::I();
    curveRotation.setSinCos(finalSin, finalCos);
    auto rotatedAnchor = curveRotation.mapXY(localAnchor.x, localAnchor.y);
    *glyphMatrix = rotationScale;
    glyphMatrix->postConcat(curveRotation);
    glyphMatrix->postTranslate(curvePoint.x - rotatedAnchor.x, curvePoint.y - rotatedAnchor.y);
  } else {
    *glyphMatrix = rotationScale;
    glyphMatrix->postTranslate(curvePoint.x - localAnchor.x, curvePoint.y - localAnchor.y);
  }
}

}  // namespace

bool ApplyTextPathToGlyphs(const TextPath& textPath, const std::vector<TextPathGlyphInput>& glyphs,
                           std::vector<Matrix>* outTransforms) {
  if (outTransforms == nullptr) {
    return false;
  }
  outTransforms->clear();
  if (textPath.path == nullptr || glyphs.empty()) {
    return false;
  }
  auto path = ToTGFX(*textPath.path);
  if (path.isEmpty()) {
    return false;
  }
  float scale = textPath.renderScale();
  if (!std::isfinite(scale) || scale <= 0.0f) {
    return false;
  }
  if (scale != 1.0f) {
    path.transform(tgfx::Matrix::MakeScale(scale, scale));
  }
  auto position = textPath.renderPosition();
  if (position.x != 0.0f || position.y != 0.0f) {
    path.transform(tgfx::Matrix::MakeTrans(position.x, position.y));
  }
  auto pathMeasure = tgfx::PathMeasure::MakeFrom(path);
  if (pathMeasure == nullptr) {
    return false;
  }
  float pathLength = pathMeasure->getLength();
  if (!std::isfinite(pathLength) || pathLength <= 0.0f) {
    return false;
  }
  bool isClosed = pathMeasure->isClosed();
  std::vector<tgfx::Matrix> matrices(glyphs.size());
  for (size_t i = 0; i < glyphs.size(); i++) {
    matrices[i] = ToTGFX(glyphs[i].transform);
  }

  if (textPath.forceAlignment) {
    size_t glyphCount = glyphs.size();
    float totalAdvance = 0.0f;
    for (const auto& glyph : glyphs) {
      totalAdvance += glyph.advance;
    }
    auto availableLength = pathLength + textPath.lastMargin - textPath.firstMargin;
    float extraSpacingPerGap = 0.0f;
    if (glyphCount > 1 && totalAdvance > 0.0f) {
      extraSpacingPerGap = (availableLength - totalAdvance) / static_cast<float>(glyphCount - 1);
    }
    float accumulatedAdvance = 0.0f;
    for (size_t i = 0; i < glyphs.size(); i++) {
      float pathOffset = textPath.firstMargin + glyphs[i].advance * 0.5f + accumulatedAdvance +
                         static_cast<float>(i) * extraSpacingPerGap;
      pathOffset = AdjustPathOffset(pathOffset, pathLength, textPath.reversed, isClosed);
      tgfx::Point curvePoint = {};
      tgfx::Point tangent = {};
      if (GetPosTanExtended(pathMeasure.get(), pathOffset, pathLength, &curvePoint, &tangent)) {
        auto anchor = ToTGFX(glyphs[i].anchor);
        PlaceGlyphOnCurve(&matrices[i], anchor, curvePoint, tangent, textPath.perpendicular,
                          textPath.reversed, 1.0f, 0.0f);
      }
      accumulatedAdvance += glyphs[i].advance;
    }
  } else {
    float baselineRadians = textPath.baselineAngle * PI / 180.0f;
    float baselineCos = std::cos(baselineRadians);
    float baselineSin = std::sin(baselineRadians);
    auto baselineOrigin = ToTGFX(textPath.renderBaselineOrigin());
    for (size_t i = 0; i < glyphs.size(); i++) {
      auto transform = matrices[i];
      auto localPos = transform.mapXY(glyphs[i].anchor.x, glyphs[i].anchor.y);
      float dx = localPos.x - baselineOrigin.x;
      float dy = localPos.y - baselineOrigin.y;
      float tangentDistance = dx * baselineCos + dy * baselineSin;
      float normalOffset = dy * baselineCos - dx * baselineSin;
      float pathOffset = AdjustPathOffset(textPath.firstMargin + tangentDistance, pathLength,
                                          textPath.reversed, isClosed);
      tgfx::Point curvePoint = {};
      tgfx::Point tangent = {};
      if (!GetPosTanExtended(pathMeasure.get(), pathOffset, pathLength, &curvePoint, &tangent)) {
        continue;
      }
      tgfx::Point anchorNew = {curvePoint.x - tangent.y * normalOffset,
                               curvePoint.y + tangent.x * normalOffset};
      auto anchor = ToTGFX(glyphs[i].anchor);
      PlaceGlyphOnCurve(&matrices[i], anchor, anchorNew, tangent, textPath.perpendicular,
                        textPath.reversed, baselineCos, baselineSin);
    }
  }
  outTransforms->resize(matrices.size());
  for (size_t i = 0; i < matrices.size(); i++) {
    (*outTransforms)[i] = FromTGFX(matrices[i]);
  }
  return true;
}

}  // namespace pagx
