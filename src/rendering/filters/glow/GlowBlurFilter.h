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

#include "rendering/filters/RuntimeFilter.h"
#include "rendering/filters/utils/BlurTypes.h"

namespace pag {
class GlowBlurUniforms : public Uniforms {
 public:
  explicit GlowBlurUniforms(tgfx::Context* context, unsigned program);

  int textureOffsetHHandle = -1;
  int textureOffsetVHandle = -1;
};

class GlowBlurRuntimeFilter : public RuntimeFilter {
 public:
  DEFINE_RUNTIME_EFFECT_PROGRAM_ID
  GlowBlurRuntimeFilter(BlurDirection blurDirection, float blurOffset, float resizeRatio);

 protected:
  std::string onBuildVertexShader() const override;

  std::string onBuildFragmentShader() const override;

  std::unique_ptr<Uniforms> onPrepareProgram(tgfx::Context* context,
                                             unsigned program) const override;

  void onUpdateParams(tgfx::Context* context, const RuntimeProgram* program,
                      const std::vector<tgfx::BackendTexture>&) const override;

  tgfx::Rect filterBounds(const tgfx::Rect& srcRect) const override;

 private:
  BlurDirection blurDirection = BlurDirection::Horizontal;
  float blurOffset = 0.0f;
  float resizeRatio = 1.0f;
};
}  // namespace pag
