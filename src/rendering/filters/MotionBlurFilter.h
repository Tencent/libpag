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

#pragma once

#include <array>
#include "EffectFilter.h"

namespace pag {

class MotionBlurUniforms : public Uniforms {
 public:
  MotionBlurUniforms(tgfx::Context* context, unsigned program) : Uniforms(context, program) {
    auto gl = tgfx::GLFunctions::Get(context);
    prevTransformHandle = gl->getUniformLocation(program, "uPrevTransform");
    transformHandle = gl->getUniformLocation(program, "uTransform");
    velCenterHandle = gl->getUniformLocation(program, "uVelCenter");
    maxDistanceHandle = gl->getUniformLocation(program, "maxDistance");
  }

  int prevTransformHandle = 0;
  int transformHandle = 0;
  int velCenterHandle = 0;
  int maxDistanceHandle = 0;
};

class MotionBlurRuntimeFilter : public RuntimeFilter {
 public:
  DEFINE_RUNTIME_EFFECT_TYPE

  MotionBlurRuntimeFilter(const std::array<float, 9>& preMatrix,
                          const std::array<float, 9>& curMatrix)
      : RuntimeFilter(Type()), _previousMatrix(preMatrix), _currentMatrix(curMatrix) {
  }
  ~MotionBlurRuntimeFilter() override = default;

  tgfx::Rect filterBounds(const tgfx::Rect& srcRect) const override;

 protected:
  std::string onBuildVertexShader() const override;

  std::string onBuildFragmentShader() const override;

  std::unique_ptr<Uniforms> onPrepareProgram(tgfx::Context* context,
                                             unsigned program) const override;

  void onUpdateParams(tgfx::Context* context, const RuntimeProgram* program,
                      const std::vector<tgfx::BackendTexture>&) const override;

  std::vector<float> computeVertices(const std::vector<tgfx::BackendTexture>& sources,
                                     const tgfx::BackendRenderTarget& target,
                                     const tgfx::Point& offset) const override;

 private:
  std::array<float, 9> _previousMatrix = {};
  std::array<float, 9> _currentMatrix = {};
};

class MotionBlurFilter : public EffectFilter {
 public:
  static void TransformBounds(tgfx::Rect* contentBounds, Layer* layer, Frame layerFrame);

  static bool ShouldSkipFilter(Layer* layer, Frame layerFrame);

  explicit MotionBlurFilter(Layer* layer) : layer(layer) {
  }

  void update(Frame layerFrame, const tgfx::Point& sourceScale) override;

 protected:
  std::shared_ptr<tgfx::RuntimeEffect> createRuntimeEffect() override;

 private:
  Layer* layer = nullptr;
  std::array<float, 9> previousGLMatrix = {};
  std::array<float, 9> currentGLMatrix = {};
};
}  // namespace pag
