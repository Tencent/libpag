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
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include "pagx/nodes/StrokeLayerStyle.h"
#include "tgfx/core/BlendMode.h"
#include "tgfx/core/Color.h"
#include "tgfx/core/ImageFilter.h"
#include "tgfx/core/MapDirection.h"
#include "tgfx/layers/filters/LayerFilter.h"

namespace tgfx {
class Image;
}

namespace pagx {

// Layer filter for the PAGX StrokeLayerStyle node (spec 7.4). Replicates the legacy
// StrokeFilter shader chain: the content alpha (Outside) or its detected contour band
// (Center/Inside, scaled by 0.4/0.8) is dilated through the SolidStroke runtime shader with the
// original image preserved inside the stroke, and the merged output REPLACES the content (the
// legacy draws above-position styles instead of the source image) drawn as one image at the
// style opacity. tgfx has no native stroke style, so the style rides the layer filter chain,
// appended after the node filters to mirror the legacy FilterRenderer order.
class StrokeLayerFilter : public tgfx::LayerFilter {
 public:
  tgfx::Color color() const {
    return _color;
  }

  void setColor(tgfx::Color value);

  float size() const {
    return _size;
  }

  void setSize(float value);

  float alpha() const {
    return _alpha;
  }

  void setAlpha(float value);

  StrokePosition position() const {
    return _position;
  }

  void setPosition(StrokePosition value);

  tgfx::BlendMode blendMode() const {
    return _blendMode;
  }

  void setBlendMode(tgfx::BlendMode value);

 protected:
  std::shared_ptr<tgfx::Image> onFilterImage(std::shared_ptr<tgfx::Image> input, float scale,
                                             const tgfx::Rect& contentBounds, tgfx::Point* offset,
                                             const tgfx::Point& inputOrigin) override;

  tgfx::Rect filterBounds(const tgfx::Rect& srcRect, float contentScale,
                          tgfx::MapDirection direction) override;

 private:
  template <typename T>
  void setStrokeField(T& field, T value) {
    if (field == value) {
      return;
    }
    field = value;
    invalidateFilter();
  }

  // Builds the stroke image filter chain at the given layer-to-pixel scale. The original image
  // feeds the Center/Inside shader's content-preservation sampler; returns null when the
  // parameters produce no stroke.
  std::shared_ptr<tgfx::ImageFilter> makeStrokeFilter(
      float scale, const std::shared_ptr<tgfx::Image>& original) const;

  // Node-space fields: size in layer units.
  tgfx::Color _color = tgfx::Color::Black();
  float _size = 3.0f;
  float _alpha = 1.0f;
  StrokePosition _position = StrokePosition::Outside;
  tgfx::BlendMode _blendMode = tgfx::BlendMode::SrcOver;
};

}  // namespace pagx
