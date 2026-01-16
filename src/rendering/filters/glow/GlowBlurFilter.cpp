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

#include "GlowBlurFilter.h"

namespace pag {
static const char GLOW_BLUR_VERTEX_SHADER[] = R"(
    in vec2 aPosition;
    in vec2 aTextureCoord;

    layout(std140) uniform VertexUniforms {
        float textureOffsetH;
        float textureOffsetV;
    };

    out vec2 blurCoordinates[5];

    void main() {
        gl_Position = vec4(aPosition.xy, 0, 1);
        vec2 singleStepOffset = vec2(textureOffsetH, textureOffsetV);
        blurCoordinates[0] = aTextureCoord.xy;
        blurCoordinates[1] = aTextureCoord.xy + singleStepOffset * 1.182425;
        blurCoordinates[2] = aTextureCoord.xy - singleStepOffset * 1.182425;
        blurCoordinates[3] = aTextureCoord.xy + singleStepOffset * 3.029312;
        blurCoordinates[4] = aTextureCoord.xy - singleStepOffset * 3.029312;
    }
    )";

static const char GLOW_BLUR_FRAGMENT_SHADER[] = R"(
    precision mediump float;
    uniform sampler2D sTexture;
    in highp vec2 blurCoordinates[5];
    out vec4 tgfx_FragColor;
    void main() {
        lowp vec4 sum = vec4(0.0);
        sum += texture(sTexture, blurCoordinates[0]) * 0.398943;
        sum += texture(sTexture, blurCoordinates[1]) * 0.295963;
        sum += texture(sTexture, blurCoordinates[2]) * 0.295963;
        sum += texture(sTexture, blurCoordinates[3]) * 0.004566;
        sum += texture(sTexture, blurCoordinates[4]) * 0.004566;
        tgfx_FragColor = sum;
    }
    )";

struct GlowBlurUniforms {
  float textureOffsetH = 0.0f;
  float textureOffsetV = 0.0f;
};

GlowBlurRuntimeFilter::GlowBlurRuntimeFilter(RenderCache* cache, BlurDirection blurDirection,
                                             float blurOffset, float resizeRatio)
    : RuntimeFilter(cache), blurDirection(blurDirection), blurOffset(blurOffset),
      resizeRatio(resizeRatio) {
}

std::string GlowBlurRuntimeFilter::onBuildVertexShader() const {
  return GLOW_BLUR_VERTEX_SHADER;
}

std::string GlowBlurRuntimeFilter::onBuildFragmentShader() const {
  return GLOW_BLUR_FRAGMENT_SHADER;
}

std::vector<tgfx::BindingEntry> GlowBlurRuntimeFilter::uniformBlocks() const {
  return {{"VertexUniforms", 0}};
}

void GlowBlurRuntimeFilter::onUpdateUniforms(tgfx::RenderPass* renderPass, tgfx::GPU* gpu,
                                             const std::vector<std::shared_ptr<tgfx::Texture>>&,
                                             const tgfx::Point&) const {
  GlowBlurUniforms uniforms = {};
  uniforms.textureOffsetH = blurDirection == BlurDirection::Horizontal ? blurOffset : 0;
  uniforms.textureOffsetV = blurDirection == BlurDirection::Vertical ? blurOffset : 0;
  auto uniformBuffer = gpu->createBuffer(sizeof(GlowBlurUniforms), tgfx::GPUBufferUsage::UNIFORM);
  if (uniformBuffer != nullptr) {
    auto* data = uniformBuffer->map();
    if (data != nullptr) {
      memcpy(data, &uniforms, sizeof(GlowBlurUniforms));
      uniformBuffer->unmap();
      renderPass->setUniformBuffer(0, uniformBuffer, 0, sizeof(GlowBlurUniforms));
    }
  }
}

tgfx::Rect GlowBlurRuntimeFilter::filterBounds(const tgfx::Rect& srcRect) const {
  auto result = srcRect;
  result.scale(resizeRatio, resizeRatio);
  result.offsetTo(srcRect.left, srcRect.top);
  result.roundOut();
  return result;
}

}  // namespace pag
