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

#include "GaussianBlurFilterPass.h"
#include "rendering/filters/LayerFilter.h"
#include "rendering/filters/utils/FilterBuffer.h"

namespace pag {
class GaussianBlurFilter : public LayerFilter {
 public:
  explicit GaussianBlurFilter(Effect* effect);
  ~GaussianBlurFilter() override;

  bool initialize(tgfx::Context* context) override;

  void draw(tgfx::Context* context, const FilterSource* source,
            const FilterTarget* target) override;

  void update(Frame frame, const tgfx::Rect& contentBounds, const tgfx::Rect& transformedBounds,
              const tgfx::Point& filterScale) override;

 private:
  Effect* effect = nullptr;

  GaussianBlurFilterPass* downBlurPass = nullptr;
  GaussianBlurFilterPass* upBlurPass = nullptr;

  std::vector<std::shared_ptr<FilterBuffer>> bufferCache = {};
  PassBounds filtersBounds[BLUR_DEPTH_MAX * 2] = {};
  tgfx::Point filtersBoundsScale = {};

  BlurParam blurParam = {};

  void updateBlurParam(float blurriness);
  std::shared_ptr<FilterBuffer> getBuffer(tgfx::Context* context, int width, int height);
  void cacheBuffer(std::shared_ptr<FilterBuffer> buffer);
};
}  // namespace pag
