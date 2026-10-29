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
//  license is distributed on an "AS IS" basis, without warranties or conditions of any kind,
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

// Layer filter for the PAGX HueSaturationFilter node (spec 7.5). Reuses the legacy
// hue/saturation RuntimeFilter shader (src/rendering/filters/HueSaturationFilter.cpp) so both
// pipelines adjust colors identically in HSV space; it runs cache-less because the PAGX layer
// path has no pag::RenderCache. Values carry over raw (degrees/percent - the legacy RuntimeFilter
// normalizes internally). The filter never changes bounds.
class HueSaturationLayerFilter : public tgfx::LayerFilter {
 public:
  static std::shared_ptr<HueSaturationLayerFilter> Make(float hue, float saturation,
                                                        float lightness, bool colorize,
                                                        float colorizeHue, float colorizeSaturation,
                                                        float colorizeLightness);

  float hue() const {
    return _hue;
  }

  void setHue(float value);

  float saturation() const {
    return _saturation;
  }

  void setSaturation(float value);

  float lightness() const {
    return _lightness;
  }

  void setLightness(float value);

  bool colorize() const {
    return _colorize;
  }

  void setColorize(bool value);

  float colorizeHue() const {
    return _colorizeHue;
  }

  void setColorizeHue(float value);

  float colorizeSaturation() const {
    return _colorizeSaturation;
  }

  void setColorizeSaturation(float value);

  float colorizeLightness() const {
    return _colorizeLightness;
  }

  void setColorizeLightness(float value);

 protected:
  std::shared_ptr<tgfx::Image> onFilterImage(std::shared_ptr<tgfx::Image> input, float scale,
                                             const tgfx::Rect& contentBounds, tgfx::Point* offset,
                                             const tgfx::Point& inputOrigin) override;

 private:
  float _hue = 0.0f;
  float _saturation = 0.0f;
  float _lightness = 0.0f;
  bool _colorize = false;
  float _colorizeHue = 0.0f;
  float _colorizeSaturation = 0.0f;
  float _colorizeLightness = 0.0f;
};

}  // namespace pagx
