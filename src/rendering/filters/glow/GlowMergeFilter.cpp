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
    #version 100
    precision mediump float;
    varying highp vec2 vertexColor;
    uniform sampler2D inputImageTexture;
    uniform sampler2D blurImageTexture;
    uniform float progress;
    float translate(float fValue, float fMax)
    {
        return fValue / fMax;
    }
    void main() {
        vec4 srcColor = texture2D(inputImageTexture, vertexColor);
        vec4 blurColor = texture2D(blurImageTexture, vertexColor);
        vec4 glowColor = vec4(0.0, 0.0, 0.0, srcColor.a);
        srcColor.rgb = max(srcColor.rgb, blurColor.rgb);
        glowColor.r = translate(srcColor.r,progress);
        glowColor.g = translate(srcColor.g,progress);
        glowColor.b = translate(srcColor.b,progress);
        gl_FragColor = glowColor;
    }
    )";

GlowMergeUniforms::GlowMergeUniforms(tgfx::Context* context, unsigned program)
    : Uniforms(context, program) {
  auto gl = tgfx::GLFunctions::Get(context);
  progressHandle = gl->getUniformLocation(program, "progress");
}
GlowMergeRuntimeFilter::GlowMergeRuntimeFilter(float progress,
                                               std::shared_ptr<tgfx::Image> blurImage)
    : RuntimeFilter({std::move(blurImage)}), progress(progress) {
}

std::string GlowMergeRuntimeFilter::onBuildFragmentShader() const {
  return GLOW_TARGET_FRAGMENT_SHADER;
}

std::unique_ptr<Uniforms> GlowMergeRuntimeFilter::onPrepareProgram(tgfx::Context* context,
                                                                   unsigned program) const {
  return std::make_unique<GlowMergeUniforms>(context, program);
}

void GlowMergeRuntimeFilter::onUpdateParams(tgfx::Context* context, const RuntimeProgram* program,
                                            const std::vector<tgfx::BackendTexture>&) const {
  auto gl = tgfx::GLFunctions::Get(context);
  auto uniform = static_cast<GlowMergeUniforms*>(program->uniforms.get());
  gl->uniform1f(uniform->progressHandle, progress);
}

}  // namespace pag
