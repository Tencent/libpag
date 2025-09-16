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

#pragma once

#include "RuntimeFilter.h"
#include "pag/file.h"
#include "tgfx/core/Size.h"

namespace pag {
class RenderCache;

class DisplacementMapEffectUniforms : public Uniforms {
 public:
  DisplacementMapEffectUniforms(tgfx::Context* context, unsigned program)
      : Uniforms(context, program) {
    auto gl = tgfx::GLFunctions::Get(context);
    mapTextureHandle = gl->getUniformLocation(program, "mapTexture");
    flagsHandle = gl->getUniformLocation(program, "uFlags");
    inputMatrixHandle = gl->getUniformLocation(program, "uInputMatrix");
    mapMatrixHandle = gl->getUniformLocation(program, "uMapMatrix");
    inputSizeHandle = gl->getUniformLocation(program, "uInputSize");
    selectorMatrixRGBAHandle = gl->getUniformLocation(program, "uSelectorMatrixRGBA");
    selectorMatrixHSLAHandle = gl->getUniformLocation(program, "uSelectorMatrixHSLA");
    selectorOffsetHandle = gl->getUniformLocation(program, "uSelectorOffset");
    effectOpacityHandle = gl->getUniformLocation(program, "uEffectOpacity");
  }

  int flagsHandle = 0;
  int inputMatrixHandle = 0;
  int mapMatrixHandle = 0;
  int inputSizeHandle = 0;
  int selectorMatrixRGBAHandle = 0;
  int selectorMatrixHSLAHandle = 0;
  int selectorOffsetHandle = 0;
  int mapTextureHandle = 0;
  int effectOpacityHandle = 0;
};

class DisplacementMapFilter : public RuntimeFilter {
 public:
  DEFINE_RUNTIME_EFFECT_PROGRAM_ID

  static std::shared_ptr<tgfx::Image> Apply(std::shared_ptr<tgfx::Image> input, Effect* effect,
                                            Layer* layer, RenderCache* cache,
                                            const tgfx::Matrix& layerMatrix, Frame layerFrame,
                                            const tgfx::Rect& contentBounds, tgfx::Point* offset);

  DisplacementMapFilter(DisplacementMapSource useForHorizontalDisplacement,
                        float maxHorizontalDisplacement,
                        DisplacementMapSource useForVerticalDisplacement,
                        float maxVerticalDisplacement,
                        DisplacementMapBehavior displacementMapBehavior, bool edgeBehavior,
                        bool expandOutput, float effectOpacity, tgfx::Matrix layerMatrix,
                        tgfx::Size size, tgfx::Size displacementSize, tgfx::Rect contentBounds,
                        std::shared_ptr<tgfx::Image> sourceImage)
      : RuntimeFilter({std::move(sourceImage)}),
        useForHorizontalDisplacement(useForHorizontalDisplacement),
        maxHorizontalDisplacement(maxHorizontalDisplacement),
        useForVerticalDisplacement(useForVerticalDisplacement),
        maxVerticalDisplacement(maxVerticalDisplacement),
        displacementMapBehavior(displacementMapBehavior), edgeBehavior(edgeBehavior),
        expandOutput(expandOutput), effectOpacity(effectOpacity), layerMatrix(layerMatrix),
        size(size), displacementSize(displacementSize), contentBounds(contentBounds) {
  }

 protected:
  std::string onBuildFragmentShader() const override;
  std::unique_ptr<Uniforms> onPrepareProgram(tgfx::Context* context,
                                             unsigned program) const override;

  void onUpdateParams(tgfx::Context* context, const RuntimeProgram* program,
                      const std::vector<tgfx::BackendTexture>& sources) const override;

  tgfx::Rect filterBounds(const tgfx::Rect& srcRect) const override;

  DisplacementMapSource useForHorizontalDisplacement = DisplacementMapSource::Red;
  float maxHorizontalDisplacement = 0.f;
  DisplacementMapSource useForVerticalDisplacement = DisplacementMapSource::Red;
  float maxVerticalDisplacement = 0.f;
  DisplacementMapBehavior displacementMapBehavior = DisplacementMapBehavior::CenterMap;
  bool edgeBehavior = false;
  bool expandOutput = true;
  float effectOpacity = 1.f;
  tgfx::Matrix layerMatrix = tgfx::Matrix::I();
  tgfx::Size size = {0, 0};
  tgfx::Size displacementSize = {0, 0};
  tgfx::Rect contentBounds = {0, 0, 0, 0};
};

}  // namespace pag
