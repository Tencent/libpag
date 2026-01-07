/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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
#include "tgfx/core/ImageFilter.h"

namespace pag {
static const char CORNER_PIN_VERTEX_SHADER[] = R"(
        in vec2 aPosition;
        in vec3 aTextureCoord;
        out vec3 vertexColor;
        void main() {
            gl_Position = vec4(aPosition.xy, 0, 1);
            vertexColor = aTextureCoord;
        }
    )";

static const char CORNER_PIN_FRAGMENT_SHADER[] = R"(
        precision mediump float;
        in vec3 vertexColor;
        uniform sampler2D sTexture;
        out vec4 tgfx_FragColor;
        void main() {
            tgfx_FragColor = texture(sTexture, vertexColor.xy / vertexColor.z);
        }
    )";

std::shared_ptr<tgfx::Image> CornerPinFilter::Apply(std::shared_ptr<tgfx::Image> input,
                                                    Effect* effect, Frame layerFrame,
                                                    const tgfx::Point& sourceScale,
                                                    tgfx::Point* offset) {
  auto cornerPinEffect = reinterpret_cast<const CornerPinEffect*>(effect);
  Point points[4] = {};
  points[0] = cornerPinEffect->lowerLeft->getValueAt(layerFrame);
  points[1] = cornerPinEffect->lowerRight->getValueAt(layerFrame);
  points[2] = cornerPinEffect->upperLeft->getValueAt(layerFrame);
  points[3] = cornerPinEffect->upperRight->getValueAt(layerFrame);
  for (auto& point : points) {
    point.x *= sourceScale.x;
    point.y *= sourceScale.y;
  }
  auto filter = std::shared_ptr<CornerPinFilter>(new CornerPinFilter(points));
  return input->makeWithFilter(tgfx::ImageFilter::Runtime(filter), offset);
}

std::string CornerPinFilter::onBuildVertexShader() const {
  return CORNER_PIN_VERTEX_SHADER;
}

std::string CornerPinFilter::onBuildFragmentShader() const {
  return CORNER_PIN_FRAGMENT_SHADER;
}

std::vector<tgfx::Attribute> CornerPinFilter::vertexAttributes() const {
  return {{"aPosition", tgfx::VertexFormat::Float2}, {"aTextureCoord", tgfx::VertexFormat::Float3}};
}

tgfx::Rect CornerPinFilter::filterBounds(const tgfx::Rect&) const {
  auto& lowerLeft = cornerPoints[0];
  auto& lowerRight = cornerPoints[1];
  auto& upperLeft = cornerPoints[2];
  auto& upperRight = cornerPoints[3];
  auto left = std::min(std::min(upperLeft.x, lowerLeft.x), std::min(upperRight.x, lowerRight.x));
  auto top = std::min(std::min(upperLeft.y, lowerLeft.y), std::min(upperRight.y, lowerRight.y));
  auto right = std::max(std::max(upperLeft.x, lowerLeft.x), std::max(upperRight.x, lowerRight.x));
  auto bottom = std::max(std::max(upperLeft.y, lowerLeft.y), std::max(upperRight.y, lowerRight.y));
  return tgfx::Rect::MakeLTRB(left, top, right, bottom);
}

static float calculateDistance(const tgfx::Point& intersection, const tgfx::Point& vertexPoint) {
  return std::sqrt(std::pow(fabsf(intersection.x - vertexPoint.x), 2.f) +
                   std::pow(fabsf(intersection.y - vertexPoint.y), 2.f));
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
  auto lowerLeft = cornerPoints[0];
  auto lowerRight = cornerPoints[1];
  auto upperLeft = cornerPoints[2];
  auto upperRight = cornerPoints[3];
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

std::vector<float> CornerPinFilter::computeVertices(const tgfx::Texture* source,
                                                    const tgfx::Texture* target,
                                                    const tgfx::Point& offset) const {
  tgfx::Point texturePoints[4] = {
      {0.0f, static_cast<float>(source->height())},
      {static_cast<float>(source->width()), static_cast<float>(source->height())},
      {0.0f, 0.0f},
      {static_cast<float>(source->width()), 0.0f}};

  std::vector<float> vertices = {};
  vertices.reserve(20);
  for (size_t i = 0; i < 4; i++) {
    auto vertexPoint = ToVertexPoint(target, cornerPoints[i] + offset);
    vertices.push_back(vertexPoint.x);
    vertices.push_back(vertexPoint.y);
    auto texturePoint = ToTexturePoint(source, texturePoints[i]);
    vertices.push_back(texturePoint.x * vertexQs[i]);
    vertices.push_back(texturePoint.y * vertexQs[i]);
    vertices.push_back(vertexQs[i]);
  }
  return vertices;
}

int CornerPinFilter::sampleCount() const {
  return 4;
}

}  // namespace pag
