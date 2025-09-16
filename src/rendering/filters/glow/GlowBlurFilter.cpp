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
    #version 100
    attribute vec2 aPosition;
    attribute vec2 aTextureCoord;

    uniform float textureOffsetH;
    uniform float textureOffsetV;

    varying vec2 blurCoordinates[5];

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
    #version 100
    precision mediump float;
    uniform sampler2D inputImageTexture;
    varying highp vec2 blurCoordinates[5];
    void main() {
        lowp vec4 sum = vec4(0.0);
        sum += texture2D(inputImageTexture, blurCoordinates[0]) * 0.398943;
        sum += texture2D(inputImageTexture, blurCoordinates[1]) * 0.295963;
        sum += texture2D(inputImageTexture, blurCoordinates[2]) * 0.295963;
        sum += texture2D(inputImageTexture, blurCoordinates[3]) * 0.004566;
        sum += texture2D(inputImageTexture, blurCoordinates[4]) * 0.004566;
        gl_FragColor = sum;
    }
    )";
GlowBlurUniforms::GlowBlurUniforms(tgfx::Context* context, unsigned program)
    : Uniforms(context, program) {
  auto gl = tgfx::GLFunctions::Get(context);
  textureOffsetHHandle = gl->getUniformLocation(program, "textureOffsetH");
  textureOffsetVHandle = gl->getUniformLocation(program, "textureOffsetV");
}

GlowBlurRuntimeFilter::GlowBlurRuntimeFilter(BlurDirection blurDirection, float blurOffset,
                                             float resizeRatio)
    : blurDirection(blurDirection), blurOffset(blurOffset), resizeRatio(resizeRatio) {
}

std::string GlowBlurRuntimeFilter::onBuildVertexShader() const {
  return GLOW_BLUR_VERTEX_SHADER;
}

std::string GlowBlurRuntimeFilter::onBuildFragmentShader() const {
  return GLOW_BLUR_FRAGMENT_SHADER;
}

std::unique_ptr<Uniforms> GlowBlurRuntimeFilter::onPrepareProgram(tgfx::Context* context,
                                                                  unsigned program) const {
  return std::make_unique<GlowBlurUniforms>(context, program);
}

void GlowBlurRuntimeFilter::onUpdateParams(tgfx::Context* context, const RuntimeProgram* program,
                                           const std::vector<tgfx::BackendTexture>&) const {
  auto textureOffsetH = blurDirection == BlurDirection::Horizontal ? blurOffset : 0;
  auto textureOffsetV = blurDirection == BlurDirection::Vertical ? blurOffset : 0;
  auto gl = tgfx::GLFunctions::Get(context);
  auto uniform = static_cast<GlowBlurUniforms*>(program->uniforms.get());
  gl->uniform1f(uniform->textureOffsetHHandle, textureOffsetH);
  gl->uniform1f(uniform->textureOffsetVHandle, textureOffsetV);
}

tgfx::Rect GlowBlurRuntimeFilter::filterBounds(const tgfx::Rect& srcRect) const {
  auto result = srcRect;
  result.scale(resizeRatio, resizeRatio);
  result.offsetTo(srcRect.left, srcRect.top);
  result.roundOut();
  return result;
}

}  // namespace pag
