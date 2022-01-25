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

#include "GlowBlurFilter.h"
#include "gpu/opengl/GLUtil.h"

namespace pag {
static const char GLOW_BLUR_VERTEX_SHADER[] = R"(
    #version 100
    attribute vec2 aPosition;
    attribute vec2 aTextureCoord;
    uniform mat3 uVertexMatrix;
    uniform mat3 uTextureMatrix;

    uniform float textureOffsetH;
    uniform float textureOffsetV;

    varying vec2 blurCoordinates[5];

    void main() {
        vec3 position = uVertexMatrix * vec3(aPosition, 1);
        gl_Position = vec4(position.xy, 0, 1);
        vec3 colorPosition = uTextureMatrix * vec3(aTextureCoord, 1);
        vec2 singleStepOffset = vec2(textureOffsetH, textureOffsetV);
        blurCoordinates[0] = colorPosition.xy;
        blurCoordinates[1] = colorPosition.xy + singleStepOffset * 1.182425;
        blurCoordinates[2] = colorPosition.xy - singleStepOffset * 1.182425;
        blurCoordinates[3] = colorPosition.xy + singleStepOffset * 3.029312;
        blurCoordinates[4] = colorPosition.xy - singleStepOffset * 3.029312;
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

GlowBlurFilter::GlowBlurFilter(BlurDirection direction) : blurDirection(direction) {
}

std::string GlowBlurFilter::onBuildVertexShader() {
    return GLOW_BLUR_VERTEX_SHADER;
}

std::string GlowBlurFilter::onBuildFragmentShader() {
    return GLOW_BLUR_FRAGMENT_SHADER;
}

void GlowBlurFilter::onPrepareProgram(const GLInterface* gl, unsigned int program) {
    textureOffsetHHandle = gl->getUniformLocation(program, "textureOffsetH");
    textureOffsetVHandle = gl->getUniformLocation(program, "textureOffsetV");
}

void GlowBlurFilter::updateOffset(float offset) {
    blurOffset = offset;
}

void GlowBlurFilter::onUpdateParams(const GLInterface* gl, const Rect&, const Point&) {
    auto textureOffsetH = blurDirection == BlurDirection::Horizontal ? blurOffset : 0;
    auto textureOffsetV = blurDirection == BlurDirection::Vertical ? blurOffset : 0;
    gl->uniform1f(textureOffsetHHandle, textureOffsetH);
    gl->uniform1f(textureOffsetVHandle, textureOffsetV);
}
}  // namespace pag
