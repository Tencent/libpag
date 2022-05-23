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

#include "GaussianBlurDefines.h"
#include "rendering/filters/LayerFilter.h"

namespace pag {
class GaussianBlurFilterPass : public LayerFilter {
 public:
  explicit GaussianBlurFilterPass(BlurOptions options);
  ~GaussianBlurFilterPass() override = default;

  void updateParams(float blurValue, float scale, bool isExpendBoundsValue);

 protected:
  std::string onBuildFragmentShader() override;

  void onPrepareProgram(tgfx::Context* context, unsigned program) override;

  void onUpdateParams(tgfx::Context* context, const tgfx::Rect& contentBounds,
                      const tgfx::Point& filterScale) override;

  std::vector<tgfx::Point> computeVertices(const tgfx::Rect& contentBounds,
                                           const tgfx::Rect& transformedBounds,
                                           const tgfx::Point& filterScale) override;

 private:
  int stepHandle = -1;
  int offsetHandle = -1;

  BlurOptions options = BlurOptions::None;
  float blurriness = 0.0;
  float scale = 1.0;
  bool isExpendBounds = false;
};
}  // namespace pag
