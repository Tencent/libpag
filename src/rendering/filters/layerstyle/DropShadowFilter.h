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

#include "SolidStrokeFilter.h"
#include "rendering/filters/LayerFilter.h"
#include "rendering/filters/utils/FilterBuffer.h"

namespace pag {
class DropShadowFilter : public LayerFilter {
 public:
  explicit DropShadowFilter(DropShadowStyle* layerStyle);

  DropShadowFilter(const DropShadowFilter&) = delete;

  DropShadowFilter(DropShadowFilter&&) = delete;

  ~DropShadowFilter() override;

  bool initialize(tgfx::Context* context) override;

  void update(Frame frame, const tgfx::Rect& contentBounds, const tgfx::Rect& transformedBounds,
              const tgfx::Point& filterScale) override;

  void draw(tgfx::Context* context, const FilterSource* source,
            const FilterTarget* target) override;

 private:
  void updateParamModeNotFullSpread(const tgfx::Rect& contentBounds);
  void updateParamModeFullSpread(const tgfx::Rect& contentBounds);

  void onDrawModeNotSpread(tgfx::Context* context, const FilterSource* source,
                           const FilterTarget* target);
  void onDrawModeNotFullSpread(tgfx::Context* context, const FilterSource* source,
                               const FilterTarget* target);
  void onDrawModeFullSpread(tgfx::Context* context, const FilterSource* source,
                            const FilterTarget* target);

  DropShadowStyle* layerStyle = nullptr;

  std::shared_ptr<FilterBuffer> solidStrokeFilterBuffer = nullptr;

  SolidStrokeOption strokeOption;
  SolidStrokeFilter* strokeFilter = nullptr;
  SolidStrokeFilter* strokeThickFilter = nullptr;

  tgfx::Color color = tgfx::Color::Black();
  float spread = 0.f;
  float spreadSize = 0.f;
  float blurXSize = 0.f;
  float blurYSize = 0.f;
  float offsetX = 0.f;
  float offsetY = 0.f;
  std::vector<tgfx::Rect> filtersBounds = {};
};
}  // namespace pag
