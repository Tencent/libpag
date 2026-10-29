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
//  or required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>
#include "pagx/nodes/StrokeLayerStyle.h"
#include "tgfx/core/BlendMode.h"
#include "tgfx/core/Color.h"
#include "tgfx/layers/filters/LayerFilter.h"

namespace pagx {

// Layer filter for the PAGX GradientOverlayStyle node (spec 7.4). Replicates the legacy
// GradientOverlayFilter shader chain: the gradient (color stops from the style's Gradient
// resource; geometry recomputed per frame from angle/scale/offset, since the legacy derives it
// from the content image size rather than stored points) is masked to the layer content's alpha
// and composited over the content with the style's blend mode. tgfx has no native gradient
// overlay style, so the style rides the layer filter chain, appended after the node filters to
// mirror the legacy FilterRenderer order (styles apply over the effect-processed content).
class GradientOverlayLayerFilter : public tgfx::LayerFilter {
 public:
  float alpha() const {
    return _alpha;
  }

  void setAlpha(float value);

  float angle() const {
    return _angle;
  }

  void setAngle(float value);

  GradientOverlayType overlayType() const {
    return _overlayType;
  }

  void setOverlayType(GradientOverlayType value);

  bool reverse() const {
    return _reverse;
  }

  void setReverse(bool value);

  float scale() const {
    return _scale;
  }

  void setScale(float value);

  float offsetX() const {
    return _offsetX;
  }

  void setOffsetX(float value);

  float offsetY() const {
    return _offsetY;
  }

  void setOffsetY(float value);

  tgfx::BlendMode blendMode() const {
    return _blendMode;
  }

  void setBlendMode(tgfx::BlendMode value);

  // Replaces the gradient color stops (parallel color and position arrays, positions in 0-1).
  void setGradientStops(std::vector<tgfx::Color> colors, std::vector<float> positions);

 protected:
  std::shared_ptr<tgfx::Image> onFilterImage(std::shared_ptr<tgfx::Image> input, float scale,
                                             const tgfx::Rect& contentBounds, tgfx::Point* offset,
                                             const tgfx::Point& inputOrigin) override;

 private:
  template <typename T>
  void setGradientOverlayField(T& field, T value) {
    if (field == value) {
      return;
    }
    field = value;
    invalidateFilter();
  }

  float _alpha = 1.0f;
  float _angle = 90.0f;
  GradientOverlayType _overlayType = GradientOverlayType::Linear;
  bool _reverse = false;
  float _scale = 100.0f;
  float _offsetX = 0.0f;
  float _offsetY = 0.0f;
  tgfx::BlendMode _blendMode = tgfx::BlendMode::SrcOver;
  std::vector<tgfx::Color> _colors = {};
  std::vector<float> _positions = {};
};

}  // namespace pagx
