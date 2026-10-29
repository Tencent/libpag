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
//  license is distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include "pagx/nodes/DisplacementMapFilter.h"
#include "tgfx/core/Image.h"
#include "tgfx/core/Rect.h"
#include "tgfx/layers/filters/LayerFilter.h"

namespace tgfx {
class Layer;
struct Point;
}  // namespace tgfx

namespace pagx {

// Layer filter for the PAGX DisplacementMapFilter node (spec 7.5). Replicates the legacy
// cross-layer pipeline: the referenced map layer's content renders offscreen each frame (its
// tgfx layer's recorded content drawn into a picture sized to the map layer's legacy
// getBounds), and the legacy displacement RuntimeFilter shader consumes it as an extra input.
// The target layer's own matrix and both layers' legacy getBounds sizes feed the shader's
// map-placement math, which is why the filter holds weak references to both runtime layers -
// the LayerBuilder resolves them after the composition's children are built (the map layer may
// convert after the target). The shader input is the target's FULL content re-rendered from its
// layer subtree (the legacy never clips the effect input to the visible region), re-anchored
// onto the framework input through the inputOrigin the filter pipeline reports. The
// effectOpacity compositing option has no node field and stays fully opaque.
class DisplacementMapLayerFilter : public tgfx::LayerFilter {
 public:
  static std::shared_ptr<DisplacementMapLayerFilter> Make(
      DisplacementMapSource useForHorizontalDisplacement, float maxHorizontalDisplacement,
      DisplacementMapSource useForVerticalDisplacement, float maxVerticalDisplacement,
      DisplacementMapBehavior displacementMapBehavior, bool edgeBehavior, bool expandOutput);

  void setTargetLayer(const std::weak_ptr<tgfx::Layer>& layer);
  void setMapLayer(const std::weak_ptr<tgfx::Layer>& layer);

  // The legacy measures both layers through pag::Layer::getBounds(), which returns the
  // referenced composition's dimensions for precompose layers - the LayerBuilder resolves those
  // at build time because the filter only sees the runtime tgfx layers (whose bounds are the
  // children union instead).
  void setTargetSize(float width, float height);
  void setDisplacementSize(float width, float height);

  float maxHorizontalDisplacement() const {
    return _maxHorizontalDisplacement;
  }

  void setMaxHorizontalDisplacement(float value);

  float maxVerticalDisplacement() const {
    return _maxVerticalDisplacement;
  }

  void setMaxVerticalDisplacement(float value);

  bool edgeBehavior() const {
    return _edgeBehavior;
  }

  void setEdgeBehavior(bool value);

  bool expandOutput() const {
    return _expandOutput;
  }

  void setExpandOutput(bool value);

  DisplacementMapBehavior displacementMapBehavior() const {
    return _displacementMapBehavior;
  }

  void setDisplacementMapBehavior(DisplacementMapBehavior value);

  DisplacementMapSource useForHorizontalDisplacement() const {
    return _useForHorizontalDisplacement;
  }

  void setUseForHorizontalDisplacement(DisplacementMapSource value);

  DisplacementMapSource useForVerticalDisplacement() const {
    return _useForVerticalDisplacement;
  }

  void setUseForVerticalDisplacement(DisplacementMapSource value);

  float effectOpacity() const {
    return _effectOpacity;
  }

  void setEffectOpacity(float value);

  bool enabled() const {
    return _enabled;
  }

  void setEnabled(bool value);

 protected:
  std::shared_ptr<tgfx::Image> onFilterImage(std::shared_ptr<tgfx::Image> input, float scale,
                                             const tgfx::Rect& contentBounds, tgfx::Point* offset,
                                             const tgfx::Point& inputOrigin) override;

  tgfx::Rect filterBounds(const tgfx::Rect& srcRect, float contentScale,
                          tgfx::MapDirection direction) override;

 private:
  // Renders the map layer's content into an image sized to the map layer's legacy getBounds,
  // mirroring the legacy GetDisplacementMapGraphic + Image::MakeFrom recording, including the
  // TileMap clip to the target's mapped content bounds.
  std::shared_ptr<tgfx::Image> makeMapImage(float* displacementWidth, float* displacementHeight);

  DisplacementMapSource _useForHorizontalDisplacement = DisplacementMapSource::Red;
  float _maxHorizontalDisplacement = 0.0f;
  DisplacementMapSource _useForVerticalDisplacement = DisplacementMapSource::Red;
  float _maxVerticalDisplacement = 0.0f;
  DisplacementMapBehavior _displacementMapBehavior = DisplacementMapBehavior::CenterMap;
  bool _edgeBehavior = false;
  bool _expandOutput = true;
  float _effectOpacity = 1.0f;
  bool _enabled = true;
  std::weak_ptr<tgfx::Layer> _targetLayer = {};
  std::weak_ptr<tgfx::Layer> _mapLayer = {};
  float _targetWidth = 0.0f;
  float _targetHeight = 0.0f;
  float _displacementWidth = 0.0f;
  float _displacementHeight = 0.0f;
};

}  // namespace pagx
