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

#include "MotionBlurFilter.h"
#include "rendering/caches/LayerCache.h"
#include "rendering/filters/utils/FilterHelper.h"

namespace pag {
#define MOTION_BLUR_SCALE_FACTOR 1.2f

static const char MOTIONBLUR_VERTEX_SHADER[] = R"(
        #version 100
        attribute vec2 aPosition;
        attribute vec2 aTextureCoord;
        uniform mat3 uVertexMatrix;
        uniform mat3 uTextureMatrix;
		    uniform mat3 uPrevTransform;
        uniform mat3 uTransform;
        varying vec2 vertexColor;
        varying vec2 vCurrPosition;
        varying vec2 vPrevPosition;
        void main() {
            vec3 position = uVertexMatrix * vec3(aPosition, 1);
            gl_Position = vec4(position.xy, 0, 1);
            vec3 colorPosition = uTextureMatrix * vec3(aTextureCoord, 1);
            vertexColor = colorPosition.xy;
            vCurrPosition = (uTextureMatrix * uTransform * vec3(aTextureCoord, 1)).xy;
            vPrevPosition = (uTextureMatrix * uPrevTransform * vec3(aTextureCoord, 1)).xy;
        }
    )";

static const char MOTIONBLUR_FRAGMENT_SHADER[] = R"(
        #version 100
        precision highp float;

        varying vec2 vCurrPosition;
        varying vec2 vPrevPosition;
        varying vec2 vertexColor;

        uniform sampler2D uTextureInput;
        uniform float uVelCenter;
        uniform float maxDistance;
        const int kSamplesPerFrame = 37;
        void main() {
            vec2 velocity = vCurrPosition.xy - vPrevPosition.xy;
            float distance = length(velocity);
            velocity *= (min(distance, maxDistance) / distance);

            vec2 target = vec2(0.0);
            vec2 edgeDetect = vec2(0.0);
            float edgeDetectValue = 0.0;
            float reachedEdgeCount = 0.0;

            vec4 result = texture2D(uTextureInput, vertexColor);
            for (int i = 1; i < kSamplesPerFrame; ++i) {
                target = vertexColor + velocity * (float(i) / float(kSamplesPerFrame - 1) - uVelCenter);

                edgeDetect = abs(step(vec2(1.0), target) - vec2(1.0)) * step(vec2(0.0), target);
                edgeDetectValue = edgeDetect.x * edgeDetect.y;

                reachedEdgeCount += (1.0 - edgeDetectValue);

                result += texture2D(uTextureInput, target) * edgeDetectValue;
            }
            gl_FragColor = (reachedEdgeCount < float(kSamplesPerFrame) - 1.0) ? result / float(kSamplesPerFrame) : vec4(0.0);
        }
    )";

void MotionBlurFilter::TransformBounds(tgfx::Rect* bounds, const tgfx::Point&, Layer* layer,
                                       Frame layerFrame) {
  auto contentFrame = layerFrame - layer->startTime;
  auto layerCache = LayerCache::Get(layer);
  auto previousMatrix = layerCache->getTransform(contentFrame > 0 ? contentFrame - 1 : 0)->matrix;
  auto currentMatrix = layerCache->getTransform(contentFrame)->matrix;
  if (previousMatrix != currentMatrix) {
    auto width = bounds->width() * MOTION_BLUR_SCALE_FACTOR;
    auto height = bounds->height() * MOTION_BLUR_SCALE_FACTOR;
    auto x = bounds->x() + (bounds->width() - width) * 0.5f;
    auto y = bounds->y() + (bounds->height() - height) * 0.5f;
    bounds->setXYWH(x, y, width, height);
  }
}

MotionBlurFilter::MotionBlurFilter() {
}

std::string MotionBlurFilter::onBuildVertexShader() {
  return MOTIONBLUR_VERTEX_SHADER;
}

std::string MotionBlurFilter::onBuildFragmentShader() {
  return MOTIONBLUR_FRAGMENT_SHADER;
}

void MotionBlurFilter::onPrepareProgram(tgfx::Context* context, unsigned int program) {
  auto gl = tgfx::GLFunctions::Get(context);
  prevTransformHandle = gl->getUniformLocation(program, "uPrevTransform");
  transformHandle = gl->getUniformLocation(program, "uTransform");
  velCenterHandle = gl->getUniformLocation(program, "uVelCenter");
  maxDistanceHandle = gl->getUniformLocation(program, "maxDistance");
}

bool MotionBlurFilter::updateLayer(Layer* targetLayer, Frame layerFrame) {
  auto contentFrame = layerFrame - targetLayer->startTime;
  auto layerCache = LayerCache::Get(targetLayer);
  previousMatrix = layerCache->getTransform(contentFrame > 0 ? contentFrame - 1 : 0)->matrix;
  currentMatrix = layerCache->getTransform(contentFrame)->matrix;
  return previousMatrix != currentMatrix;
}

void MotionBlurFilter::onUpdateParams(tgfx::Context* context, const tgfx::Rect& contentBounds,
                                      const tgfx::Point&) {
  auto width = static_cast<int>(contentBounds.width());
  auto height = static_cast<int>(contentBounds.height());
  auto origin = tgfx::ImageOrigin::TopLeft;

  previousMatrix.preTranslate(contentBounds.left, contentBounds.top);
  currentMatrix.preTranslate(contentBounds.left, contentBounds.top);
  std::array<float, 9> previousGLMatrix = ToGLTextureMatrix(previousMatrix, width, height, origin);
  std::array<float, 9> currentGLMatrix = ToGLTextureMatrix(currentMatrix, width, height, origin);

  auto scaling = (previousMatrix.getScaleX() != currentMatrix.getScaleX() ||
                  previousMatrix.getScaleY() != currentMatrix.getScaleY());
  auto gl = tgfx::GLFunctions::Get(context);
  gl->uniformMatrix3fv(prevTransformHandle, 1, GL_FALSE, previousGLMatrix.data());
  gl->uniformMatrix3fv(transformHandle, 1, GL_FALSE, currentGLMatrix.data());
  gl->uniform1f(velCenterHandle, scaling ? 0.0f : 0.5f);
  gl->uniform1f(maxDistanceHandle, (MOTION_BLUR_SCALE_FACTOR - 1.0) * 0.5f);
}

std::vector<tgfx::Point> MotionBlurFilter::computeVertices(const tgfx::Rect& inputBounds,
                                                           const tgfx::Rect& outputBounds,
                                                           const tgfx::Point&) {
  return ComputeVerticesForMotionBlurAndBulge(inputBounds, outputBounds);
}
}  // namespace pag
