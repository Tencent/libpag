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
#include "tgfx/core/ImageFilter.h"

namespace pag {
static const char SOLID_STROKE_FRAGMENT_SHADER[] = R"(
        #version 100
        precision highp float;
        uniform sampler2D uTextureInput;
        uniform sampler2D uOriginalTextureInput;
        uniform vec3 uColor;
        uniform vec2 uSize;
        uniform float uIsUseOriginalTexture;
        uniform float uIsOutside;
        uniform float uIsCenter;
        uniform float uIsInside;

        varying vec2 vertexColor;

        const float PI = 3.1415926535;
        float threshold = 0.3;

        float check(vec2 point) {
            vec2 result = step(point, vec2(1.0)) * step(vec2(0.0), point);
            return step(0.5, result.x * result.y);
        }

        void main()
        {
            vec2 point = vertexColor;
            vec4 inputColor = texture2D(uTextureInput, point);
            float alphaSum = inputColor.a * check(point);
            for (float i = 0.0; i <= 180.0; i += 11.25) {
                float arc = i * PI / 180.0;
                float measureX = cos(arc) * uSize.x;
                float measureY = sqrt(pow(uSize.x, 2.0) - pow(measureX, 2.0)) * uSize.y / uSize.x;
                point = vertexColor + vec2(measureX, measureY);
                alphaSum += texture2D(uTextureInput, point).a * check(point);
                point = vertexColor + vec2(measureX, -measureY);
                alphaSum += texture2D(uTextureInput, point).a * check(point);
            }

            vec4 srcColor = (uIsUseOriginalTexture == 1.0) ? texture2D(uOriginalTextureInput, vertexColor) : inputColor;

            vec4 result = (alphaSum > 0.0) ? vec4(uColor, 1.0) : vec4(0.0);
            result = (uIsOutside == 1.0 && srcColor.a > threshold) ? srcColor : result;
            result = (uIsCenter == 1.0 && result.a < threshold) ? srcColor : result;
            result = (uIsInside == 1.0 && (result.a < threshold || srcColor.a < threshold)) ? srcColor : result;
            gl_FragColor = result;
        }
    )";

static const char SOLID_STROKE_THICK_FRAGMENT_SHADER[] = R"(
        #version 100
        precision highp float;
        uniform sampler2D uTextureInput;
        uniform sampler2D uOriginalTextureInput;
        uniform vec3 uColor;
        uniform vec2 uSize;
        uniform float uIsUseOriginalTexture;
        uniform float uIsOutside;
        uniform float uIsCenter;
        uniform float uIsInside;

        varying vec2 vertexColor;

        const float PI = 3.1415926535;
        float threshold = 0.3;

        float check(vec2 point) {
            vec2 result = step(point, vec2(1.0)) * step(vec2(0.0), point);
            return step(0.5, result.x * result.y);
        }

        void main()
        {
            vec2 point = vertexColor;
            vec4 inputColor = texture2D(uTextureInput, point);
            float alphaSum = inputColor.a * check(point);
            for (float i = 0.0; i <= 180.0; i += 11.25) {
                float arc = i * PI / 180.0;
                float measureX = cos(arc) * uSize.x;
                float measureY = sqrt(pow(uSize.x, 2.0) - pow(measureX, 2.0)) * uSize.y / uSize.x;
                point = vertexColor + vec2(measureX, measureY);
                alphaSum += texture2D(uTextureInput, point).a * check(point);
                point = vertexColor + vec2(measureX, -measureY);
                alphaSum += texture2D(uTextureInput, point).a * check(point);
                point = vertexColor + vec2(measureX / 2.0, measureY / 2.0);
                alphaSum += texture2D(uTextureInput, point).a * check(point);
                point = vertexColor + vec2(measureX / 2.0, -measureY / 2.0);
                alphaSum += texture2D(uTextureInput, point).a * check(point);
            }

            vec4 srcColor = (uIsUseOriginalTexture == 1.0) ? texture2D(uOriginalTextureInput, vertexColor) : inputColor;

            vec4 result = (alphaSum > 0.0) ? vec4(uColor, 1.0) : vec4(0.0);
            result = (uIsOutside == 1.0 && srcColor.a > threshold) ? srcColor : result;
            result = (uIsCenter == 1.0 && result.a < threshold) ? srcColor : result;
            result = (uIsInside == 1.0 && (result.a < threshold || srcColor.a < threshold)) ? srcColor : result;

            gl_FragColor = result;
        }
    )";

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

std::unique_ptr<Uniforms> SolidStrokeFilter::onPrepareProgram(tgfx::Context* context,
                                                              unsigned program) const {
  return std::make_unique<SolidStrokeUniforms>(context, program);
}

void SolidStrokeFilter::onUpdateParams(tgfx::Context* context, const RuntimeProgram* program,
                                       const std::vector<tgfx::BackendTexture>& sources) const {

  auto color = option.color;
  auto spreadSizeX = option.spreadSizeX;
  auto spreadSizeY = option.spreadSizeY;
  spreadSizeX = std::min(spreadSizeX, STROKE_MAX_SPREAD_SIZE);
  spreadSizeY = std::min(spreadSizeY, STROKE_MAX_SPREAD_SIZE);
  auto uniforms = static_cast<SolidStrokeUniforms*>(program->uniforms.get());
  auto gl = tgfx::GLFunctions::Get(context);
  if (hasOriginalImage) {
    gl->uniform1i(uniforms->originalTextureHandle, 1);
    gl->uniform1f(uniforms->isUseOriginalTextureHandle, 1.0);
  } else {
    gl->uniform1f(uniforms->isUseOriginalTextureHandle, 0.0);
  }
  gl->uniform3f(uniforms->colorHandle, color.red, color.green, color.blue);
  gl->uniform2f(uniforms->sizeHandle, spreadSizeX / sources[0].width(),
                spreadSizeY / sources[0].height());
  gl->uniform1f(uniforms->isOutsideHandle, option.position == StrokePosition::Outside);
  gl->uniform1f(uniforms->isCenterHandle, option.position == StrokePosition::Center);
  gl->uniform1f(uniforms->isInsideHandle, option.position == StrokePosition::Inside);
}

std::vector<float> SolidStrokeFilter::computeVertices(
    const std::vector<tgfx::BackendTexture>& sources, const tgfx::BackendRenderTarget& target,
    const tgfx::Point&) const {
  auto outputBounds = tgfx::Rect::MakeWH(target.width(), target.height());
  std::vector<float> vertices = {};
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

  for (size_t i = 0; i < 4; i++) {
    auto vertexPoint = ToGLVertexPoint(target, contentPoint[i]);
    vertices.push_back(vertexPoint.x);
    vertices.push_back(vertexPoint.y);
    auto texturePoint = ToGLTexturePoint(&sources[0], texturePoints[i]);
    vertices.push_back(texturePoint.x);
    vertices.push_back(texturePoint.y);
  }
  return vertices;
}

tgfx::Rect SolidStrokeFilter::filterBounds(const tgfx::Rect& srcRect) const {
  auto desRect = srcRect.makeOutset(option.spreadSizeX, option.spreadSizeY);
  desRect.offset(option.offsetX, option.offsetY);
  return desRect;
}

}  // namespace pag
