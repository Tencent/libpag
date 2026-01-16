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
        precision mediump float;
        in vec2 vertexColor;
        uniform sampler2D sTexture;
        layout(std140) uniform Args {
            float inputBlack;
            float inputWhite;
            float gamma;
            float outputBlack;
            float outputWhite;
            float redInputBlack;
            float redInputWhite;
            float redGamma;
            float redOutputBlack;
            float redOutputWhite;
            float blueInputBlack;
            float blueInputWhite;
            float blueGamma;
            float blueOutputBlack;
            float blueOutputWhite;
            float greenInputBlack;
            float greenInputWhite;
            float greenGamma;
            float greenOutputBlack;
            float greenOutputWhite;
        };
        out vec4 tgfx_FragColor;

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
            vec4 color = texture(sTexture, vertexColor);
            if (color.a == 0.0) {
                tgfx_FragColor = color;
                return;
            }
            vec4 newColor = vec4(0,0,0,color.a);
            newColor.r = GetPixelLevel(color.r, LevelsIndividualParam(redInputBlack,redInputWhite,redGamma,redOutputBlack,redOutputWhite));
            newColor.g = GetPixelLevel(color.g, LevelsIndividualParam(greenInputBlack,greenInputWhite,greenGamma,greenOutputBlack,greenOutputWhite));
            newColor.b = GetPixelLevel(color.b, LevelsIndividualParam(blueInputBlack,blueInputWhite,blueGamma,blueOutputBlack,blueOutputWhite));

            newColor.r = GetPixelLevel(newColor.r, LevelsIndividualParam(inputBlack,inputWhite,gamma,outputBlack,outputWhite));
            newColor.g = GetPixelLevel(newColor.g, LevelsIndividualParam(inputBlack,inputWhite,gamma,outputBlack,outputWhite));
            newColor.b = GetPixelLevel(newColor.b, LevelsIndividualParam(inputBlack,inputWhite,gamma,outputBlack,outputWhite));
            tgfx_FragColor = newColor;
        }
    )";

std::shared_ptr<tgfx::Image> LevelsIndividualFilter::Apply(std::shared_ptr<tgfx::Image> input,
                                                           RenderCache* cache, Effect* effect,
                                                           Frame layerFrame, tgfx::Point* offset) {
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
  auto filter = std::make_shared<LevelsIndividualFilter>(cache, param);
  return input->makeWithFilter(tgfx::ImageFilter::Runtime(filter), offset);
}

std::string LevelsIndividualFilter::onBuildFragmentShader() const {
  return FRAGMENT_SHADER;
}

std::vector<tgfx::BindingEntry> LevelsIndividualFilter::uniformBlocks() const {
  return {{"Args", 0}};
}

void LevelsIndividualFilter::onUpdateUniforms(tgfx::RenderPass* renderPass, tgfx::GPU* gpu,
                                              const std::vector<std::shared_ptr<tgfx::Texture>>&,
                                              const tgfx::Point&) const {
  constexpr int redChannel = 0;
  constexpr int greenChannel = 1;
  constexpr int blueChannel = 2;
  constexpr int globalChannel = 3;

  struct Uniforms {
    float inputBlack = 0.0f;
    float inputWhite = 0.0f;
    float gamma = 0.0f;
    float outputBlack = 0.0f;
    float outputWhite = 0.0f;
    float redInputBlack = 0.0f;
    float redInputWhite = 0.0f;
    float redGamma = 0.0f;
    float redOutputBlack = 0.0f;
    float redOutputWhite = 0.0f;
    float blueInputBlack = 0.0f;
    float blueInputWhite = 0.0f;
    float blueGamma = 0.0f;
    float blueOutputBlack = 0.0f;
    float blueOutputWhite = 0.0f;
    float greenInputBlack = 0.0f;
    float greenInputWhite = 0.0f;
    float greenGamma = 0.0f;
    float greenOutputBlack = 0.0f;
    float greenOutputWhite = 0.0f;
  };

  Uniforms uniforms = {};
  uniforms.inputBlack = param.inBlack[globalChannel];
  uniforms.inputWhite = param.inWhite[globalChannel];
  uniforms.gamma = param.gamma[globalChannel];
  uniforms.outputBlack = param.outBlack[globalChannel];
  uniforms.outputWhite = param.outWhite[globalChannel];

  uniforms.redInputBlack = param.inBlack[redChannel];
  uniforms.redInputWhite = param.inWhite[redChannel];
  uniforms.redGamma = param.gamma[redChannel];
  uniforms.redOutputBlack = param.outBlack[redChannel];
  uniforms.redOutputWhite = param.outWhite[redChannel];

  uniforms.greenInputBlack = param.inBlack[greenChannel];
  uniforms.greenInputWhite = param.inWhite[greenChannel];
  uniforms.greenGamma = param.gamma[greenChannel];
  uniforms.greenOutputBlack = param.outBlack[greenChannel];
  uniforms.greenOutputWhite = param.outWhite[greenChannel];

  uniforms.blueInputBlack = param.inBlack[blueChannel];
  uniforms.blueInputWhite = param.inWhite[blueChannel];
  uniforms.blueGamma = param.gamma[blueChannel];
  uniforms.blueOutputBlack = param.outBlack[blueChannel];
  uniforms.blueOutputWhite = param.outWhite[blueChannel];

  auto uniformBuffer = gpu->createBuffer(sizeof(Uniforms), tgfx::GPUBufferUsage::UNIFORM);
  if (uniformBuffer != nullptr) {
    auto* data = uniformBuffer->map();
    if (data != nullptr) {
      memcpy(data, &uniforms, sizeof(Uniforms));
      uniformBuffer->unmap();
      renderPass->setUniformBuffer(0, uniformBuffer, 0, sizeof(Uniforms));
    }
  }
}
}  // namespace pag
