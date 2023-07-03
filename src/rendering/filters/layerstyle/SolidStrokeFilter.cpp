/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

namespace pag {
static const char SOLID_STROKE_FRAGMENT_SHADER[] = R"(
        #version 100
        precision highp float;
        uniform sampler2D uTextureInput;
        uniform sampler2D uOriginalTextureInput;
        uniform vec3 uColor;
        uniform float uAlpha;
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
    
            vec4 result = (alphaSum > 0.0) ? vec4(uColor * uAlpha, uAlpha) : vec4(0.0);
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
        uniform float uAlpha;
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
    
            vec4 result = (alphaSum > 0.0) ? vec4(uColor * uAlpha, uAlpha) : vec4(0.0);
            result = (uIsOutside == 1.0 && srcColor.a > threshold) ? srcColor : result;
            result = (uIsCenter == 1.0 && result.a < threshold) ? srcColor : result;
            result = (uIsInside == 1.0 && (result.a < threshold || srcColor.a < threshold)) ? srcColor : result;
    
            gl_FragColor = result;
        }
    )";

SolidStrokeFilter::SolidStrokeFilter(SolidStrokeMode mode)
    : styleMode(mode) {
}

std::string SolidStrokeFilter::onBuildFragmentShader() {
  if (styleMode == SolidStrokeMode::Thick) {
    return SOLID_STROKE_THICK_FRAGMENT_SHADER;
  }
  return SOLID_STROKE_FRAGMENT_SHADER;
}

void SolidStrokeFilter::onPrepareProgram(tgfx::Context* context, unsigned program) {
  auto gl = tgfx::GLFunctions::Get(context);
  originalTextureHandle = gl->getUniformLocation(program, "uOriginalTextureInput");
  isUseOriginalTextureHandle = gl->getUniformLocation(program, "uIsUseOriginalTexture");
  colorHandle = gl->getUniformLocation(program, "uColor");
  alphaHandle = gl->getUniformLocation(program, "uAlpha");
  sizeHandle = gl->getUniformLocation(program, "uSize");
  isOutsideHandle = gl->getUniformLocation(program, "uIsOutside");
  isCenterHandle = gl->getUniformLocation(program, "uIsCenter");
  isInsideHandle = gl->getUniformLocation(program, "uIsInside");
}

void SolidStrokeFilter::onUpdateOption(SolidStrokeOption newOption) {
  option = newOption;
}

void SolidStrokeFilter::onUpdateOriginalTexture(const tgfx::GLTextureInfo* sampler) {
  originalSampler = {sampler->id, sampler->target, sampler->format};
}

void SolidStrokeFilter::onUpdateParams(tgfx::Context* context, const tgfx::Rect& contentBounds,
                                       const tgfx::Point& filterScale) {
  auto color = ToTGFX(option.color);
  auto alpha = ToAlpha(option.opacity);

  auto spreadSizeX = option.spreadSize * filterScale.x;
  auto spreadSizeY = option.spreadSize * filterScale.y;
  spreadSizeX = std::min(spreadSizeX, STROKE_MAX_SPREAD_SIZE);
  spreadSizeY = std::min(spreadSizeY, STROKE_MAX_SPREAD_SIZE);
  auto gl = tgfx::GLFunctions::Get(context);
  if (originalSampler.id != 0) {
    ActiveGLTexture(context, 1, &originalSampler);
    gl->uniform1i(originalTextureHandle, 1);
    gl->uniform1f(isUseOriginalTextureHandle, 1.0);
  } else {
    gl->uniform1f(isUseOriginalTextureHandle, 0.0);
  }
  gl->uniform3f(colorHandle, color.red, color.green, color.blue);
  gl->uniform1f(alphaHandle, alpha);
  gl->uniform2f(sizeHandle, spreadSizeX / contentBounds.width(),
                spreadSizeY / contentBounds.height());
  gl->uniform1f(isOutsideHandle, option.position == StrokePosition::Outside);
  gl->uniform1f(isCenterHandle, option.position == StrokePosition::Center);
  gl->uniform1f(isInsideHandle, option.position == StrokePosition::Inside);
}

std::vector<tgfx::Point> SolidStrokeFilter::computeVertices(const tgfx::Rect&,
                                                                 const tgfx::Rect& outputBounds,
                                                                 const tgfx::Point& filterScale) {
  std::vector<tgfx::Point> vertices = {};
  tgfx::Point contentPoint[4] = {{outputBounds.left, outputBounds.bottom},
                                 {outputBounds.right, outputBounds.bottom},
                                 {outputBounds.left, outputBounds.top},
                                 {outputBounds.right, outputBounds.top}};

  auto deltaX = -option.spreadSize * filterScale.x;
  auto deltaY = -option.spreadSize * filterScale.y;

  tgfx::Point texturePoints[4] = {
      {deltaX, (outputBounds.height() + deltaY)},
      {(outputBounds.width() + deltaX), (outputBounds.height() + deltaY)},
      {deltaX, deltaY},
      {(outputBounds.width() + deltaX), deltaY}};
  for (int ii = 0; ii < 4; ii++) {
    vertices.push_back(contentPoint[ii]);
    vertices.push_back(texturePoints[ii]);
  }
  return vertices;
}
}  // namespace pag
