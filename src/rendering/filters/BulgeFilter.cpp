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

#include "BulgeFilter.h"
#include "tgfx/core/ImageFilter.h"
#include "utils/FilterHelper.h"

namespace pag {

static void ConvertEllipseToPath(tgfx::Path* path, float centerX, float centerY, float radiusX,
                                 float radiusY) {
  auto start = -static_cast<float>(M_PI_2);
  auto endAngle = static_cast<float>(M_PI + M_PI_2);
  auto end = 0.0f;
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

static void AdjustLeft(const Point& bulgeCenterValue, float horizontalRadiusValue, float* left) {
  auto leftDiffer =
      bulgeCenterValue.x > *left ? (*left - (bulgeCenterValue.x - horizontalRadiusValue)) : 0;
  if (leftDiffer > 0) {
    *left -= leftDiffer;
  }
}

static void AdjustRight(const Point& bulgeCenterValue, float horizontalRadiusValue, float* right) {
  auto rightDiffer =
      bulgeCenterValue.x < *right ? ((bulgeCenterValue.x + horizontalRadiusValue) - *right) : 0;
  if (rightDiffer > 0) {
    *right += rightDiffer;
  }
}

static void AdjustTop(const Point& bulgeCenterValue, float verticalRadiusValue, float* top) {
  auto topDiffer =
      bulgeCenterValue.y > *top ? (*top - (bulgeCenterValue.y - verticalRadiusValue)) : 0;
  if (topDiffer > 0) {
    *top -= topDiffer;
  }
}

static void AdjustBottom(const Point& bulgeCenterValue, float verticalRadiusValue, float* bottom) {
  auto bottomDiffer =
      (bulgeCenterValue.y < *bottom) ? ((bulgeCenterValue.y + verticalRadiusValue) - *bottom) : 0;
  if (bottomDiffer > 0) {
    *bottom += bottomDiffer;
  }
}

static const char BULGE_VERTEX_SHADER[] = R"(
        in vec2 aPosition;
        in vec2 aTextureCoord;
        out vec2 vertexColor;
        void main() {
            gl_Position = vec4(aPosition.xy, 0, 1);
            vertexColor = aTextureCoord.xy;
        }
    )";

static const char BULGE_FRAGMENT_SHADER[] = R"(
    precision highp float;
    in highp vec2 vertexColor;
    uniform sampler2D sTexture;

    layout(std140) uniform Args {
        float uHorizontalRadius;
        float uVerticalRadius;
        vec2 uBulgeCenter;
        float uBulgeHeight;
        int uPinning;
    };

    out vec4 tgfx_FragColor;

    float PI = 3.1415926535;
    float edge = 0.005;

    void main() {
        vec2 target = vertexColor;
        vec2 point = target - uBulgeCenter;

        float distance = pow(point.x, 2.0) / pow(uHorizontalRadius, 2.0) + pow(point.y, 2.0) / pow(uVerticalRadius, 2.0);

        float len = length(point);
        float radius = atan(point.y, point.x);
        float weight = (1.0 - distance) * atan(len, sqrt(1.0 - sqrt(len))) / PI;
        vec2 newPoint = point - uBulgeHeight * weight * vec2(cos(radius), sin(radius));

        // TODO: 在swiftshader上，此处使用mix来减少分支语句会引起画面异常，待排查
        if (distance <= 1.0) {
            target = newPoint + uBulgeCenter;
        }

        if(uPinning != 0) {
            target.x = clamp(target.x, edge, 1.0 - edge);
            target.y = clamp(target.y, edge, 1.0 - edge);
        }

        vec2 edgeDetect = abs(step(vec2(1.0), target) - vec2(1.0)) * step(vec2(0.0), target);

        tgfx_FragColor = texture(sTexture, target) * edgeDetect.x * edgeDetect.y;
    }
    )";

std::shared_ptr<tgfx::Image> BulgeFilter::Apply(std::shared_ptr<tgfx::Image> input, Effect* effect,
                                                Frame layerFrame, const tgfx::Rect& contentBounds,
                                                tgfx::Point* offset) {
  auto* bulgeEffect = reinterpret_cast<const BulgeEffect*>(effect);
  auto horizontalRadius =
      bulgeEffect->horizontalRadius->getValueAt(layerFrame) / contentBounds.width();
  auto verticalRadius =
      bulgeEffect->verticalRadius->getValueAt(layerFrame) / contentBounds.height();
  auto bulgeCenter = bulgeEffect->bulgeCenter->getValueAt(layerFrame);
  bulgeCenter.x = (bulgeCenter.x - contentBounds.x()) / contentBounds.width();
  bulgeCenter.y = (bulgeCenter.y - contentBounds.y()) / contentBounds.height();
  auto bulgeHeight = bulgeEffect->bulgeHeight->getValueAt(layerFrame);
  auto pinning = bulgeEffect->pinning->getValueAt(layerFrame);
  auto filter = std::make_shared<BulgeFilter>(horizontalRadius, verticalRadius, bulgeCenter,
                                              bulgeHeight, pinning);
  return input->makeWithFilter(tgfx::ImageFilter::Runtime(filter), offset);
}

