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
class GlowBlurRuntimeFilter : public RuntimeFilter {
 public:
  GlowBlurRuntimeFilter(BlurDirection blurDirection, float blurOffset, float resizeRatio);

 protected:
  std::string onBuildVertexShader() const override;

  std::string onBuildFragmentShader() const override;

  void onUpdateUniforms(tgfx::RenderPass* renderPass, tgfx::GPU* gpu,
                        const std::vector<std::shared_ptr<tgfx::Texture>>& inputTextures,
                        const tgfx::Point& offset) const override;

  std::vector<tgfx::BindingEntry> uniformBlocks() const override;

  tgfx::Rect filterBounds(const tgfx::Rect& srcRect) const override;

 private:
  BlurDirection blurDirection = BlurDirection::Horizontal;
  float blurOffset = 0.0f;
  float resizeRatio = 1.0f;
};
}  // namespace pag
