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
        #version 100
        precision mediump float;
        varying vec2 vertexColor;
        uniform sampler2D sTexture;
        uniform float mHorizontalStep;
        uniform float mVerticalStep;
    
        float threshold = 0.9;

        void main() {
            float alphaSum = 0.0;
            alphaSum += floor(texture2D(sTexture, vertexColor + vec2(-mHorizontalStep, -mVerticalStep)).a + threshold);
            alphaSum += floor(texture2D(sTexture, vertexColor + vec2(-mHorizontalStep, 0.0)).a + threshold);
            alphaSum += floor(texture2D(sTexture, vertexColor + vec2(-mHorizontalStep, mVerticalStep)).a + threshold);
            alphaSum += floor(texture2D(sTexture, vertexColor + vec2(mHorizontalStep, -mVerticalStep)).a + threshold);
            alphaSum += floor(texture2D(sTexture, vertexColor + vec2(mHorizontalStep, 0.0)).a + threshold);
            alphaSum += floor(texture2D(sTexture, vertexColor + vec2(mHorizontalStep, mVerticalStep)).a + threshold);
            alphaSum += floor(texture2D(sTexture, vertexColor + vec2(0.0, -mVerticalStep)).a + threshold);
            alphaSum += floor(texture2D(sTexture, vertexColor + vec2(0.0, 0.0)).a + threshold);
            alphaSum += floor(texture2D(sTexture, vertexColor + vec2(0.0, mVerticalStep)).a + threshold);
            gl_FragColor = (alphaSum > 0.0 && alphaSum < 9.0) ? vec4(1.0) : vec4(0.0);
        }
    )";

std::string AlphaEdgeDetectLayerEffect::onBuildFragmentShader() const {
  return FRAGMENT_SHADER;
}

std::unique_ptr<Uniforms> AlphaEdgeDetectLayerEffect::onPrepareProgram(tgfx::Context* context,
                                                                       unsigned program) const {
  return std::make_unique<AlphaEdgeDetectEffectUniforms>(context, program);
}

void AlphaEdgeDetectLayerEffect::onUpdateParams(
    tgfx::Context* context, const RuntimeProgram* program,
    const std::vector<tgfx::BackendTexture>& sources) const {
  auto uniforms = static_cast<AlphaEdgeDetectEffectUniforms*>(program->uniforms.get());
  auto gl = tgfx::GLFunctions::Get(context);
  gl->uniform1f(uniforms->horizontalStepHandle, 1.0f / sources[0].width());
  gl->uniform1f(uniforms->verticalStepHandle, 1.0f / sources[0].height());
}

}  // namespace pag
