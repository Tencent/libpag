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

#include "CornerPinFilter.h"
#include "rendering/filters/utils/FilterHelper.h"

namespace pag {
static const char CORNER_PIN_VERTEX_SHADER[] = R"(
        #version 100
        attribute vec2 aPosition;
        attribute vec3 aTextureCoord;
        uniform mat3 uVertexMatrix;
        uniform mat3 uTextureMatrix;
        varying vec3 vertexColor;
        void main() {
            vec3 position = uVertexMatrix * vec3(aPosition, 1);
            gl_Position = vec4(position.xy, 0, 1);
            vertexColor = uTextureMatrix * aTextureCoord;
        }
    )";

static const char CORNER_PIN_FRAGMENT_SHADER[] = R"(
        #version 100
        precision mediump float;
        varying vec3 vertexColor;
        uniform sampler2D sTexture;
        void main() {
            gl_FragColor = texture2D(sTexture, vertexColor.xy / vertexColor.z);
        }
    )";

CornerPinFilter::CornerPinFilter(Effect* effect) : effect(effect) {
}

std::string CornerPinFilter::onBuildVertexShader() {
  return CORNER_PIN_VERTEX_SHADER;
}

std::string CornerPinFilter::onBuildFragmentShader() {
  return CORNER_PIN_FRAGMENT_SHADER;
}

static float calculateDistance(const tgfx::Point& intersection, const tgfx::Point& vertexPoint) {
  return std::sqrt(std::pow(fabs(intersection.x - vertexPoint.x), 2) +
                   std::pow(fabs(intersection.y - vertexPoint.y), 2));
}

static bool PointIsBetween(const tgfx::Point& point, const tgfx::Point& start,
                           const tgfx::Point& end) {
  auto minX = std::min(start.x, end.x);
  auto maxX = std::max(start.x, end.x);
  auto minY = std::min(start.y, end.y);
  auto maxY = std::max(start.y, end.y);
  return minX <= point.x && point.x <= maxX && minY <= point.y && point.y <= maxY;
}

void CornerPinFilter::calculateVertexQs() {
  // https://www.reedbeta.com/blog/quadrilateral-interpolation-part-1/
  // 计算2条对角线的交点：y1 = k1 * x1 + b1; y2 = k2 * x2 + b2
  auto* cornerPinEffect = reinterpret_cast<const CornerPinEffect*>(effect);
  auto lowerLeft = ToTGFX(cornerPinEffect->lowerLeft->getValueAt(layerFrame));
  auto upperRight = ToTGFX(cornerPinEffect->upperRight->getValueAt(layerFrame));
  auto lowerRight = ToTGFX(cornerPinEffect->lowerRight->getValueAt(layerFrame));
  auto upperLeft = ToTGFX(cornerPinEffect->upperLeft->getValueAt(layerFrame));
  auto ll2ur_k = (upperRight.y - lowerLeft.y) / (upperRight.x - lowerLeft.x);
  auto ul2lr_k = (lowerRight.y - upperLeft.y) / (lowerRight.x - upperLeft.x);
  auto ll2ur_b = lowerLeft.y - ll2ur_k * lowerLeft.x;
  auto ul2lr_b = upperLeft.y - ul2lr_k * upperLeft.x;
  tgfx::Point intersection = {0, 0};
  intersection.x = (ul2lr_b - ll2ur_b) / (ll2ur_k - ul2lr_k);
  intersection.y = ll2ur_k * intersection.x + ll2ur_b;
  // 计算对角线交点与4个顶点的距离
  auto lowerLeftDistance = calculateDistance(intersection, lowerLeft);
  auto lowerRightDistance = calculateDistance(intersection, lowerRight);
  auto upperRightDistance = calculateDistance(intersection, upperRight);
  auto upperLeftDistance = calculateDistance(intersection, upperLeft);
  // 计算4个顶点的uvq: uvq0 = float3(u0, v0, 1) * (d0 + d2) / d2
  if (PointIsBetween(intersection, lowerLeft, upperRight) &&
      PointIsBetween(intersection, upperLeft, lowerRight) && upperRightDistance != 0 &&
      upperLeftDistance != 0 && lowerRightDistance != 0 && lowerLeftDistance != 0) {
    vertexQs[0] = (lowerLeftDistance + upperRightDistance) / upperRightDistance;  // LowerLeft
    vertexQs[1] = (lowerRightDistance + upperLeftDistance) / upperLeftDistance;   // LowerRight
    vertexQs[2] = (upperLeftDistance + lowerRightDistance) / lowerRightDistance;  // UpperLeft
    vertexQs[3] = (upperRightDistance + lowerLeftDistance) / lowerLeftDistance;   // UpperRight
  } else {
    vertexQs[0] = 1.0f;
    vertexQs[1] = 1.0f;
    vertexQs[2] = 1.0f;
    vertexQs[3] = 1.0f;
  }
}

std::vector<tgfx::Point> CornerPinFilter::computeVertices(const tgfx::Rect& contentBounds,
                                                          const tgfx::Rect&, const tgfx::Point&) {
  std::vector<tgfx::Point> vertices = {};
  auto* cornerPinEffect = reinterpret_cast<const CornerPinEffect*>(effect);
  tgfx::Point contentPoint[4] = {ToTGFX(cornerPinEffect->lowerLeft->getValueAt(layerFrame)),
                                 ToTGFX(cornerPinEffect->lowerRight->getValueAt(layerFrame)),
                                 ToTGFX(cornerPinEffect->upperLeft->getValueAt(layerFrame)),
                                 ToTGFX(cornerPinEffect->upperRight->getValueAt(layerFrame))};
  tgfx::Point texturePoints[4] = {{0.0f, contentBounds.height()},
                                  {contentBounds.width(), contentBounds.height()},
                                  {0.0f, 0.0f},
                                  {contentBounds.width(), 0.0f}};

  for (int ii = 0; ii < 4; ii++) {
    vertices.push_back(contentPoint[ii]);
    vertices.push_back(texturePoints[ii]);
  }
  return vertices;
}

void CornerPinFilter::bindVertices(const tgfx::GLInterface* gl, const FilterSource* source,
                                   const FilterTarget* target,
                                   const std::vector<tgfx::Point>& points) {
  std::vector<float> vertices = {};
  calculateVertexQs();
  for (size_t i = 0, j = 0; i < points.size() && j < 4; j++) {
    auto vertexPoint = ToGLVertexPoint(target, source, contentBounds, points[i++]);
    vertices.push_back(vertexPoint.x);
    vertices.push_back(vertexPoint.y);
    auto texturePoint = ToGLTexturePoint(source, points[i++]);
    vertices.push_back(texturePoint.x * vertexQs[j]);
    vertices.push_back(texturePoint.y * vertexQs[j]);
    vertices.push_back(vertexQs[j]);
  }

  if (filterProgram->vertexArray > 0) {
    gl->bindVertexArray(filterProgram->vertexArray);
  }
  gl->bindBuffer(GL_ARRAY_BUFFER, filterProgram->vertexBuffer);
  gl->bufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STREAM_DRAW);
  gl->vertexAttribPointer(static_cast<unsigned>(positionHandle), 2, GL_FLOAT, GL_FALSE,
                          5 * sizeof(float), static_cast<void*>(0));
  gl->enableVertexAttribArray(static_cast<unsigned>(positionHandle));

  gl->vertexAttribPointer(textureCoordHandle, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          reinterpret_cast<void*>(2 * sizeof(float)));
  gl->enableVertexAttribArray(textureCoordHandle);
  gl->bindBuffer(GL_ARRAY_BUFFER, 0);
}
}  // namespace pag
