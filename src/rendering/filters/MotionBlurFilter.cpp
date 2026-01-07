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
        in vec2 aPosition;
        in vec2 aTextureCoord;

        layout(std140) uniform VertexArgs {
            mat3 uPrevTransform;
            mat3 uTransform;
        };

        out vec2 vertexColor;
        out vec2 vCurrPosition;
        out vec2 vPrevPosition;
        void main() {
            gl_Position = vec4(aPosition.xy, 0, 1);
            vertexColor = aTextureCoord.xy;
            vCurrPosition = (uTransform * vec3(aTextureCoord, 1)).xy;
            vPrevPosition = (uPrevTransform * vec3(aTextureCoord, 1)).xy;
        }
    )";

static const char MOTIONBLUR_FRAGMENT_SHADER[] = R"(
        precision highp float;

        in vec2 vCurrPosition;
        in vec2 vPrevPosition;
        in vec2 vertexColor;

        uniform sampler2D sTexture;

        layout(std140) uniform FragmentArgs {
            float uVelCenter;
            float maxDistance;
        };

        out vec4 tgfx_FragColor;
        const int kSamplesPerFrame = 37;
        void main() {
            vec2 velocity = vCurrPosition.xy - vPrevPosition.xy;
            float distance = length(velocity);
            velocity *= (min(distance, maxDistance) / distance);

            vec2 target = vec2(0.0);
            vec2 edgeDetect = vec2(0.0);
            float edgeDetectValue = 0.0;
            float reachedEdgeCount = 0.0;

            vec4 result = texture(sTexture, vertexColor);
            for (int i = 1; i < kSamplesPerFrame; ++i) {
                target = vertexColor + velocity * (float(i) / float(kSamplesPerFrame - 1) - uVelCenter);

                edgeDetect = abs(step(vec2(1.0), target) - vec2(1.0)) * step(vec2(0.0), target);
                edgeDetectValue = edgeDetect.x * edgeDetect.y;

                reachedEdgeCount += (1.0 - edgeDetectValue);

                result += texture(sTexture, target) * edgeDetectValue;
            }
            tgfx_FragColor = (reachedEdgeCount < float(kSamplesPerFrame) - 1.0) ? result / float(kSamplesPerFrame) : vec4(0.0);
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
      ToTextureMatrix(previousMatrix, width, height, tgfx::ImageOrigin::TopLeft);
  auto currentGLMatrix = ToTextureMatrix(currentMatrix, width, height, tgfx::ImageOrigin::TopLeft);
  auto filter = std::make_shared<MotionBlurFilter>(previousGLMatrix, currentGLMatrix);
  return input->makeWithFilter(tgfx::ImageFilter::Runtime(filter), offset);
}

std::string MotionBlurFilter::onBuildVertexShader() const {
  return MOTIONBLUR_VERTEX_SHADER;
}

std::string MotionBlurFilter::onBuildFragmentShader() const {
  return MOTIONBLUR_FRAGMENT_SHADER;
}

std::vector<tgfx::Attribute> MotionBlurFilter::vertexAttributes() const {
  return {{"aPosition", tgfx::VertexFormat::Float2}, {"aTextureCoord", tgfx::VertexFormat::Float2}};
}

std::vector<tgfx::BindingEntry> MotionBlurFilter::uniformBlocks() const {
  return {{"VertexArgs", 0}, {"FragmentArgs", 1}};
}

void MotionBlurFilter::onUpdateUniforms(tgfx::RenderPass* renderPass, tgfx::GPU* gpu,
                                        const std::vector<std::shared_ptr<tgfx::Texture>>&,
                                        const tgfx::Point&) const {
  auto scaling = _previousMatrix[0] != _currentMatrix[0] || _previousMatrix[4] != _currentMatrix[4];

  // Vertex uniform buffer for matrices
  // mat3 in std140 is stored as 3 vec4s (each column padded to vec4)
  struct VertexUniforms {
    float prevTransform[12] = {};  // mat3 padded to 3 vec4s
    float transform[12] = {};      // mat3 padded to 3 vec4s
  };

  VertexUniforms vertexUniforms = {};
  // Copy mat3 with padding (each column padded to vec4)
  for (int i = 0; i < 3; i++) {
    vertexUniforms.prevTransform[i * 4 + 0] = _previousMatrix[i * 3 + 0];
    vertexUniforms.prevTransform[i * 4 + 1] = _previousMatrix[i * 3 + 1];
    vertexUniforms.prevTransform[i * 4 + 2] = _previousMatrix[i * 3 + 2];
    vertexUniforms.prevTransform[i * 4 + 3] = 0.0f;

    vertexUniforms.transform[i * 4 + 0] = _currentMatrix[i * 3 + 0];
    vertexUniforms.transform[i * 4 + 1] = _currentMatrix[i * 3 + 1];
    vertexUniforms.transform[i * 4 + 2] = _currentMatrix[i * 3 + 2];
    vertexUniforms.transform[i * 4 + 3] = 0.0f;
  }

  auto vertexBuffer = gpu->createBuffer(sizeof(VertexUniforms), tgfx::GPUBufferUsage::UNIFORM);
  if (vertexBuffer != nullptr) {
    auto* data = vertexBuffer->map();
    if (data != nullptr) {
      memcpy(data, &vertexUniforms, sizeof(VertexUniforms));
      vertexBuffer->unmap();
      renderPass->setUniformBuffer(0, vertexBuffer, 0, sizeof(VertexUniforms));
    }
  }

  // Fragment uniform buffer
  struct FragmentUniforms {
    float velCenter = 0.0f;
    float maxDistance = 0.0f;
  };

  FragmentUniforms fragmentUniforms = {};
  fragmentUniforms.velCenter = scaling ? 0.0f : 0.5f;
  fragmentUniforms.maxDistance = (MOTION_BLUR_SCALE_FACTOR - 1.0f) * 0.5f;

  auto fragmentBuffer = gpu->createBuffer(sizeof(FragmentUniforms), tgfx::GPUBufferUsage::UNIFORM);
  if (fragmentBuffer != nullptr) {
    auto* data = fragmentBuffer->map();
    if (data != nullptr) {
      memcpy(data, &fragmentUniforms, sizeof(FragmentUniforms));
      fragmentBuffer->unmap();
      renderPass->setUniformBuffer(1, fragmentBuffer, 0, sizeof(FragmentUniforms));
    }
  }
}

std::vector<float> MotionBlurFilter::computeVertices(const tgfx::Texture* source,
                                                     const tgfx::Texture* target,
                                                     const tgfx::Point& offset) const {
  auto inputRect = tgfx::Rect::MakeWH(source->width(), source->height());
  auto outputRect = filterBounds(inputRect);
  auto points = ComputeVerticesForMotionBlurAndBulge(inputRect, outputRect);
  std::vector<float> vertices = {};
  vertices.reserve(points.size() * 2);
  for (size_t i = 0; i < points.size();) {
    auto vertexPoint = ToVertexPoint(target, points[i++] + offset);
    vertices.push_back(vertexPoint.x);
    vertices.push_back(vertexPoint.y);
    auto texturePoint = ToTexturePoint(source, points[i++]);
    vertices.push_back(texturePoint.x);
    vertices.push_back(texturePoint.y);
  }
  return vertices;
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
