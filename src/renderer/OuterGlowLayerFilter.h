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

// Layer filter for the PAGX OuterGlowStyle node (spec 7.4). Replicates the legacy
// OuterGlowFilter shader chain: the spread fraction of the size dilates the content silhouette
// through the SolidStroke runtime shader and the remainder blurs it through
// ImageFilter::DropShadowOnly, producing a colored halo that is composited BEHIND the content
// (the legacy draws blow-position styles first and the source image last). tgfx has no native
// outer glow style, so the style rides the layer filter chain, appended after the node filters
// to mirror the legacy FilterRenderer order (styles apply over the effect-processed content).
class OuterGlowLayerFilter : public tgfx::LayerFilter {
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

  float spread() const {
    return _spread;
  }

  void setSpread(float value);

  float range() const {
    return _range;
  }

  void setRange(float value);

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
  void setOuterGlowField(T& field, T value) {
    if (field == value) {
      return;
    }
    field = value;
    invalidateFilter();
  }

  // Builds the halo-only image filter chain at the given layer-to-pixel scale. Returns null
  // when the parameters produce no halo (the caller keeps the input untouched).
  std::shared_ptr<tgfx::ImageFilter> makeHaloFilter(float scale) const;

  // Node-space fields: size in layer units, spread/range in the node's 0-100 percent space.
  tgfx::Color _color = tgfx::Color::Black();
  float _size = 5.0f;
  float _alpha = 0.75f;
  float _spread = 0.0f;
  float _range = 50.0f;
  tgfx::BlendMode _blendMode = tgfx::BlendMode::SrcOver;
};

}  // namespace pagx
