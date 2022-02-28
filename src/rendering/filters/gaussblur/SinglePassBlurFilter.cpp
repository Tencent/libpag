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

#include "SinglePassBlurFilter.h"
#include "rendering/filters/utils/BlurTypes.h"

namespace pag {
static const char BLUR_FRAGMENT_SHADER[] = R"(
    #version 100
    precision highp float;
    uniform sampler2D uTextureInput;
    uniform float uRadius;
    uniform vec2 uLevel;
    uniform float uRepeatEdge;

    uniform vec3 uColor;
    uniform float uColorValid;
    uniform float uAlpha;

    varying vec2 vertexColor;

    float maxEdge = -999.0;
    float edge = 0.005;

    float Curve (float value) {
        value = value * 2.0 - 1.0;
        return dot(vec3(-value, 2.0, 1.0),vec3(abs(value), value, 1.0)) * 0.5;
    }

    void main()
    {
        vec4 color = vec4(0.0);
        vec4 sum = vec4(0.0);

        float divisor = 0.0;
        float weight = 0.0;
        
        vec2 point = vec2(0.0);
        vec2 isVaild = vec2(0.0);

        float radiusMultiplier = 1.0 / uRadius;

        for (float i = 0.0; i < 1000.0; i++)
        {
            float value = i - uRadius;
            if (value > uRadius) {
                break;
            }
            point = vertexColor + value * uLevel;
            vec2 target = clamp(point, edge + maxEdge * (1.0 - uRepeatEdge), 1.0 - edge + (1.0 - maxEdge) * (1.0 - uRepeatEdge));
            color = texture2D(uTextureInput, target);
            isVaild = abs(step(vec2(1.0), point) - vec2(1.0)) * step(vec2(0.0), point);
            color *= step(1.0, isVaild.x * isVaild.y + uRepeatEdge);
            weight = Curve(1.0 - (abs(value) * radiusMultiplier));
            sum += color * weight;
            divisor += weight;
        }

        color = sum / divisor;
        gl_FragColor = vec4(mix(color.rgb, uColor * color.a, uColorValid), color.a) * uAlpha;
    }
    )";

SinglePassBlurFilter::SinglePassBlurFilter(BlurDirection direction) : direction(direction) {
}

std::string SinglePassBlurFilter::onBuildFragmentShader() {
  return BLUR_FRAGMENT_SHADER;
}

void SinglePassBlurFilter::onPrepareProgram(const tgfx::GLInterface* gl, unsigned int program) {
  radiusHandle = gl->functions->getUniformLocation(program, "uRadius");
  levelHandle = gl->functions->getUniformLocation(program, "uLevel");
  repeatEdgeHandle = gl->functions->getUniformLocation(program, "uRepeatEdge");
  colorHandle = gl->functions->getUniformLocation(program, "uColor");
  colorValidHandle = gl->functions->getUniformLocation(program, "uColorValid");
  alphaHandle = gl->functions->getUniformLocation(program, "uAlpha");
}

void SinglePassBlurFilter::updateParams(float blurrinessValue, float blurAlphaValue,
                                        bool repeatEdgeValue, BlurMode mode) {
  blurriness = blurrinessValue;
  alpha = blurAlphaValue;
  repeatEdge = repeatEdgeValue;
  switch (mode) {
    case BlurMode::Picture:
      this->maxRadius = BLUR_MODE_PIC_MAX_RADIUS;
      this->maxLevel = BLUR_MODE_PIC_MAX_LEVEL;
      break;
    case BlurMode::Shadow:
      this->maxRadius = BLUR_MODE_SHADOW_MAX_RADIUS;
      this->maxLevel = BLUR_MODE_SHADOW_MAX_LEVEL;
      break;
    default:
      break;
  }
}

void SinglePassBlurFilter::enableBlurColor(tgfx::Color blurColor) {
  isColorValid = true;
  color = blurColor;
}

void SinglePassBlurFilter::disableBlurColor() {
  isColorValid = false;
  color = tgfx::Color::Black();
}

void SinglePassBlurFilter::onUpdateParams(const tgfx::GLInterface* gl,
                                          const tgfx::Rect& contentBounds,
                                          const tgfx::Point& filterScale) {
  auto scale = direction == BlurDirection::Horizontal ? filterScale.x : filterScale.y;

  auto blurValue = std::min(blurriness * scale, BLUR_LIMIT_BLURRINESS);
  auto blurRadius = blurValue / BLUR_LIMIT_BLURRINESS * (maxRadius - 1.0) + 1.0;
  auto blurLevel = blurValue / BLUR_LIMIT_BLURRINESS * (maxLevel - 1.0) + 1.0;

  gl->functions->uniform1f(radiusHandle, blurRadius);
  gl->functions->uniform2f(levelHandle,
                           blurLevel / static_cast<float>(contentBounds.width()) *
                               (direction == BlurDirection::Horizontal),
                           blurLevel / static_cast<float>(contentBounds.height()) *
                               (direction == BlurDirection::Vertical));
  gl->functions->uniform1f(repeatEdgeHandle, repeatEdge);
  gl->functions->uniform3f(colorHandle, color.red, color.green, color.blue);
  gl->functions->uniform1f(colorValidHandle, isColorValid);
  gl->functions->uniform1f(alphaHandle, alpha);
}

std::vector<tgfx::Point> SinglePassBlurFilter::computeVertices(const tgfx::Rect& inputBounds,
                                                               const tgfx::Rect& outputBounds,
                                                               const tgfx::Point& filterScale) {
  if (repeatEdge) {
    return LayerFilter::computeVertices(inputBounds, outputBounds, filterScale);
  }
  std::vector<tgfx::Point> vertices = {};
  tgfx::Point contentPoint[4] = {{outputBounds.left, outputBounds.bottom},
                                 {outputBounds.right, outputBounds.bottom},
                                 {outputBounds.left, outputBounds.top},
                                 {outputBounds.right, outputBounds.top}};

  auto deltaX = direction == BlurDirection::Horizontal ? -blurriness * filterScale.x : 0;
  auto deltaY = direction == BlurDirection::Vertical ? -blurriness * filterScale.y : 0;

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
