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

#include "GlowMergeFilter.h"

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

GlowMergeFilter::GlowMergeFilter(Effect* effect) : effect(effect) {
}

std::string GlowMergeFilter::onBuildFragmentShader() {
  return GLOW_TARGET_FRAGMENT_SHADER;
}

void GlowMergeFilter::onPrepareProgram(const tgfx::GLInterface* gl, unsigned int program) {
  inputTextureHandle = gl->getUniformLocation(program, "inputImageTexture");
  blurTextureHandle = gl->getUniformLocation(program, "blurImageTexture");
  progressHandle = gl->getUniformLocation(program, "progress");
}

void GlowMergeFilter::updateTexture(unsigned blurTexture) {
  blurTextureID = blurTexture;
}

void GlowMergeFilter::onUpdateParams(const tgfx::GLInterface* gl, const tgfx::Rect&,
                                     const tgfx::Point&) {
  auto glowEffect = static_cast<GlowEffect*>(effect);
  auto glowThreshold = glowEffect->glowThreshold->getValueAt(layerFrame);
  gl->uniform1f(progressHandle, glowThreshold);
  gl->uniform1i(inputTextureHandle, 0);
  // TODO(domrjchen): 下面这行之前写成了 gl->uniform1i(progressHandle, 1), 会导致 glError,
  // 暂时注释掉。目前的发光效果跟 AE 也没有对齐，后面重写发光效果时时再修复。
  //  gl->uniform1i(blurTextureHandle, 1);
  ActiveGLTexture(gl, GL_TEXTURE1, GL_TEXTURE_2D, blurTextureID);
}
}  // namespace pag
