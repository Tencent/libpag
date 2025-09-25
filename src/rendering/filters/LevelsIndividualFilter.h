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

class LevelsIndividualUniforms : public Uniforms {
 public:
  LevelsIndividualUniforms(tgfx::Context* context, unsigned program);
  // Handle
  int inputBlackHandle;
  int inputWhiteHandle;
  int gammaHandle;
  int outputBlackHandle;
  int outputWhiteHandle;

  int redInputBlackHandle;
  int redInputWhiteHandle;
  int redGammaHandle;
  int redOutputBlackHandle;
  int redOutputWhiteHandle;

  int blueInputBlackHandle;
  int blueInputWhiteHandle;
  int blueGammaHandle;
  int blueOutputBlackHandle;
  int blueOutputWhiteHandle;

  int greenInputBlackHandle;
  int greenInputWhiteHandle;
  int greenGammaHandle;
  int greenOutputBlackHandle;
  int greenOutputWhiteHandle;
};

struct LevelsIndividualFilterParam {
  // 0 is red, 1 is green, 2 is blue, 3 is global
  float inBlack[4] = {};
  float inWhite[4] = {};
  float gamma[4] = {};
  float outBlack[4] = {};
  float outWhite[4] = {};
};

class LevelsIndividualFilter : public RuntimeFilter {
 public:
  DEFINE_RUNTIME_EFFECT_PROGRAM_ID

  static std::shared_ptr<tgfx::Image> Apply(std::shared_ptr<tgfx::Image> input, Effect* effect,
                                            Frame layerFrame, tgfx::Point* offset);

  explicit LevelsIndividualFilter(const LevelsIndividualFilterParam& param) : param(param) {
  }

 protected:
  std::string onBuildFragmentShader() const override;

  std::unique_ptr<Uniforms> onPrepareProgram(tgfx::Context* context,
                                             unsigned program) const override;

  void onUpdateParams(tgfx::Context* context, const RuntimeProgram* program,
                      const std::vector<tgfx::BackendTexture>&) const override;

 private:
  LevelsIndividualFilterParam param;
};
}  // namespace pag