BulgeFilter::BulgeFilter(float horizontalRadius, float verticalRadius, const Point& bulgeCenter,
                         float bulgeHeight, float pinning)
    : horizontalRadius(horizontalRadius), verticalRadius(verticalRadius), bulgeCenter(bulgeCenter),
      bulgeHeight(bulgeHeight), pinning(pinning) {
}

std::string BulgeFilter::onBuildVertexShader() const {
  return BULGE_VERTEX_SHADER;
}

std::string BulgeFilter::onBuildFragmentShader() const {
  return BULGE_FRAGMENT_SHADER;
}

std::vector<tgfx::BindingEntry> BulgeFilter::uniformBlocks() const {
  return {{"Args", 0}};
}

void BulgeFilter::onUpdateUniforms(tgfx::RenderPass* renderPass, tgfx::GPU* gpu,
                                   const std::vector<std::shared_ptr<tgfx::Texture>>&,
                                   const tgfx::Point&) const {
  struct Uniforms {
    float horizontalRadius = 0.0f;
    float verticalRadius = 0.0f;
    float bulgeCenterX = 0.0f;
    float bulgeCenterY = 0.0f;
    float bulgeHeight = 0.0f;
    int pinning = 0;
  };

  Uniforms uniforms = {};
  uniforms.horizontalRadius = horizontalRadius;
  uniforms.verticalRadius = verticalRadius;
  uniforms.bulgeCenterX = bulgeCenter.x;
  uniforms.bulgeCenterY = bulgeCenter.y;
  uniforms.bulgeHeight = bulgeHeight;
  uniforms.pinning = static_cast<int>(pinning);

  auto uniformBuffer = gpu->createBuffer(sizeof(Uniforms), tgfx::GPUBufferUsage::UNIFORM);
  if (uniformBuffer != nullptr) {
    auto* data = uniformBuffer->map();
    if (data != nullptr) {
      memcpy(data, &uniforms, sizeof(Uniforms));
      uniformBuffer->unmap();
      renderPass->setUniformBuffer(0, uniformBuffer, 0, sizeof(Uniforms));
    }
  }
}

void BulgeFilter::collectVertices(const tgfx::Texture* source, const tgfx::Texture* target,
                                  const tgfx::Point& offset, float* vertices) const {
  auto inputRect = tgfx::Rect::MakeWH(source->width(), source->height());
  auto outputRect = filterBounds(inputRect);
  auto points = ComputeVerticesForMotionBlurAndBulge(inputRect, outputRect);
  size_t index = 0;
  for (size_t i = 0; i < points.size();) {
    auto vertexPoint = ToVertexPoint(target, points[i++] + offset);
    vertices[index++] = vertexPoint.x;
    vertices[index++] = vertexPoint.y;
    auto texturePoint = ToTexturePoint(source, points[i++]);
    vertices[index++] = texturePoint.x;
    vertices[index++] = texturePoint.y;
  }
}

tgfx::Rect BulgeFilter::filterBounds(const tgfx::Rect& srcRect) const {
  // 凸出特效固定开启的话，不改变尺寸。
  if (pinning || bulgeHeight == 0) {
    return srcRect;
  }

  auto left = srcRect.left;
  auto top = srcRect.top;
  auto right = srcRect.right;
  auto bottom = srcRect.bottom;

  tgfx::Path ellipsePath = {};
  auto originBulgeCenterX = bulgeCenter.x * srcRect.width() + srcRect.left;
  auto originBulgeCenterY = bulgeCenter.y * srcRect.height() + srcRect.top;
  auto originHorizontalRadius = horizontalRadius * srcRect.width();
  auto originVerticalRadius = verticalRadius * srcRect.height();
  // radius 需要加 1，如果 2 者相交只有一个点时，intersectPath.isEmpty 为 true
  ConvertEllipseToPath(&ellipsePath, originBulgeCenterX, originBulgeCenterY,
                       originHorizontalRadius + 1, originVerticalRadius + 1);
  tgfx::Path boundsPath = {};
  boundsPath.addRect(left, top, right, bottom);
  auto intersectPath = ellipsePath;
  intersectPath.addPath(boundsPath, tgfx::PathOp::Intersect);
  // 凸出效果的椭圆与内容 bounds 没有交集，不改变尺寸
  if (intersectPath.isEmpty() && !intersectPath.isInverseFillType()) {
    return srcRect;
  }
  // 计算凸出范围
  auto originBulgeCenter = Point::Make(originBulgeCenterX, originBulgeCenterY);
  AdjustLeft(originBulgeCenter, originHorizontalRadius, &left);
  AdjustRight(originBulgeCenter, originHorizontalRadius, &right);
  AdjustTop(originBulgeCenter, originVerticalRadius, &top);
  AdjustBottom(originBulgeCenter, originVerticalRadius, &bottom);
  return tgfx::Rect::MakeLTRB(left, top, right, bottom);
}

}  // namespace pag
