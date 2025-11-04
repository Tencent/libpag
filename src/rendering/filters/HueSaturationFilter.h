/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

class HueSaturationUniform : public Uniforms {
 public:
  HueSaturationUniform(tgfx::Context* context, unsigned program);

  int hueHandle = -1;
  int saturationHandle = -1;
  int lightnessHandle = -1;
  int colorizeHandle = -1;
  int colorizeHueHandle = -1;
  int colorizeSaturationHandle = -1;
  int colorizeLightnessHandle = -1;
};

class HueSaturationFilter : public RuntimeFilter {
 public:
  static std::shared_ptr<tgfx::Image> Apply(std::shared_ptr<tgfx::Image> input, Effect* effect,
                                            Frame layerFrame, tgfx::Point* offset);
  DEFINE_RUNTIME_EFFECT_PROGRAM_ID
  HueSaturationFilter(float hue, float saturation, float lightness, float colorize,
                      float colorizeHue, float colorizeSaturation, float colorizeLightness);

 protected:
  std::string onBuildFragmentShader() const override;

  std::unique_ptr<Uniforms> onPrepareProgram(tgfx::Context* context,
                                             unsigned program) const override;

  void onUpdateParams(tgfx::Context* context, const RuntimeProgram* program,
                      const std::vector<tgfx::BackendTexture>& sources) const override;

 private:
  float hue = 0.f;
  float saturation = 0.f;
  float lightness = 0.f;
  float colorize = 0.f;
  float colorizeHue = 0.f;
  float colorizeSaturation = 0.f;
  float colorizeLightness = 0.f;
};

}  // namespace pag
