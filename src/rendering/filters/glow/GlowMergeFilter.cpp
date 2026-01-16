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

#include "GlowMergeFilter.h"
#include <utility>

namespace pag {
static const char GLOW_TARGET_FRAGMENT_SHADER[] = R"(
    precision mediump float;
    in highp vec2 vertexColor;
    uniform sampler2D sTexture;
    uniform sampler2D blurImageTexture;

    layout(std140) uniform FilterUniforms {
        float progress;
    };

    float translate(float fValue, float fMax)
    {
        return fValue / fMax;
    }
    out vec4 tgfx_FragColor;
    void main() {
        vec4 srcColor = texture(sTexture, vertexColor);
        vec4 blurColor = texture(blurImageTexture, vertexColor);
        vec4 glowColor = vec4(0.0, 0.0, 0.0, srcColor.a);
        srcColor.rgb = max(srcColor.rgb, blurColor.rgb);
        glowColor.r = translate(srcColor.r, progress);
        glowColor.g = translate(srcColor.g, progress);
        glowColor.b = translate(srcColor.b, progress);
        glowColor = clamp(glowColor, 0.0, 1.0);
        tgfx_FragColor = glowColor;
    }
    )";

struct GlowMergeUniforms {
  float progress = 0.0f;
};

GlowMergeRuntimeFilter::GlowMergeRuntimeFilter(RenderCache* cache, float progress,
                                               std::shared_ptr<tgfx::Image> blurImage)
    : RuntimeFilter(cache, {std::move(blurImage)}), progress(progress) {
}

std::string GlowMergeRuntimeFilter::onBuildFragmentShader() const {
  return GLOW_TARGET_FRAGMENT_SHADER;
}

std::vector<tgfx::BindingEntry> GlowMergeRuntimeFilter::uniformBlocks() const {
  return {{"FilterUniforms", 0}};
}

std::vector<tgfx::BindingEntry> GlowMergeRuntimeFilter::textureSamplers() const {
  return {{"sTexture", 0}, {"blurImageTexture", 1}};
}

void GlowMergeRuntimeFilter::onUpdateUniforms(tgfx::RenderPass* renderPass, tgfx::GPU* gpu,
                                              const std::vector<std::shared_ptr<tgfx::Texture>>&,
                                              const tgfx::Point&) const {
  GlowMergeUniforms uniforms = {};
  uniforms.progress = progress;
  auto uniformBuffer = gpu->createBuffer(sizeof(GlowMergeUniforms), tgfx::GPUBufferUsage::UNIFORM);
  if (uniformBuffer != nullptr) {
    auto* data = uniformBuffer->map();
    if (data != nullptr) {
      memcpy(data, &uniforms, sizeof(GlowMergeUniforms));
      uniformBuffer->unmap();
      renderPass->setUniformBuffer(0, uniformBuffer, 0, sizeof(GlowMergeUniforms));
    }
  }
}

}  // namespace pag
