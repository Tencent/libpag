/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "AS IS" basis, without warranties OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include "tgfx/core/Image.h"
#include "tgfx/core/Rect.h"
#include "tgfx/layers/filters/LayerFilter.h"

namespace tgfx {
struct Point;
}

namespace pagx {

// Layer filter for the PAGX BrightnessContrastFilter node (spec 7.5). Reuses the legacy
// brightness/contrast RuntimeFilter shader (src/rendering/filters/BrightnessContrastFilter.cpp)
// so both pipelines adjust colors identically; it runs cache-less because the PAGX layer path
// has no pag::RenderCache. Values carry over raw (the legacy RuntimeFilter normalizes
// internally). The filter never changes bounds.
class BrightnessContrastLayerFilter : public tgfx::LayerFilter {
 public:
  static std::shared_ptr<BrightnessContrastLayerFilter> Make(float brightness, float contrast);

  float brightness() const {
    return _brightness;
  }

  void setBrightness(float value);

  float contrast() const {
    return _contrast;
  }

  void setContrast(float value);

 protected:
  std::shared_ptr<tgfx::Image> onFilterImage(std::shared_ptr<tgfx::Image> input, float scale,
                                             const tgfx::Rect& contentBounds, tgfx::Point* offset,
                                             const tgfx::Point& inputOrigin) override;

 private:
  float _brightness = 0.0f;
  float _contrast = 0.0f;
};

}  // namespace pagx
