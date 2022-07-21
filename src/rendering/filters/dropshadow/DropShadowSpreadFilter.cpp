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

#include "DropShadowSpreadFilter.h"
#include "base/utils/TGFXCast.h"
#include "rendering/filters/utils/BlurTypes.h"

namespace pag {
static const char DROPSHADOW_SPREAD_FRAGMENT_SHADER[] = R"(
        #version 100
        precision highp float;
        uniform sampler2D uTextureInput;
        uniform vec3 uColor;
        uniform float uAlpha;
        uniform vec2 uSize;

        varying vec2 vertexColor;

        const float PI = 3.1415926535;

        float check(vec2 point) {
            vec2 result = step(point, vec2(1.0)) * step(vec2(0.0), point);
            return step(0.5, result.x * result.y);
        }

        void main()
        {
            vec2 point = vertexColor;
            vec4 srcColor = texture2D(uTextureInput, point);
            float alphaSum = srcColor.a * check(point);
            for (float i = 0.0; i <= 180.0; i += 11.25) {
                float arc = i * PI / 180.0;
                float measureX = cos(arc) * uSize.x;
                float measureY = sqrt(pow(uSize.x, 2.0) - pow(measureX, 2.0)) * uSize.y / uSize.x;
                point = vertexColor + vec2(measureX, measureY);
                alphaSum += texture2D(uTextureInput, point).a * check(point);
                point = vertexColor + vec2(measureX, -measureY);
                alphaSum += texture2D(uTextureInput, point).a * check(point);
            }

            gl_FragColor = (alphaSum > 0.0) ? vec4(uColor * uAlpha, uAlpha) : vec4(0.0);
        }
    )";

static const char DROPSHADOW_SPREAD_THICK_FRAGMENT_SHADER[] = R"(
        #version 100
        precision highp float;
        uniform sampler2D uTextureInput;
        uniform vec3 uColor;
        uniform float uAlpha;
        uniform vec2 uSize;

        varying vec2 vertexColor;

        const float PI = 3.1415926535;

        float check(vec2 point) {
            vec2 result = step(point, vec2(1.0)) * step(vec2(0.0), point);
            return step(0.5, result.x * result.y);
        }

        void main()
        {
            vec2 point = vertexColor;
            vec4 srcColor = texture2D(uTextureInput, point);
            float alphaSum = srcColor.a * check(point);
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

            gl_FragColor = (alphaSum > 0.0) ? vec4(uColor * uAlpha, uAlpha) : vec4(0.0);
        }
    )";

DropShadowSpreadFilter::DropShadowSpreadFilter(DropShadowStyle* style, DropShadowStyleMode mode)
    : layerStyle(style), styleMode(mode) {
}

std::string DropShadowSpreadFilter::onBuildFragmentShader() {
  if (styleMode == DropShadowStyleMode::Thick) {
    return DROPSHADOW_SPREAD_THICK_FRAGMENT_SHADER;
  }
  return DROPSHADOW_SPREAD_FRAGMENT_SHADER;
}

void DropShadowSpreadFilter::onPrepareProgram(tgfx::Context* context, unsigned program) {
  auto gl = tgfx::GLFunctions::Get(context);
  spreadColorHandle = gl->getUniformLocation(program, "uColor");
  spreadAlphaHandle = gl->getUniformLocation(program, "uAlpha");
  spreadSizeHandle = gl->getUniformLocation(program, "uSize");
}

void DropShadowSpreadFilter::onUpdateParams(tgfx::Context* context, const tgfx::Rect& contentBounds,
                                            const tgfx::Point& filterScale) {
  auto color = ToTGFX(layerStyle->color->getValueAt(layerFrame));
  auto alpha = ToAlpha(layerStyle->opacity->getValueAt(layerFrame));
  auto spread = layerStyle->spread->getValueAt(layerFrame);
  auto size = layerStyle->size->getValueAt(layerFrame);
  spread *= (spread == 1.0) ? 1.0 : 0.8;

  auto spreadSizeX = size * spread * filterScale.x;
  auto spreadSizeY = size * spread * filterScale.y;
  spreadSizeX = std::min(spreadSizeX, DROPSHADOW_MAX_SPREAD_SIZE);
  spreadSizeY = std::min(spreadSizeY, DROPSHADOW_MAX_SPREAD_SIZE);
  auto gl = tgfx::GLFunctions::Get(context);
  gl->uniform3f(spreadColorHandle, color.red, color.green, color.blue);
  gl->uniform1f(spreadAlphaHandle, alpha);
  gl->uniform2f(spreadSizeHandle, spreadSizeX / contentBounds.width(),
                spreadSizeY / contentBounds.height());
}

std::vector<tgfx::Point> DropShadowSpreadFilter::computeVertices(const tgfx::Rect&,
                                                                 const tgfx::Rect& outputBounds,
                                                                 const tgfx::Point& filterScale) {
  std::vector<tgfx::Point> vertices = {};
  tgfx::Point contentPoint[4] = {{outputBounds.left, outputBounds.bottom},
                                 {outputBounds.right, outputBounds.bottom},
                                 {outputBounds.left, outputBounds.top},
                                 {outputBounds.right, outputBounds.top}};

  auto spread = layerStyle->spread->getValueAt(layerFrame);
  auto size = layerStyle->size->getValueAt(layerFrame);
  auto deltaX = -size * spread * filterScale.x;
  auto deltaY = -size * spread * filterScale.y;

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
