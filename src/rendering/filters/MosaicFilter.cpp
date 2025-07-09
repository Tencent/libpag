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
        #version 100
        precision mediump float;
        varying vec2 vertexColor;
        uniform sampler2D sTexture;
        uniform float mHorizontalBlocks;
        uniform float mVerticalBlocks;
        uniform bool mSharpColors;

        void main() {
            vec2 blocks = vec2(mHorizontalBlocks, mVerticalBlocks);
            vec2 position = floor(vertexColor / blocks);
            vec2 target = blocks * position + blocks / 2.0;
            gl_FragColor = texture2D(sTexture, target);
        }
    )";

std::shared_ptr<tgfx::Image> MosaicFilter::Apply(std::shared_ptr<tgfx::Image> input, Effect* effect,
                                                 Frame layerFrame, tgfx::Point* offset) {
  auto* mosaicEffect = reinterpret_cast<const MosaicEffect*>(effect);
  auto sharpColors = mosaicEffect->sharpColors->getValueAt(layerFrame);

  auto horizontalBlocks = 1.0f / mosaicEffect->horizontalBlocks->getValueAt(layerFrame);
  auto verticalBlocks = 1.0f / mosaicEffect->verticalBlocks->getValueAt(layerFrame);

  auto filter = std::make_shared<MosaicFilter>(horizontalBlocks, verticalBlocks, sharpColors);

  return input->makeWithFilter(tgfx::ImageFilter::Runtime(filter), offset);
}

std::string MosaicFilter::onBuildFragmentShader() const {
  return FRAGMENT_SHADER;
}

std::unique_ptr<Uniforms> MosaicFilter::onPrepareProgram(tgfx::Context* context,
                                                         unsigned program) const {
  return std::make_unique<MosaicUniforms>(context, program);
}

void MosaicFilter::onUpdateParams(tgfx::Context* context, const RuntimeProgram* program,
                                  const std::vector<tgfx::BackendTexture>&) const {
  auto gl = tgfx::GLFunctions::Get(context);
  auto uniform = static_cast<const MosaicUniforms*>(program->uniforms.get());
  gl->uniform1f(uniform->horizontalBlocksHandle, horizontalBlocks);
  gl->uniform1f(uniform->verticalBlocksHandle, verticalBlocks);
  gl->uniform1f(uniform->sharpColorsHandle, sharpColors);
}

}  // namespace pag
