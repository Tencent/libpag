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

namespace pag {
class GradientOverlayFilter : public LayerStyleFilter {
 public:
  explicit GradientOverlayFilter(GradientOverlayStyle* layerStyle);

  void update(Frame layerFrame, const tgfx::Point& filterScale,
              const tgfx::Point& sourceScale) override;

  bool draw(Canvas* canvas, std::shared_ptr<tgfx::Image> source) override;

 private:
  GradientOverlayStyle* layerStyle = nullptr;
  BlendMode blendMode = BlendMode::Normal;
  Opacity opacity = 255;
  GradientColorHandle colors = nullptr;
  float angle = 0;
  GradientFillType gradStyle = GradientFillType::Linear;
  bool reverse = false;
  float scale = 1.0;
  Point offset = Point::Zero();
  tgfx::Point _filterScale = tgfx::Point::Make(1.0f, 1.0f);
};
}  // namespace pag
