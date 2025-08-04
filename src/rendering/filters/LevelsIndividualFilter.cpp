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

#include "LevelsIndividualFilter.h"
#include "tgfx/core/ImageFilter.h"

namespace pag {
static const char FRAGMENT_SHADER[] = R"(
        #version 100
        precision mediump float;
        varying vec2 vertexColor;
        uniform sampler2D sTexture;

        uniform float inputBlack;
        uniform float inputWhite;
        uniform float gamma;
        uniform float outputBlack;
        uniform float outputWhite;

        uniform float redInputBlack;
        uniform float redInputWhite;
        uniform float redGamma;
        uniform float redOutputBlack;
        uniform float redOutputWhite;

        uniform float blueInputBlack;
        uniform float blueInputWhite;
        uniform float blueGamma;
        uniform float blueOutputBlack;
        uniform float blueOutputWhite;

        uniform float greenInputBlack;
        uniform float greenInputWhite;
        uniform float greenGamma;
        uniform float greenOutputBlack;
        uniform float greenOutputWhite;

        struct LevelsIndividualParam {
            float inBlack;
            float inWhite;
            float gamma;
            float outBlack;
            float outWhite;
        };

        float GetPixelLevel(float inPixel, LevelsIndividualParam param) {
            float x = ((inPixel * 255.0) - param.inBlack) / (param.inWhite - param.inBlack);
            float y = 1.0 / param.gamma;
            float p = 0.0;
            if (!(x < 0.0 || (x == 0.0 && y <= 0.0))) {
                p = clamp(pow(x, y), 0.0, 1.0);
            }
            return (p * (param.outWhite - param.outBlack) + param.outBlack) / 255.0;
        }

        void main() {
            vec4 color = texture2D(sTexture, vertexColor);
            if (color.a == 0.0) {
                gl_FragColor = color;
                return;
            }
            vec4 newColor = vec4(0,0,0,color.a);
            newColor.r = GetPixelLevel(color.r, LevelsIndividualParam(redInputBlack,redInputWhite,redGamma,redOutputBlack,redOutputWhite));
            newColor.g = GetPixelLevel(color.g, LevelsIndividualParam(greenInputBlack,greenInputWhite,greenGamma,greenOutputBlack,greenOutputWhite));
            newColor.b = GetPixelLevel(color.b, LevelsIndividualParam(blueInputBlack,blueInputWhite,blueGamma,blueOutputBlack,blueOutputWhite));

            newColor.r = GetPixelLevel(newColor.r, LevelsIndividualParam(inputBlack,inputWhite,gamma,outputBlack,outputWhite));
            newColor.g = GetPixelLevel(newColor.g, LevelsIndividualParam(inputBlack,inputWhite,gamma,outputBlack,outputWhite));
            newColor.b = GetPixelLevel(newColor.b, LevelsIndividualParam(inputBlack,inputWhite,gamma,outputBlack,outputWhite));
            gl_FragColor = newColor;
        }
    )";

LevelsIndividualUniforms::LevelsIndividualUniforms(tgfx::Context* context, unsigned program)
    : Uniforms(context, program) {
  auto gl = tgfx::GLFunctions::Get(context);
  inputBlackHandle = gl->getUniformLocation(program, "inputBlack");
  inputWhiteHandle = gl->getUniformLocation(program, "inputWhite");
  gammaHandle = gl->getUniformLocation(program, "gamma");
  outputBlackHandle = gl->getUniformLocation(program, "outputBlack");
  outputWhiteHandle = gl->getUniformLocation(program, "outputWhite");

  redInputBlackHandle = gl->getUniformLocation(program, "redInputBlack");
  redInputWhiteHandle = gl->getUniformLocation(program, "redInputWhite");
  redGammaHandle = gl->getUniformLocation(program, "redGamma");
  redOutputBlackHandle = gl->getUniformLocation(program, "redOutputBlack");
  redOutputWhiteHandle = gl->getUniformLocation(program, "redOutputWhite");

  greenInputBlackHandle = gl->getUniformLocation(program, "greenInputBlack");
  greenInputWhiteHandle = gl->getUniformLocation(program, "greenInputWhite");
  greenGammaHandle = gl->getUniformLocation(program, "greenGamma");
  greenOutputBlackHandle = gl->getUniformLocation(program, "greenOutputBlack");
  greenOutputWhiteHandle = gl->getUniformLocation(program, "greenOutputWhite");

  blueInputBlackHandle = gl->getUniformLocation(program, "blueInputBlack");
  blueInputWhiteHandle = gl->getUniformLocation(program, "blueInputWhite");
  blueGammaHandle = gl->getUniformLocation(program, "blueGamma");
  blueOutputBlackHandle = gl->getUniformLocation(program, "blueOutputBlack");
  blueOutputWhiteHandle = gl->getUniformLocation(program, "blueOutputWhite");
}

