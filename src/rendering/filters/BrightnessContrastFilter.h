/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

namespace pag {
class BrightnessContrastUniforms : public Uniforms {
 public:
  BrightnessContrastUniforms(tgfx::Context* context, unsigned program);

  // Handle
  int brightnessBlocksHandle = -1;
  int contrastHandle = -1;
};

class BrightnessContrastRuntimeFilter : public RuntimeFilter {
 public:
  DEFINE_RUNTIME_EFFECT_TYPE
  BrightnessContrastRuntimeFilter(float brightness, float contrast)
      : RuntimeFilter(Type()), brightness(brightness), contrast(contrast) {
  }

  std::string onBuildFragmentShader() const override;

  std::unique_ptr<Uniforms> onPrepareProgram(tgfx::Context* context,
                                             unsigned program) const override;

  void onUpdateParams(tgfx::Context* context, const RuntimeProgram* program,
                      const std::vector<tgfx::BackendTexture>& sources) const override;

 private:
  float brightness = 0.f;
  float contrast = 0.f;
};

class BrightnessContrastFilter : public EffectFilter {
 public:
  explicit BrightnessContrastFilter(Effect* effect);
  ~BrightnessContrastFilter() override = default;

  void update(Frame layerFrame, const tgfx::Point& sourceScale) override;

 protected:
  std::shared_ptr<tgfx::RuntimeEffect> createRuntimeEffect() override;

 private:
  Effect* effect = nullptr;
  std::shared_ptr<BrightnessContrastRuntimeFilter> currentFilter;
};
}  // namespace pag
