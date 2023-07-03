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

#include "SolidStrokeFilter.h"
#include "AlphaEdgeDetectFilter.h"
#include "rendering/filters/LayerFilter.h"
#include "rendering/filters/utils/FilterBuffer.h"

namespace pag {
class StrokeFilter : public LayerFilter {
 public:
  explicit StrokeFilter(StrokeStyle* layerStyle);

  StrokeFilter(const StrokeFilter&) = delete;

  StrokeFilter(StrokeFilter&&) = delete;

  ~StrokeFilter() override;

  bool initialize(tgfx::Context* context) override;

  void update(Frame frame, const tgfx::Rect& contentBounds, const tgfx::Rect& transformedBounds,
              const tgfx::Point& filterScale) override;

  void draw(tgfx::Context* context, const FilterSource* source,
            const FilterTarget* target) override;

 private:
  void onDrawPositionOutside(tgfx::Context* context, const FilterSource* source,
                             const FilterTarget* target);
  void onDrawPositionInsideOrCenter(tgfx::Context* context, const FilterSource* source,
                            const FilterTarget* target);

  StrokeStyle* layerStyle = nullptr;

  std::shared_ptr<FilterBuffer> alphaEdgeDetectFilterBuffer = nullptr;

  Enum strokePosition;
  
  SolidStrokeOption strokeOption;
  SolidStrokeFilter* strokeFilter = nullptr;
  SolidStrokeFilter* strokeThickFilter = nullptr;
  AlphaEdgeDetectFilter* alphaEdgeDetectFilter = nullptr;

  tgfx::Color color = tgfx::Color::Black();
  float spreadSize = 0.f;
};
}  // namespace pag
