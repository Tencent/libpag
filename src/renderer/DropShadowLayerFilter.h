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
#include "tgfx/core/BlendMode.h"
#include "tgfx/core/Color.h"
#include "tgfx/core/ImageFilter.h"
#include "tgfx/core/MapDirection.h"
#include "tgfx/layers/filters/LayerFilter.h"

namespace tgfx {
class Image;
}

namespace pagx {

// Layer filter for the PAGX DropShadowStyle node (spec 7.4) when the style carries a spread: it
// replicates the legacy DropShadowFilter shader chain, splitting the stored blur radius into a
// solid core (grown through the SolidStroke runtime shader at the shadow offset) and the
// remaining blur falloff, and composites the shadow BEHIND the content (the legacy draws
// blow-position styles first and the source image last). The tgfx native DropShadowStyle cannot
// take the spread: it derives the core from the layer's vector contour, which text and group
// layers cannot provide, so the shadow would be skipped entirely; spread-less styles stay on
// that native binding.
class DropShadowLayerFilter : public tgfx::LayerFilter {
 public:
  float offsetX() const {
    return _offsetX;
  }

  void setOffsetX(float value);

  float offsetY() const {
    return _offsetY;
  }

  void setOffsetY(float value);

  float blurX() const {
    return _blurX;
  }

  void setBlurX(float value);

  float blurY() const {
    return _blurY;
  }

  void setBlurY(float value);

  float spread() const {
    return _spread;
  }

  void setSpread(float value);

  tgfx::Color color() const {
    return _color;
  }

  void setColor(tgfx::Color value);

  tgfx::BlendMode blendMode() const {
    return _blendMode;
  }

  void setBlendMode(tgfx::BlendMode value);

  bool showBehindLayer() const {
    return _showBehindLayer;
  }

  void setShowBehindLayer(bool value);

 protected:
  std::shared_ptr<tgfx::Image> onFilterImage(std::shared_ptr<tgfx::Image> input, float scale,
                                             const tgfx::Rect& contentBounds, tgfx::Point* offset,
                                             const tgfx::Point& inputOrigin) override;

  tgfx::Rect filterBounds(const tgfx::Rect& srcRect, float contentScale,
                          tgfx::MapDirection direction) override;

 private:
  template <typename T>
  void setDropShadowField(T& field, T value) {
    if (field == value) {
      return;
    }
    field = value;
    invalidateFilter();
  }

  // Builds the shadow-only image filter chain at the given layer-to-pixel scale. Returns null
  // when the parameters produce no shadow.
  std::shared_ptr<tgfx::ImageFilter> makeShadowFilter(float scale) const;

  // Node-space fields: offsets and blur radii in layer units (the blur is the pre-spread
  // radius), spread in the node's 0-100 percent space.
  float _offsetX = 0.0f;
  float _offsetY = 0.0f;
  float _blurX = 0.0f;
  float _blurY = 0.0f;
  float _spread = 0.0f;
  tgfx::Color _color = tgfx::Color::Black();
  tgfx::BlendMode _blendMode = tgfx::BlendMode::SrcOver;
  bool _showBehindLayer = true;
};

}  // namespace pagx
