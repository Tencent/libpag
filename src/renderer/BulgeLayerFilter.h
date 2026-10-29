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

// Layer filter for the PAGX BulgeFilter node (spec 7.5). Reuses the legacy bulge RuntimeFilter
// (src/rendering/filters/BulgeFilter.cpp) so both pipelines warp identically; it runs cache-less
// because the PAGX layer path has no pag::RenderCache. The legacy renderer ignores taperRadius,
// so the field stays data-only here too. The bounds delegate to the legacy RuntimeFilter's
// ellipse-overflow expansion, which is self-contained given a rect and uv-normalized params.
class BulgeLayerFilter : public tgfx::LayerFilter {
 public:
  static std::shared_ptr<BulgeLayerFilter> Make(float horizontalRadius, float verticalRadius,
                                                Point bulgeCenter, float bulgeHeight, bool pinning);

  float horizontalRadius() const {
    return _horizontalRadius;
  }

  void setHorizontalRadius(float value);

  float verticalRadius() const {
    return _verticalRadius;
  }

  void setVerticalRadius(float value);

  float bulgeCenterX() const {
    return _bulgeCenter.x;
  }

  float bulgeCenterY() const {
    return _bulgeCenter.y;
  }

  void setBulgeCenter(float x, float y);

  float bulgeHeight() const {
    return _bulgeHeight;
  }

  void setBulgeHeight(float value);

  bool pinning() const {
    return _pinning;
  }

  void setPinning(bool value);

 protected:
  std::shared_ptr<tgfx::Image> onFilterImage(std::shared_ptr<tgfx::Image> input, float scale,
                                             const tgfx::Rect& contentBounds, tgfx::Point* offset,
                                             const tgfx::Point& inputOrigin) override;

  tgfx::Rect filterBounds(const tgfx::Rect& srcRect, float contentScale,
                          tgfx::MapDirection direction) override;

 private:
  float _horizontalRadius = 0.0f;
  float _verticalRadius = 0.0f;
  Point _bulgeCenter = {};
  float _bulgeHeight = 0.0f;
  bool _pinning = false;
};

}  // namespace pagx
