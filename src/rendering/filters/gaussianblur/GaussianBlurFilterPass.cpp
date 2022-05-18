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

#include "GaussianBlurFilterPass.h"
#include "GaussianBlurShaders.h"

namespace pag {

GaussianBlurFilterPass::GaussianBlurFilterPass(BlurOptions options) : options(options) {
}

std::string GaussianBlurFilterPass::onBuildFragmentShader() {
  if ((options & BlurOptions::Down) != BlurOptions::None) {
    return BLUR_DOWN_FRAGMENT_SHADER;
  } else if ((options & BlurOptions::Up) != BlurOptions::None) {
    if ((options & BlurOptions::RepeatEdgePixels) != BlurOptions::None) {
      return BLUR_UP_FRAGMENT_SHADER;
    } else {
      return BLUR_UP_FRAGMENT_SHADER_NO_REPEAT_EDGE;
    }
  } else {
    return nullptr;
  }
}

void GaussianBlurFilterPass::onPrepareProgram(tgfx::Context* context, unsigned int program) {
  auto gl = tgfx::GLFunctions::Get(context);
  stepHandle = gl->getUniformLocation(program, "uStep");
  offsetHandle = gl->getUniformLocation(program, "uOffset");
}

void GaussianBlurFilterPass::updateParams(float blurValue, float scaleValue,
                                          bool isExpendBoundsValue) {
  blurriness = blurValue;
  scale = scaleValue;
  isExpendBounds = isExpendBoundsValue;
}

void GaussianBlurFilterPass::onUpdateParams(tgfx::Context* context, const tgfx::Rect& contentBounds,
                                            const tgfx::Point& filterScale) {
  auto gl = tgfx::GLFunctions::Get(context);
  gl->uniform2f(stepHandle, 0.5 / static_cast<float>(contentBounds.width()),
                0.5 / static_cast<float>(contentBounds.height()));
  gl->uniform2f(
      offsetHandle,
      (options & BlurOptions::Horizontal) != BlurOptions::None ? blurriness * filterScale.x : 0,
      (options & BlurOptions::Vertical) != BlurOptions::None ? blurriness * filterScale.y : 0);
}

std::vector<tgfx::Point> GaussianBlurFilterPass::computeVertices(const tgfx::Rect& inputBounds,
                                                                 const tgfx::Rect& outputBounds,
                                                                 const tgfx::Point& filterScale) {
  if (!isExpendBounds) {
    return LayerFilter::computeVertices(inputBounds, outputBounds, filterScale);
  }
  std::vector<tgfx::Point> vertices = {};
  tgfx::Point contentPoint[4] = {{outputBounds.left, outputBounds.bottom},
                                 {outputBounds.right, outputBounds.bottom},
                                 {outputBounds.left, outputBounds.top},
                                 {outputBounds.right, outputBounds.top}};

  auto expendX = (outputBounds.left - inputBounds.left) * scale;
  auto expendY = (outputBounds.top - inputBounds.top) * scale;

  tgfx::Point texturePoints[4] = {{expendX, inputBounds.height() - expendY},
                                  {inputBounds.width() - expendX, inputBounds.height() - expendY},
                                  {expendX, expendY},
                                  {inputBounds.width() - expendX, expendY}};

  for (int ii = 0; ii < 4; ii++) {
    vertices.push_back(contentPoint[ii]);
    vertices.push_back(texturePoints[ii]);
  }
  return vertices;
}
}  // namespace pag
