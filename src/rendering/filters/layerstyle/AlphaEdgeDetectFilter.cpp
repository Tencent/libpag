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

#include "AlphaEdgeDetectFilter.h"

namespace pag {
static const char FRAGMENT_SHADER[] = R"(
        precision mediump float;
        in vec2 vertexColor;
        uniform sampler2D sTexture;

        layout(std140) uniform FilterUniforms {
            float mHorizontalStep;
            float mVerticalStep;
        };

        out vec4 tgfx_FragColor;
    
        float threshold = 0.9;

        void main() {
            float alphaSum = 0.0;
            alphaSum += floor(texture(sTexture, vertexColor + vec2(-mHorizontalStep, -mVerticalStep)).a + threshold);
            alphaSum += floor(texture(sTexture, vertexColor + vec2(-mHorizontalStep, 0.0)).a + threshold);
            alphaSum += floor(texture(sTexture, vertexColor + vec2(-mHorizontalStep, mVerticalStep)).a + threshold);
            alphaSum += floor(texture(sTexture, vertexColor + vec2(mHorizontalStep, -mVerticalStep)).a + threshold);
            alphaSum += floor(texture(sTexture, vertexColor + vec2(mHorizontalStep, 0.0)).a + threshold);
            alphaSum += floor(texture(sTexture, vertexColor + vec2(mHorizontalStep, mVerticalStep)).a + threshold);
            alphaSum += floor(texture(sTexture, vertexColor + vec2(0.0, -mVerticalStep)).a + threshold);
            alphaSum += floor(texture(sTexture, vertexColor + vec2(0.0, 0.0)).a + threshold);
            alphaSum += floor(texture(sTexture, vertexColor + vec2(0.0, mVerticalStep)).a + threshold);
            tgfx_FragColor = (alphaSum > 0.0 && alphaSum < 9.0) ? vec4(1.0) : vec4(0.0);
        }
    )";

struct AlphaEdgeDetectUniforms {
  float horizontalStep = 0.0f;
  float verticalStep = 0.0f;
};

std::string AlphaEdgeDetectLayerEffect::onBuildFragmentShader() const {
  return FRAGMENT_SHADER;
}

std::vector<tgfx::BindingEntry> AlphaEdgeDetectLayerEffect::uniformBlocks() const {
  return {{"FilterUniforms", 0}};
}

void AlphaEdgeDetectLayerEffect::onUpdateUniforms(
    tgfx::RenderPass* renderPass, tgfx::GPU* gpu,
    const std::vector<std::shared_ptr<tgfx::Texture>>& inputTextures, const tgfx::Point&) const {
  if (inputTextures.empty()) {
    return;
  }

  AlphaEdgeDetectUniforms uniforms = {};
  uniforms.horizontalStep = 1.0f / inputTextures[0]->width();
  uniforms.verticalStep = 1.0f / inputTextures[0]->height();

  auto uniformBuffer =
      gpu->createBuffer(sizeof(AlphaEdgeDetectUniforms), tgfx::GPUBufferUsage::UNIFORM);
  if (uniformBuffer != nullptr) {
    auto* data = uniformBuffer->map();
    if (data != nullptr) {
      memcpy(data, &uniforms, sizeof(AlphaEdgeDetectUniforms));
      uniformBuffer->unmap();
      renderPass->setUniformBuffer(0, uniformBuffer, 0, sizeof(AlphaEdgeDetectUniforms));
    }
  }
}

}  // namespace pag
