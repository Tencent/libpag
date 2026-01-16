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
    precision highp float;
    in highp vec2 vertexColor;
    uniform sampler2D sTexture;

    layout(std140) uniform Args {
        highp vec2 uCenter;
        float uAmount;
    };

    out vec4 tgfx_FragColor;

    const float sampleCount = 16.0;

    void main() {
        vec2 oneStep = 1.0 / sampleCount * (vertexColor - uCenter) * uAmount;
        vec4 color = texture(sTexture, vertexColor);
        color += texture(sTexture, vertexColor + oneStep * 1.0);
        color += texture(sTexture, vertexColor + oneStep * 2.0);
        color += texture(sTexture, vertexColor + oneStep * 3.0);
        color += texture(sTexture, vertexColor + oneStep * 4.0);
        color += texture(sTexture, vertexColor + oneStep * 5.0);
        color += texture(sTexture, vertexColor + oneStep * 6.0);
        color += texture(sTexture, vertexColor + oneStep * 7.0);
        color += texture(sTexture, vertexColor + oneStep * 8.0);
        color += texture(sTexture, vertexColor + oneStep * 9.0);
        color += texture(sTexture, vertexColor + oneStep * 10.0);
        color += texture(sTexture, vertexColor + oneStep * 11.0);
        color += texture(sTexture, vertexColor + oneStep * 12.0);
        color += texture(sTexture, vertexColor + oneStep * 13.0);
        color += texture(sTexture, vertexColor + oneStep * 14.0);
        color += texture(sTexture, vertexColor + oneStep * 15.0);

        tgfx_FragColor = color / sampleCount;
    }
    )";

std::shared_ptr<tgfx::Image> RadialBlurFilter::Apply(std::shared_ptr<tgfx::Image> input,
                                                     RenderCache* cache, Effect* effect,
                                                     Frame layerFrame,
                                                     const tgfx::Rect& contentBounds,
                                                     tgfx::Point* offset) {
  auto* radialBlurEffect = reinterpret_cast<const RadialBlurEffect*>(effect);
  auto amount = radialBlurEffect->amount->getValueAt(layerFrame) * 0.00625;
  auto center = ToTGFX(radialBlurEffect->center->getValueAt(layerFrame));
  center.x = center.x / contentBounds.width();
  center.y = center.y / contentBounds.height();
  amount = amount < 0.25 ? amount : 0.25;
  auto filter = std::make_shared<RadialBlurFilter>(cache, amount, center);
  return input->makeWithFilter(tgfx::ImageFilter::Runtime(filter), offset);
}

std::string RadialBlurFilter::onBuildFragmentShader() const {
  return RADIAL_BLUR_FRAGMENT_SHADER;
}

std::vector<tgfx::BindingEntry> RadialBlurFilter::uniformBlocks() const {
  return {{"Args", 0}};
}

void RadialBlurFilter::onUpdateUniforms(tgfx::RenderPass* renderPass, tgfx::GPU* gpu,
                                        const std::vector<std::shared_ptr<tgfx::Texture>>&,
                                        const tgfx::Point&) const {
  struct Uniforms {
    float centerX = 0.0f;
    float centerY = 0.0f;
    float amount = 0.0f;
  };

  Uniforms uniforms = {};
  uniforms.amount = static_cast<float>(amount);
  uniforms.centerX = center.x;
  uniforms.centerY = center.y;

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

}  // namespace pag
