/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "SolidStrokeFilter.h"
#include "base/utils/TGFXCast.h"
#include "rendering/filters/utils/BlurTypes.h"
#include "rendering/filters/utils/FilterHelper.h"
#include "tgfx/core/ImageFilter.h"

namespace pag {
static const char SOLID_STROKE_FRAGMENT_SHADER[] = R"(
        precision highp float;
        uniform sampler2D sTexture;
        uniform sampler2D uOriginalTextureInput;

        layout(std140) uniform FilterUniforms {
            vec3 uColor;
            float uIsUseOriginalTexture;
            vec2 uSize;
            float uIsOutside;
            float uIsCenter;
            float uIsInside;
        };

        in vec2 vertexColor;
        out vec4 tgfx_FragColor;

        const float PI = 3.1415926535;
        float threshold = 0.3;

        float check(vec2 point) {
            vec2 result = step(point, vec2(1.0)) * step(vec2(0.0), point);
            return step(0.5, result.x * result.y);
        }

        void main()
        {
            vec2 point = vertexColor;
            vec4 inputColor = texture(sTexture, point);
            float alphaSum = inputColor.a * check(point);
            for (float i = 0.0; i <= 180.0; i += 11.25) {
                float arc = i * PI / 180.0;
                float measureX = cos(arc) * uSize.x;
                float measureY = sqrt(pow(uSize.x, 2.0) - pow(measureX, 2.0)) * uSize.y / uSize.x;
                point = vertexColor + vec2(measureX, measureY);
                alphaSum += texture(sTexture, point).a * check(point);
                point = vertexColor + vec2(measureX, -measureY);
                alphaSum += texture(sTexture, point).a * check(point);
            }

            vec4 srcColor = (uIsUseOriginalTexture == 1.0) ? texture(uOriginalTextureInput, vertexColor) : inputColor;

            vec4 result = (alphaSum > 0.0) ? vec4(uColor, 1.0) : vec4(0.0);
            result = (uIsOutside == 1.0 && srcColor.a > threshold) ? srcColor : result;
            result = (uIsCenter == 1.0 && result.a < threshold) ? srcColor : result;
            result = (uIsInside == 1.0 && (result.a < threshold || srcColor.a < threshold)) ? srcColor : result;
            tgfx_FragColor = result;
        }
    )";

static const char SOLID_STROKE_THICK_FRAGMENT_SHADER[] = R"(
        precision highp float;
        uniform sampler2D sTexture;
        uniform sampler2D uOriginalTextureInput;

        layout(std140) uniform FilterUniforms {
            vec3 uColor;
            float uIsUseOriginalTexture;
            vec2 uSize;
            float uIsOutside;
            float uIsCenter;
            float uIsInside;
        };

        in vec2 vertexColor;
        out vec4 tgfx_FragColor;

        const float PI = 3.1415926535;
        float threshold = 0.3;

        float check(vec2 point) {
            vec2 result = step(point, vec2(1.0)) * step(vec2(0.0), point);
            return step(0.5, result.x * result.y);
        }

        void main()
        {
            vec2 point = vertexColor;
            vec4 inputColor = texture(sTexture, point);
            float alphaSum = inputColor.a * check(point);
            for (float i = 0.0; i <= 180.0; i += 11.25) {
                float arc = i * PI / 180.0;
                float measureX = cos(arc) * uSize.x;
                float measureY = sqrt(pow(uSize.x, 2.0) - pow(measureX, 2.0)) * uSize.y / uSize.x;
                point = vertexColor + vec2(measureX, measureY);
                alphaSum += texture(sTexture, point).a * check(point);
                point = vertexColor + vec2(measureX, -measureY);
                alphaSum += texture(sTexture, point).a * check(point);
                point = vertexColor + vec2(measureX / 2.0, measureY / 2.0);
                alphaSum += texture(sTexture, point).a * check(point);
                point = vertexColor + vec2(measureX / 2.0, -measureY / 2.0);
                alphaSum += texture(sTexture, point).a * check(point);
            }

            vec4 srcColor = (uIsUseOriginalTexture == 1.0) ? texture(uOriginalTextureInput, vertexColor) : inputColor;

            vec4 result = (alphaSum > 0.0) ? vec4(uColor, 1.0) : vec4(0.0);
            result = (uIsOutside == 1.0 && srcColor.a > threshold) ? srcColor : result;
            result = (uIsCenter == 1.0 && result.a < threshold) ? srcColor : result;
            result = (uIsInside == 1.0 && (result.a < threshold || srcColor.a < threshold)) ? srcColor : result;

            tgfx_FragColor = result;
        }
    )";

struct SolidStrokeUniforms {
  float color[3] = {};
  float isUseOriginalTexture = 0.0f;
  float size[2] = {};
  float isOutside = 0.0f;
  float isCenter = 0.0f;
  float isInside = 0.0f;
};

std::shared_ptr<tgfx::ImageFilter> SolidStrokeFilter::CreateFilter(
    const SolidStrokeOption& option, SolidStrokeMode mode, std::shared_ptr<tgfx::Image> source) {
  if (!option.valid()) {
    return nullptr;
  }
  std::shared_ptr<tgfx::RuntimeEffect> effect;
  if (mode == SolidStrokeMode::Normal) {
    effect = SolidStrokeNormalFilter::Make(option, std::move(source));
  } else {
    effect = SolidStrokeThickFilter::Make(option, std::move(source));
  }
  return tgfx::ImageFilter::Runtime(effect);
}