std::shared_ptr<tgfx::Image> LevelsIndividualFilter::Apply(std::shared_ptr<tgfx::Image> input,
                                                           Effect* effect, Frame layerFrame,
                                                           tgfx::Point* offset) {
  auto levelsIndividualEffect = static_cast<LevelsIndividualEffect*>(effect);
  constexpr int redChannel = 0;
  constexpr int greenChannel = 1;
  constexpr int blueChannel = 2;
  constexpr int globalChannel = 3;
  LevelsIndividualFilterParam param;
  param.inBlack[redChannel] = levelsIndividualEffect->redInputBlack->getValueAt(layerFrame);
  param.inWhite[redChannel] = levelsIndividualEffect->redInputWhite->getValueAt(layerFrame);
  param.gamma[redChannel] = levelsIndividualEffect->redGamma->getValueAt(layerFrame);
  param.outBlack[redChannel] = levelsIndividualEffect->redOutputBlack->getValueAt(layerFrame);
  param.outWhite[redChannel] = levelsIndividualEffect->redOutputWhite->getValueAt(layerFrame);

  param.inBlack[greenChannel] = levelsIndividualEffect->greenInputBlack->getValueAt(layerFrame);
  param.inWhite[greenChannel] = levelsIndividualEffect->greenInputWhite->getValueAt(layerFrame);
  param.gamma[greenChannel] = levelsIndividualEffect->greenGamma->getValueAt(layerFrame);
  param.outBlack[greenChannel] = levelsIndividualEffect->greenOutputBlack->getValueAt(layerFrame);
  param.outWhite[greenChannel] = levelsIndividualEffect->greenOutputWhite->getValueAt(layerFrame);

  param.inBlack[blueChannel] = levelsIndividualEffect->blueInputBlack->getValueAt(layerFrame);
  param.inWhite[blueChannel] = levelsIndividualEffect->blueInputWhite->getValueAt(layerFrame);
  param.gamma[blueChannel] = levelsIndividualEffect->blueGamma->getValueAt(layerFrame);
  param.outBlack[blueChannel] = levelsIndividualEffect->blueOutputBlack->getValueAt(layerFrame);
  param.outWhite[blueChannel] = levelsIndividualEffect->blueOutputWhite->getValueAt(layerFrame);

  param.inBlack[globalChannel] = levelsIndividualEffect->inputBlack->getValueAt(layerFrame);
  param.inWhite[globalChannel] = levelsIndividualEffect->inputWhite->getValueAt(layerFrame);
  param.gamma[globalChannel] = levelsIndividualEffect->gamma->getValueAt(layerFrame);
  param.outBlack[globalChannel] = levelsIndividualEffect->outputBlack->getValueAt(layerFrame);
  param.outWhite[globalChannel] = levelsIndividualEffect->outputWhite->getValueAt(layerFrame);
  auto filter = std::make_shared<LevelsIndividualFilter>(param);
  return input->makeWithFilter(tgfx::ImageFilter::Runtime(filter), offset);
}

std::string LevelsIndividualFilter::onBuildFragmentShader() const {
  return FRAGMENT_SHADER;
}

std::unique_ptr<Uniforms> LevelsIndividualFilter::onPrepareProgram(tgfx::Context* context,
                                                                   unsigned program) const {
  return std::make_unique<LevelsIndividualUniforms>(context, program);
}

void LevelsIndividualFilter::onUpdateParams(tgfx::Context* context, const RuntimeProgram* program,
                                            const std::vector<tgfx::BackendTexture>&) const {
  auto gl = tgfx::GLFunctions::Get(context);
  auto uniform = static_cast<LevelsIndividualUniforms*>(program->uniforms.get());
  constexpr int redChannel = 0;
  constexpr int greenChannel = 1;
  constexpr int blueChannel = 2;
  constexpr int globalChannel = 3;
  gl->uniform1f(uniform->inputBlackHandle, param.inBlack[globalChannel]);
  gl->uniform1f(uniform->inputWhiteHandle, param.inWhite[globalChannel]);
  gl->uniform1f(uniform->gammaHandle, param.gamma[globalChannel]);
  gl->uniform1f(uniform->outputBlackHandle, param.outBlack[globalChannel]);
  gl->uniform1f(uniform->outputWhiteHandle, param.outWhite[globalChannel]);

  gl->uniform1f(uniform->redInputBlackHandle, param.inBlack[redChannel]);
  gl->uniform1f(uniform->redInputWhiteHandle, param.inWhite[redChannel]);
  gl->uniform1f(uniform->redGammaHandle, param.gamma[redChannel]);
  gl->uniform1f(uniform->redOutputBlackHandle, param.outBlack[redChannel]);
  gl->uniform1f(uniform->redOutputWhiteHandle, param.outWhite[redChannel]);

  gl->uniform1f(uniform->greenInputBlackHandle, param.inBlack[greenChannel]);
  gl->uniform1f(uniform->greenInputWhiteHandle, param.inWhite[greenChannel]);
  gl->uniform1f(uniform->greenGammaHandle, param.gamma[greenChannel]);
  gl->uniform1f(uniform->greenOutputBlackHandle, param.outBlack[greenChannel]);
  gl->uniform1f(uniform->greenOutputWhiteHandle, param.outWhite[greenChannel]);

  gl->uniform1f(uniform->blueInputBlackHandle, param.inBlack[blueChannel]);
  gl->uniform1f(uniform->blueInputWhiteHandle, param.inWhite[blueChannel]);
  gl->uniform1f(uniform->blueGammaHandle, param.gamma[blueChannel]);
  gl->uniform1f(uniform->blueOutputBlackHandle, param.outBlack[blueChannel]);
  gl->uniform1f(uniform->blueOutputWhiteHandle, param.outWhite[blueChannel]);
}
}  // namespace pag
