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

#include "MosaicFilter.h"

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

MosaicFilter::MosaicFilter(pag::Effect* effect) : effect(effect) {
}

std::string MosaicRuntimeFilter::onBuildFragmentShader() const {
  return FRAGMENT_SHADER;
}

std::unique_ptr<Uniforms> MosaicRuntimeFilter::onPrepareProgram(tgfx::Context* context,
                                                                unsigned program) const {
  return std::make_unique<MosaicUniforms>(context, program);
}

void MosaicRuntimeFilter::onUpdateParams(tgfx::Context* context, const RuntimeProgram* program,
                                         const std::vector<tgfx::BackendTexture>&) const {
  auto gl = tgfx::GLFunctions::Get(context);
  auto uniform = static_cast<const MosaicUniforms*>(program->uniforms.get());
  gl->uniform1f(uniform->horizontalBlocksHandle, horizontalBlocks);
  gl->uniform1f(uniform->verticalBlocksHandle, verticalBlocks);
  gl->uniform1f(uniform->sharpColorsHandle, sharpColors);
}

void MosaicFilter::update(Frame layerFrame, const tgfx::Point&) {
  auto* mosaicEffect = reinterpret_cast<const MosaicEffect*>(effect);
  sharpColors = mosaicEffect->sharpColors->getValueAt(layerFrame);

  horizontalBlocks = 1.0f / mosaicEffect->horizontalBlocks->getValueAt(layerFrame);
  verticalBlocks = 1.0f / mosaicEffect->verticalBlocks->getValueAt(layerFrame);
}

std::shared_ptr<tgfx::RuntimeEffect> MosaicFilter::createRuntimeEffect() {
  return std::make_shared<MosaicRuntimeFilter>(horizontalBlocks, verticalBlocks, sharpColors);
}

}  // namespace pag