std::shared_ptr<SolidStrokeNormalFilter> SolidStrokeNormalFilter::Make(
    SolidStrokeOption option, std::shared_ptr<tgfx::Image> originalImage) {
  if (originalImage) {
    return std::make_shared<SolidStrokeNormalFilter>(option, std::move(originalImage));
  } else {
    return std::make_shared<SolidStrokeNormalFilter>(option);
  }
}

std::shared_ptr<SolidStrokeThickFilter> SolidStrokeThickFilter::Make(
    SolidStrokeOption option, std::shared_ptr<tgfx::Image> originalImage) {
  if (originalImage) {
    return std::make_shared<SolidStrokeThickFilter>(option, std::move(originalImage));
  } else {
    return std::make_shared<SolidStrokeThickFilter>(option);
  }
}

std::string SolidStrokeNormalFilter::onBuildFragmentShader() const {
  return SOLID_STROKE_FRAGMENT_SHADER;
}

std::string SolidStrokeThickFilter::onBuildFragmentShader() const {
  return SOLID_STROKE_THICK_FRAGMENT_SHADER;
}

std::vector<tgfx::BindingEntry> SolidStrokeFilter::uniformBlocks() const {
  return {{"FilterUniforms", 0}};
}

std::vector<tgfx::BindingEntry> SolidStrokeFilter::textureSamplers() const {
  return {{"sTexture", 0}, {"uOriginalTextureInput", 1}};
}

void SolidStrokeFilter::onUpdateUniforms(
    tgfx::RenderPass* renderPass, tgfx::GPU* gpu,
    const std::vector<std::shared_ptr<tgfx::Texture>>& inputTextures, const tgfx::Point&) const {

  auto color = option.color;
  auto spreadSizeX = option.spreadSizeX;
  auto spreadSizeY = option.spreadSizeY;
  spreadSizeX = std::min(spreadSizeX, STROKE_MAX_SPREAD_SIZE);
  spreadSizeY = std::min(spreadSizeY, STROKE_MAX_SPREAD_SIZE);

  SolidStrokeUniforms uniforms = {};
  uniforms.color[0] = color.red;
  uniforms.color[1] = color.green;
  uniforms.color[2] = color.blue;
  uniforms.size[0] = spreadSizeX / inputTextures[0]->width();
  uniforms.size[1] = spreadSizeY / inputTextures[0]->height();
  uniforms.isOutside = option.position == StrokePosition::Outside ? 1.0f : 0.0f;
  uniforms.isCenter = option.position == StrokePosition::Center ? 1.0f : 0.0f;
  uniforms.isInside = option.position == StrokePosition::Inside ? 1.0f : 0.0f;

  if (hasOriginalImage && inputTextures.size() > 1) {
    tgfx::SamplerDescriptor samplerDesc(tgfx::AddressMode::ClampToEdge,
                                        tgfx::AddressMode::ClampToEdge, tgfx::FilterMode::Linear,
                                        tgfx::FilterMode::Linear, tgfx::MipmapMode::None);
    auto sampler = gpu->createSampler(samplerDesc);
    renderPass->setTexture(1, inputTextures[1], sampler);
    uniforms.isUseOriginalTexture = 1.0f;
  } else {
    uniforms.isUseOriginalTexture = 0.0f;
  }

  auto uniformBuffer =
      gpu->createBuffer(sizeof(SolidStrokeUniforms), tgfx::GPUBufferUsage::UNIFORM);
  if (uniformBuffer != nullptr) {
    auto* data = uniformBuffer->map();
    if (data != nullptr) {
      memcpy(data, &uniforms, sizeof(SolidStrokeUniforms));
      uniformBuffer->unmap();
      renderPass->setUniformBuffer(0, uniformBuffer, 0, sizeof(SolidStrokeUniforms));
    }
  }
}

void SolidStrokeFilter::collectVertices(const tgfx::Texture* source, const tgfx::Texture* target,
                                        const tgfx::Point&, float* vertices) const {
  auto outputBounds = tgfx::Rect::MakeWH(target->width(), target->height());
  tgfx::Point contentPoint[4] = {{outputBounds.left, outputBounds.bottom},
                                 {outputBounds.right, outputBounds.bottom},
                                 {outputBounds.left, outputBounds.top},
                                 {outputBounds.right, outputBounds.top}};

  auto deltaX = -option.spreadSizeX;
  auto deltaY = -option.spreadSizeY;

  tgfx::Point texturePoints[4] = {
      {deltaX, (outputBounds.height() + deltaY)},
      {(outputBounds.width() + deltaX), (outputBounds.height() + deltaY)},
      {deltaX, deltaY},
      {(outputBounds.width() + deltaX), deltaY}};

  size_t index = 0;
  for (size_t i = 0; i < 4; i++) {
    auto vertexPoint = ToVertexPoint(target, contentPoint[i]);
    vertices[index++] = vertexPoint.x;
    vertices[index++] = vertexPoint.y;
    auto texturePoint = ToTexturePoint(source, texturePoints[i]);
    vertices[index++] = texturePoint.x;
    vertices[index++] = texturePoint.y;
  }
}

tgfx::Rect SolidStrokeFilter::filterBounds(const tgfx::Rect& srcRect) const {
  auto desRect = srcRect.makeOutset(option.spreadSizeX, option.spreadSizeY);
  desRect.offset(option.offsetX, option.offsetY);
  return desRect;
}

}  // namespace pag
