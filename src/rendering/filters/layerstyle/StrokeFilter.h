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
#include "LayerStyleFilter.h"
#include "rendering/filters/layerstyle/SolidStrokeFilter.h"
namespace pag {
class StrokeFilter : public LayerStyleFilter {
 public:
  StrokeFilter(StrokeStyle* layerStyle, RenderCache* cache);

  StrokeFilter(const StrokeFilter&) = delete;

  StrokeFilter(StrokeFilter&&) = delete;

  void update(Frame layerFrame, const tgfx::Point& filterScale,
              const tgfx::Point& sourceScale) override;

  bool draw(Canvas* canvas, std::shared_ptr<tgfx::Image> image) override;

 private:
  StrokeStyle* layerStyle = nullptr;
  SolidStrokeOption strokeOption;
  SolidStrokeMode mode = SolidStrokeMode::Normal;
  float alpha = 1.0f;
};
}  // namespace pag
