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

#include "LevelsIndividualFilter.h"

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
            return (clamp(pow(((inPixel * 255.0) - param.inBlack) / (param.inWhite - param.inBlack), 1.0 / param.gamma), 0.0, 1.0) * (param.outWhite - param.outBlack) + param.outBlack) / 255.0;
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

LevelsIndividualFilter::LevelsIndividualFilter(pag::Effect* effect) : effect(effect) {
}

std::string LevelsIndividualFilter::onBuildFragmentShader() {
  return FRAGMENT_SHADER;
}

void LevelsIndividualFilter::onPrepareProgram(const tgfx::GLInterface* gl, unsigned int program) {
  inputBlackHandle = gl->functions->getUniformLocation(program, "inputBlack");
  inputWhiteHandle = gl->functions->getUniformLocation(program, "inputWhite");
  gammaHandle = gl->functions->getUniformLocation(program, "gamma");
  outputBlackHandle = gl->functions->getUniformLocation(program, "outputBlack");
  outputWhiteHandle = gl->functions->getUniformLocation(program, "outputWhite");

  redInputBlackHandle = gl->functions->getUniformLocation(program, "redInputBlack");
  redInputWhiteHandle = gl->functions->getUniformLocation(program, "redInputWhite");
  redGammaHandle = gl->functions->getUniformLocation(program, "redGamma");
  redOutputBlackHandle = gl->functions->getUniformLocation(program, "redOutputBlack");
  redOutputWhiteHandle = gl->functions->getUniformLocation(program, "redOutputWhite");

  greenInputBlackHandle = gl->functions->getUniformLocation(program, "greenInputBlack");
  greenInputWhiteHandle = gl->functions->getUniformLocation(program, "greenInputWhite");
  greenGammaHandle = gl->functions->getUniformLocation(program, "greenGamma");
  greenOutputBlackHandle = gl->functions->getUniformLocation(program, "greenOutputBlack");
  greenOutputWhiteHandle = gl->functions->getUniformLocation(program, "greenOutputWhite");

  blueInputBlackHandle = gl->functions->getUniformLocation(program, "blueInputBlack");
  blueInputWhiteHandle = gl->functions->getUniformLocation(program, "blueInputWhite");
  blueGammaHandle = gl->functions->getUniformLocation(program, "blueGamma");
  blueOutputBlackHandle = gl->functions->getUniformLocation(program, "blueOutputBlack");
  blueOutputWhiteHandle = gl->functions->getUniformLocation(program, "blueOutputWhite");
}

void LevelsIndividualFilter::onUpdateParams(const tgfx::GLInterface* gl, const tgfx::Rect&,
                                            const tgfx::Point&) {
  auto* levelsIndividualFilter = reinterpret_cast<const LevelsIndividualEffect*>(effect);
  gl->functions->uniform1f(inputBlackHandle,
                           levelsIndividualFilter->inputBlack->getValueAt(layerFrame));
  gl->functions->uniform1f(inputWhiteHandle,
                           levelsIndividualFilter->inputWhite->getValueAt(layerFrame));
  gl->functions->uniform1f(gammaHandle, levelsIndividualFilter->gamma->getValueAt(layerFrame));
  gl->functions->uniform1f(outputBlackHandle,
                           levelsIndividualFilter->outputBlack->getValueAt(layerFrame));
  gl->functions->uniform1f(outputWhiteHandle,
                           levelsIndividualFilter->outputWhite->getValueAt(layerFrame));

  gl->functions->uniform1f(redInputBlackHandle,
                           levelsIndividualFilter->redInputBlack->getValueAt(layerFrame));
  gl->functions->uniform1f(redInputWhiteHandle,
                           levelsIndividualFilter->redInputWhite->getValueAt(layerFrame));
  gl->functions->uniform1f(redGammaHandle,
                           levelsIndividualFilter->redGamma->getValueAt(layerFrame));
  gl->functions->uniform1f(redOutputBlackHandle,
                           levelsIndividualFilter->redOutputBlack->getValueAt(layerFrame));
  gl->functions->uniform1f(redOutputWhiteHandle,
                           levelsIndividualFilter->redOutputWhite->getValueAt(layerFrame));

  gl->functions->uniform1f(greenInputBlackHandle,
                           levelsIndividualFilter->greenInputBlack->getValueAt(layerFrame));
  gl->functions->uniform1f(greenInputWhiteHandle,
                           levelsIndividualFilter->greenInputWhite->getValueAt(layerFrame));
  gl->functions->uniform1f(greenGammaHandle,
                           levelsIndividualFilter->greenGamma->getValueAt(layerFrame));
  gl->functions->uniform1f(greenOutputBlackHandle,
                           levelsIndividualFilter->greenOutputBlack->getValueAt(layerFrame));
  gl->functions->uniform1f(greenOutputWhiteHandle,
                           levelsIndividualFilter->greenOutputWhite->getValueAt(layerFrame));

  gl->functions->uniform1f(blueInputBlackHandle,
                           levelsIndividualFilter->blueInputBlack->getValueAt(layerFrame));
  gl->functions->uniform1f(blueInputWhiteHandle,
                           levelsIndividualFilter->blueInputWhite->getValueAt(layerFrame));
  gl->functions->uniform1f(blueGammaHandle,
                           levelsIndividualFilter->blueGamma->getValueAt(layerFrame));
  gl->functions->uniform1f(blueOutputBlackHandle,
                           levelsIndividualFilter->blueOutputBlack->getValueAt(layerFrame));
  gl->functions->uniform1f(blueOutputWhiteHandle,
                           levelsIndividualFilter->blueOutputWhite->getValueAt(layerFrame));
}
}  // namespace pag
