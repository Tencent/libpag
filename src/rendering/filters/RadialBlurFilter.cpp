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

#include "RadialBlurFilter.h"
#include "base/utils/TGFXCast.h"
#include "tgfx/core/ImageFilter.h"

namespace pag {

static const char RADIAL_BLUR_FRAGMENT_SHADER[] = R"(
    #version 100
    precision highp float;
    varying highp vec2 vertexColor;
    uniform sampler2D inputImageTexture;

    uniform float uAmount;
    uniform highp vec2 uCenter;

    const float sampleCount = 16.0;

    void main() {
        vec2 oneStep = 1.0 / sampleCount * (vertexColor - uCenter) * uAmount;
        vec4 color = texture2D(inputImageTexture, vertexColor);
        color += texture2D(inputImageTexture, vertexColor + oneStep * 1.0);
        color += texture2D(inputImageTexture, vertexColor + oneStep * 2.0);
        color += texture2D(inputImageTexture, vertexColor + oneStep * 3.0);
        color += texture2D(inputImageTexture, vertexColor + oneStep * 4.0);
        color += texture2D(inputImageTexture, vertexColor + oneStep * 5.0);
        color += texture2D(inputImageTexture, vertexColor + oneStep * 6.0);
        color += texture2D(inputImageTexture, vertexColor + oneStep * 7.0);
        color += texture2D(inputImageTexture, vertexColor + oneStep * 8.0);
        color += texture2D(inputImageTexture, vertexColor + oneStep * 9.0);
        color += texture2D(inputImageTexture, vertexColor + oneStep * 10.0);
        color += texture2D(inputImageTexture, vertexColor + oneStep * 11.0);
        color += texture2D(inputImageTexture, vertexColor + oneStep * 12.0);
        color += texture2D(inputImageTexture, vertexColor + oneStep * 13.0);
        color += texture2D(inputImageTexture, vertexColor + oneStep * 14.0);
        color += texture2D(inputImageTexture, vertexColor + oneStep * 15.0);

        gl_FragColor = color / sampleCount;
    }
    )";

std::shared_ptr<tgfx::Image> RadialBlurFilter::Apply(std::shared_ptr<tgfx::Image> input,
                                                     Effect* effect, Frame layerFrame,
                                                     const tgfx::Rect& contentBounds,
                                                     tgfx::Point* offset) {
  auto* radialBlurEffect = reinterpret_cast<const RadialBlurEffect*>(effect);
  auto amount = radialBlurEffect->amount->getValueAt(layerFrame) * 0.00625;
  auto center = ToTGFX(radialBlurEffect->center->getValueAt(layerFrame));
  center.x = center.x / contentBounds.width();
  center.y = center.y / contentBounds.height();
  amount = amount < 0.25 ? amount : 0.25;
  auto filter = std::make_shared<RadialBlurFilter>(amount, center);
  return input->makeWithFilter(tgfx::ImageFilter::Runtime(filter), offset);
}

std::string RadialBlurFilter::onBuildFragmentShader() const {
  return RADIAL_BLUR_FRAGMENT_SHADER;
}

std::unique_ptr<Uniforms> RadialBlurFilter::onPrepareProgram(tgfx::Context* context,
                                                             unsigned program) const {
  return std::make_unique<RadialBlurUniforms>(context, program);
}

void RadialBlurFilter::onUpdateParams(tgfx::Context* context, const RuntimeProgram* program,
                                      const std::vector<tgfx::BackendTexture>&) const {
  auto gl = tgfx::GLFunctions::Get(context);
  auto uniforms = static_cast<RadialBlurUniforms*>(program->uniforms.get());
  gl->uniform1f(uniforms->amountHandle, amount);
  gl->uniform2f(uniforms->centerHandle, center.x, center.y);
}

}  // namespace pag
