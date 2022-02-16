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

std::string MosaicFilter::onBuildFragmentShader() {
  return FRAGMENT_SHADER;
}

void MosaicFilter::onPrepareProgram(const tgfx::GLInterface* gl, unsigned int program) {
  horizontalBlocksHandle = gl->getUniformLocation(program, "mHorizontalBlocks");
  verticalBlocksHandle = gl->getUniformLocation(program, "mVerticalBlocks");
  sharpColorsHandle = gl->getUniformLocation(program, "mSharpColors");
}

void MosaicFilter::onUpdateParams(const tgfx::GLInterface* gl, const tgfx::Rect& contentBounds,
                                  const tgfx::Point&) {
  auto* mosaicEffect = reinterpret_cast<const MosaicEffect*>(effect);
  horizontalBlocks = 1.0f / mosaicEffect->horizontalBlocks->getValueAt(layerFrame);
  verticalBlocks = 1.0f / mosaicEffect->verticalBlocks->getValueAt(layerFrame);
  sharpColors = mosaicEffect->sharpColors->getValueAt(layerFrame);

  auto placeHolderWidth = static_cast<int>(contentBounds.left + contentBounds.right);
  auto placeHolderHeight = static_cast<int>(contentBounds.top + contentBounds.bottom);
  auto placeHolderRatio = 1.0f * placeHolderWidth / placeHolderHeight;

  auto contentWidth = static_cast<int>(contentBounds.width());
  auto contentHeight = static_cast<int>(contentBounds.height());
  auto contentRatio = 1.0f * contentWidth / contentHeight;

  if (placeHolderRatio > contentRatio) {
    horizontalBlocks *= 1.0f * placeHolderWidth / contentWidth;
  } else {
    verticalBlocks *= 1.0f * placeHolderHeight / contentHeight;
  }

  gl->uniform1f(horizontalBlocksHandle, horizontalBlocks);
  gl->uniform1f(verticalBlocksHandle, verticalBlocks);
  gl->uniform1f(sharpColorsHandle, sharpColors);
}
}  // namespace pag
