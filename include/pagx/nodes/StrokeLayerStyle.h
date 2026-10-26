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
//  either express or implied. See the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "pagx/nodes/LayerStyle.h"
#include "pagx/types/BlendMode.h"
#include "pagx/types/Color.h"

namespace pagx {

enum class StrokePosition : uint8_t {
  Outside,
  Center,
  Inside,
};

/**
 * A stroke layer style (spec 7.4): paints an outline around the layer's alpha contour with the
 * given color and width. The runtime blend mode composites the stroke with the layer.
 */
class StrokeLayerStyle : public LayerStyle {
 public:
  Color color = {};
  float size = 3.0f;
  float alpha = 1.0f;
  StrokePosition position = StrokePosition::Outside;
  BlendMode blendMode = BlendMode::Normal;

  NodeType nodeType() const override {
    return NodeType::StrokeLayerStyle;
  }

 private:
  StrokeLayerStyle() = default;

  friend class PAGXDocument;
};

enum class GradientOverlayType : uint8_t {
  Linear,
  Radial,
};

/**
 * A gradient overlay layer style (spec 7.4): blends a gradient over the layer content. The
 * gradient itself is a shared Gradient resource node referenced by the style, so the existing
 * gradient encoding (stops, matrix, fitsToGeometry) carries over unchanged.
 */
class GradientOverlayStyle : public LayerStyle {
 public:
  /**
   * The gradient resource painted by the overlay. Null disables the style.
   */
  Gradient* gradient = nullptr;
  float alpha = 1.0f;
  float angle = 90.0f;
  GradientOverlayType overlayType = GradientOverlayType::Linear;
  bool reverse = false;
  bool alignWithLayer = true;
  float scale = 100.0f;
  float offsetX = 0.0f;
  float offsetY = 0.0f;
  BlendMode blendMode = BlendMode::Normal;

  NodeType nodeType() const override {
    return NodeType::GradientOverlayStyle;
  }

 private:
  GradientOverlayStyle() = default;

  friend class PAGXDocument;
};

/**
 * An outer glow layer style (spec 7.4): a soft halo around the layer contour. The first
 * version paints a solid-color glow; the gradient-driven variant arrives with its tgfx
 * implementation.
 */
class OuterGlowStyle : public LayerStyle {
 public:
  Color color = {};
  float size = 5.0f;
  float alpha = 0.75f;
  float spread = 0.0f;
  float range = 50.0f;
  BlendMode blendMode = BlendMode::Screen;

  NodeType nodeType() const override {
    return NodeType::OuterGlowStyle;
  }

 private:
  OuterGlowStyle() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
