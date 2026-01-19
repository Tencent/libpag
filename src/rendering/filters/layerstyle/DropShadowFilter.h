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

#include "LayerStyleFilter.h"
#include "rendering/filters/layerstyle/SolidStrokeFilter.h"

namespace pag {
class DropShadowFilter : public LayerStyleFilter {
 public:
  DropShadowFilter(DropShadowStyle* layerStyle, RenderCache* cache);

  DropShadowFilter(const DropShadowFilter&) = delete;

  DropShadowFilter(DropShadowFilter&&) = delete;

  void update(Frame layerFrame, const tgfx::Point& filterScale,
              const tgfx::Point& sourceScale) override;

  bool draw(Canvas* canvas, std::shared_ptr<tgfx::Image> image) override;

 private:
  std::shared_ptr<tgfx::ImageFilter> getStrokeFilter() const;

  std::shared_ptr<tgfx::ImageFilter> getDropShadowFilter(float offsetX, float offsetY) const;

  DropShadowStyle* layerStyle = nullptr;
  tgfx::Color color = tgfx::Color::Black();
  float alpha = 1.0f;
  SolidStrokeMode mode = SolidStrokeMode::Normal;
  float sizeX = 0;
  float sizeY = 0;
  Percent spread = 0;
  float offsetX = 0;
  float offsetY = 0;
};
}  // namespace pag
