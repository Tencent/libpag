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
#include "pagx/types/Point.h"
#include "tgfx/core/Image.h"
#include "tgfx/core/Rect.h"
#include "tgfx/layers/filters/LayerFilter.h"

namespace tgfx {
struct Point;
}

namespace pagx {

// Layer filter for the PAGX RadialBlurFilter node (spec 7.5). Reuses the legacy radial blur
// RuntimeFilter shader (src/rendering/filters/RadialBlurFilter.cpp) so both pipelines sample
// the identical 16-tap ray; it runs cache-less because the PAGX layer path has no
// pag::RenderCache. The legacy shader ignores the mode (Spin/Zoom) and antialias flags - it
// always applies the zoom-style ray blur - so this filter keeps the fields for data fidelity
// but likewise ignores them. The filter never changes bounds.
class RadialBlurLayerFilter : public tgfx::LayerFilter {
 public:
  static std::shared_ptr<RadialBlurLayerFilter> Make(float amount, Point center);

  float amount() const {
    return _amount;
  }

  void setAmount(float value);

  float centerX() const {
    return _center.x;
  }

  float centerY() const {
    return _center.y;
  }

  void setCenter(float x, float y);

 protected:
  std::shared_ptr<tgfx::Image> onFilterImage(std::shared_ptr<tgfx::Image> input, float scale,
                                             const tgfx::Rect& contentBounds, tgfx::Point* offset,
                                             const tgfx::Point& inputOrigin) override;

 private:
  float _amount = 0.0f;
  Point _center = {};
};

}  // namespace pagx
