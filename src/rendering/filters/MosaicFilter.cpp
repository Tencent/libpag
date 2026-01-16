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

#include "MosaicFilter.h"
#include "tgfx/core/ImageFilter.h"

namespace pag {
static const char FRAGMENT_SHADER[] = R"(
        precision mediump float;
        in vec2 vertexColor;
        uniform sampler2D sTexture;
        layout(std140) uniform Args {
            float mHorizontalBlocks;
            float mVerticalBlocks;
            int mSharpColors;
        };
        out vec4 tgfx_FragColor;

        void main() {
            vec2 blocks = vec2(mHorizontalBlocks, mVerticalBlocks);
            vec2 position = floor(vertexColor / blocks);
            vec2 target = blocks * position + blocks / 2.0;
            tgfx_FragColor = texture(sTexture, target);
        }
    )";

std::shared_ptr<tgfx::Image> MosaicFilter::Apply(std::shared_ptr<tgfx::Image> input,
                                                 RenderCache* cache, Effect* effect,
                                                 Frame layerFrame, tgfx::Point* offset) {
  auto* mosaicEffect = reinterpret_cast<const MosaicEffect*>(effect);
  auto sharpColors = mosaicEffect->sharpColors->getValueAt(layerFrame);

  auto horizontalBlocks = 1.0f / mosaicEffect->horizontalBlocks->getValueAt(layerFrame);
  auto verticalBlocks = 1.0f / mosaicEffect->verticalBlocks->getValueAt(layerFrame);

  auto filter =
      std::make_shared<MosaicFilter>(cache, horizontalBlocks, verticalBlocks, sharpColors);

  return input->makeWithFilter(tgfx::ImageFilter::Runtime(filter), offset);
}

std::string MosaicFilter::onBuildFragmentShader() const {
  return FRAGMENT_SHADER;
}

std::vector<tgfx::BindingEntry> MosaicFilter::uniformBlocks() const {
  return {{"Args", 0}};
}

void MosaicFilter::onUpdateUniforms(tgfx::RenderPass* renderPass, tgfx::GPU* gpu,
                                    const std::vector<std::shared_ptr<tgfx::Texture>>&,
                                    const tgfx::Point&) const {
  struct Uniforms {
    float horizontalBlocks = 0.0f;
    float verticalBlocks = 0.0f;
    int sharpColors = 0;
  };

  Uniforms uniforms = {};
  uniforms.horizontalBlocks = horizontalBlocks;
  uniforms.verticalBlocks = verticalBlocks;
  uniforms.sharpColors = sharpColors ? 1 : 0;

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
