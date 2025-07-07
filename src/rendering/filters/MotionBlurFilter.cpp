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

#include "MotionBlurFilter.h"
#include "rendering/caches/LayerCache.h"
#include "rendering/filters/utils/FilterHelper.h"

namespace pag {
#define MOTION_BLUR_SCALE_FACTOR 1.2f

static const char MOTIONBLUR_VERTEX_SHADER[] = R"(
        #version 100
        attribute vec2 aPosition;
        attribute vec2 aTextureCoord;
		    uniform mat3 uPrevTransform;
        uniform mat3 uTransform;
        varying vec2 vertexColor;
        varying vec2 vCurrPosition;
        varying vec2 vPrevPosition;
        void main() {
            gl_Position = vec4(aPosition.xy, 0, 1);
            vertexColor = aTextureCoord.xy;
            vCurrPosition = (uTransform * vec3(aTextureCoord, 1)).xy;
            vPrevPosition = (uPrevTransform * vec3(aTextureCoord, 1)).xy;
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

void MotionBlurFilter::TransformBounds(tgfx::Rect* contentBounds, Layer* layer, Frame layerFrame) {
  auto contentFrame = layerFrame - layer->startTime;
  auto layerCache = LayerCache::Get(layer);
  auto previousMatrix = layerCache->getTransform(contentFrame > 0 ? contentFrame - 1 : 0)->matrix;
  auto currentMatrix = layerCache->getTransform(contentFrame)->matrix;
  if (previousMatrix != currentMatrix) {
    auto width = contentBounds->width() * MOTION_BLUR_SCALE_FACTOR;
    auto height = contentBounds->height() * MOTION_BLUR_SCALE_FACTOR;
    auto x = contentBounds->x() + (contentBounds->width() - width) * 0.5f;
    auto y = contentBounds->y() + (contentBounds->height() - height) * 0.5f;
    contentBounds->setXYWH(x, y, width, height);
  }
}

bool MotionBlurFilter::ShouldSkipFilter(Layer* layer, Frame layerFrame) {
  auto layerCache = LayerCache::Get(layer);
  auto contentFrame = layerFrame - layer->startTime;
  auto previousMatrix = layerCache->getTransform(contentFrame > 0 ? contentFrame - 1 : 0)->matrix;
  auto currentMatrix = layerCache->getTransform(contentFrame)->matrix;
  return previousMatrix == currentMatrix;
}

std::shared_ptr<tgfx::Image> MotionBlurFilter::Apply(std::shared_ptr<tgfx::Image> input,
                                                     Layer* layer, Frame layerFrame,
                                                     const tgfx::Rect& contentBounds,
                                                     tgfx::Point* offset) {
  auto contentFrame = layerFrame - layer->startTime;
  auto layerCache = LayerCache::Get(layer);
  auto previousMatrix = layerCache->getTransform(contentFrame > 0 ? contentFrame - 1 : 0)->matrix;
  auto currentMatrix = layerCache->getTransform(contentFrame)->matrix;
  previousMatrix.preTranslate(contentBounds.left, contentBounds.top);
  currentMatrix.preTranslate(contentBounds.left, contentBounds.top);
  auto width = static_cast<int>(contentBounds.width());
  auto height = static_cast<int>(contentBounds.height());
  auto previousGLMatrix =
      ToGLTextureMatrix(previousMatrix, width, height, tgfx::ImageOrigin::TopLeft);
  auto currentGLMatrix =
      ToGLTextureMatrix(currentMatrix, width, height, tgfx::ImageOrigin::TopLeft);
  auto filter = std::make_shared<MotionBlurFilter>(previousGLMatrix, currentGLMatrix);
  return input->makeWithFilter(tgfx::ImageFilter::Runtime(filter), offset);
}

std::string MotionBlurFilter::onBuildVertexShader() const {
  return MOTIONBLUR_VERTEX_SHADER;
}

std::string MotionBlurFilter::onBuildFragmentShader() const {
  return MOTIONBLUR_FRAGMENT_SHADER;
}

std::unique_ptr<Uniforms> MotionBlurFilter::onPrepareProgram(tgfx::Context* context,
                                                             unsigned program) const {
  return std::make_unique<MotionBlurUniforms>(context, program);
}

void MotionBlurFilter::onUpdateParams(tgfx::Context* context, const RuntimeProgram* program,
                                      const std::vector<tgfx::BackendTexture>&) const {
  auto scaling = _previousMatrix[0] != _currentMatrix[0] || _previousMatrix[4] != _currentMatrix[4];
  auto gl = tgfx::GLFunctions::Get(context);
  auto uniform = static_cast<MotionBlurUniforms*>(program->uniforms.get());
  gl->uniformMatrix3fv(uniform->prevTransformHandle, 1, GL_FALSE, _previousMatrix.data());
  gl->uniformMatrix3fv(uniform->transformHandle, 1, GL_FALSE, _currentMatrix.data());
  gl->uniform1f(uniform->velCenterHandle, scaling ? 0.0f : 0.5f);
  gl->uniform1f(uniform->maxDistanceHandle, (MOTION_BLUR_SCALE_FACTOR - 1.0) * 0.5f);
}

std::vector<float> MotionBlurFilter::computeVertices(
    const std::vector<tgfx::BackendTexture>& sources, const tgfx::BackendRenderTarget& target,
    const tgfx::Point& offset) const {
  auto inputRect = tgfx::Rect::MakeWH(sources[0].width(), sources[0].height());
  auto outputRect = filterBounds(inputRect);
  auto vertices = ComputeVerticesForMotionBlurAndBulge(inputRect, outputRect);
  std::vector<float> result;
  result.reserve(vertices.size() * 4);
  for (size_t i = 0; i < vertices.size();) {
    auto vertexPoint = ToGLVertexPoint(target, vertices[i++] + offset);
    result.push_back(vertexPoint.x);
    result.push_back(vertexPoint.y);
    auto texturePoint = ToGLTexturePoint(&sources[0], vertices[i++]);
    result.push_back(texturePoint.x);
    result.push_back(texturePoint.y);
  }
  return result;
}

tgfx::Rect MotionBlurFilter::filterBounds(const tgfx::Rect& srcRect) const {
  auto result = srcRect;
  if (_previousMatrix != _currentMatrix) {
    auto width = srcRect.width() * MOTION_BLUR_SCALE_FACTOR;
    auto height = srcRect.height() * MOTION_BLUR_SCALE_FACTOR;
    auto x = srcRect.x() + (srcRect.width() - width) * 0.5f;
    auto y = srcRect.y() + (srcRect.height() - height) * 0.5f;
    result.setXYWH(x, y, width, height);
  }
  return result;
}

}  // namespace pag
