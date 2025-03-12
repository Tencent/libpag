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

#include "EffectFilter.h"
#include "RuntimeFilter.h"

namespace pag {

class RadialBlurUniforms : public Uniforms {
 public:
  explicit RadialBlurUniforms(tgfx::Context* context, unsigned program)
      : Uniforms(context, program) {
    auto gl = tgfx::GLFunctions::Get(context);
    amountHandle = gl->getUniformLocation(program, "uAmount");
    centerHandle = gl->getUniformLocation(program, "uCenter");
  }
  int amountHandle = -1;
  int centerHandle = -1;
};

class RadialBlurRuntimeFilter : public RuntimeFilter {
  DEFINE_RUNTIME_EFFECT_TYPE;

 public:
  explicit RadialBlurRuntimeFilter(double amount, const tgfx::Point& center)
      : RuntimeFilter(Type()), amount(amount), center(center) {
  }
  ~RadialBlurRuntimeFilter() override = default;

 protected:
  std::string onBuildFragmentShader() const override;

  std::unique_ptr<Uniforms> onPrepareProgram(tgfx::Context* context,
                                             unsigned program) const override;

  void onUpdateParams(tgfx::Context* context, const RuntimeProgram* program,
                      const std::vector<tgfx::BackendTexture>& sources) const override;

 private:
  double amount = 0.f;
  tgfx::Point center = tgfx::Point::Zero();
};

class RadialBlurFilter : public EffectFilter {
 public:
  explicit RadialBlurFilter(Effect* effect);

  void update(Frame layerFrame, const tgfx::Point& sourceScale) override;

 protected:
  std::shared_ptr<tgfx::RuntimeEffect> createRuntimeEffect() override;

 private:
  Effect* effect = nullptr;
  double amount = 0.f;
  tgfx::Point center = tgfx::Point::Zero();
};

}  // namespace pag
