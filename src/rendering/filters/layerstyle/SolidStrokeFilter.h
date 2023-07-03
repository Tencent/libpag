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

#include "rendering/filters/LayerFilter.h"

namespace pag {
enum class SolidStrokeMode { Normal, Thick };

struct SolidStrokeOption {
  Enum position = 0;
  Color color = Black;
  Opacity opacity = 0.0f;
  float spreadSize = 0.0f;
};

class SolidStrokeFilter : public LayerFilter {
 public:
  explicit SolidStrokeFilter(SolidStrokeMode mode);
  ~SolidStrokeFilter() override = default;
  
  void onUpdateOption(SolidStrokeOption option);
  
  void onUpdateOriginalTexture(const tgfx::GLTextureInfo* sampler);

 protected:
  std::string onBuildFragmentShader() override;

  void onPrepareProgram(tgfx::Context* context, unsigned program) override;

  void onUpdateParams(tgfx::Context* context, const tgfx::Rect& contentBounds,
                      const tgfx::Point& filterScale) override;
  
  std::vector<tgfx::Point> computeVertices(const tgfx::Rect& contentBounds,
                                           const tgfx::Rect& transformedBounds,
                                           const tgfx::Point& filterScale) override;

 private:
  SolidStrokeOption option;
  SolidStrokeMode styleMode;
  tgfx::GLTextureInfo originalSampler;

  int colorHandle = -1;
  int alphaHandle = -1;
  int sizeHandle = -1;
  int originalTextureHandle = -1;
  int isUseOriginalTextureHandle = -1;
  int isOutsideHandle = -1;
  int isCenterHandle = -1;
  int isInsideHandle = -1;
};
}  // namespace pag
