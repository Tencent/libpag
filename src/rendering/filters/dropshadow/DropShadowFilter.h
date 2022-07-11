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

#include "DropShadowSpreadFilter.h"
#include "rendering/filters/LayerFilter.h"
#include "rendering/filters/gaussianblur/GaussianBlurFilter.h"
#include "rendering/filters/utils/FilterBuffer.h"

namespace pag {
class DropShadowFilter : public LayerFilter {
 public:
  explicit DropShadowFilter(DropShadowStyle* layerStyle);
  ~DropShadowFilter() override;

  bool initialize(tgfx::Context* context) override;

  void update(Frame frame, const tgfx::Rect& contentBounds, const tgfx::Rect& transformedBounds,
              const tgfx::Point& filterScale) override;

  void draw(tgfx::Context* context, const FilterSource* source,
            const FilterTarget* target) override;

 private:
  DropShadowStyle* layerStyle = nullptr;

  std::shared_ptr<FilterBuffer> spreadFilterBuffer = nullptr;
  std::shared_ptr<FilterBuffer> blurFilterBuffer = nullptr;

  FastBlurEffect* blurEffect = nullptr;
  GaussianBlurFilter* blurFilter = nullptr;
  DropShadowSpreadFilter* spreadFilter = nullptr;
  DropShadowSpreadFilter* spreadThickFilter = nullptr;

  tgfx::Color color = tgfx::Color::Black();
  tgfx::Point offset = {0.0f, 0.0f};
  tgfx::Rect filterContentBounds = {};
  tgfx::Rect filterNotFullSpreadBounds = {};
  float spread = 0.0f;
  float size = 0.0f;
  float expendSize = 0.0f;

  void updateParamModeNotSpread(Frame frame, const tgfx::Rect& contentBounds,
                                const tgfx::Rect& transformedBounds,
                                const tgfx::Point& filterScale);
  void updateParamModeNotFullSpread(Frame frame, const tgfx::Rect& contentBounds,
                                    const tgfx::Rect& transformedBounds,
                                    const tgfx::Point& filterScale);
  void updateParamModeFullSpread(Frame frame, const tgfx::Rect& contentBounds,
                                 const tgfx::Rect& transformedBounds,
                                 const tgfx::Point& filterScale);

  void onDrawModeNotSpread(tgfx::Context* context, const FilterSource* source,
                           const FilterTarget* target);
  void onDrawModeNotFullSpread(tgfx::Context* context, const FilterSource* source,
                               const FilterTarget* target);
  void onDrawModeFullSpread(tgfx::Context* context, const FilterSource* source,
                            const FilterTarget* target);
};
}  // namespace pag
