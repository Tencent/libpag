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

class RadialBlurFilter : public RuntimeFilter {
 public:
  DEFINE_RUNTIME_EFFECT_PROGRAM_ID;

  static std::shared_ptr<tgfx::Image> Apply(std::shared_ptr<tgfx::Image> input, Effect* effect,
                                            Frame layerFrame, const tgfx::Rect& contentBounds,
                                            tgfx::Point* offset);

  explicit RadialBlurFilter(double amount, const tgfx::Point& center)
      : amount(amount), center(center) {
  }
  ~RadialBlurFilter() override = default;

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

}  // namespace pag
