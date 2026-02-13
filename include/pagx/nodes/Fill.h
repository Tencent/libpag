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
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "pagx/nodes/ColorSource.h"
#include "pagx/nodes/Element.h"
#include "pagx/types/BlendMode.h"
#include "pagx/types/FillRule.h"
#include "pagx/types/LayerPlacement.h"

namespace pagx {

/**
 * Fill represents a fill painter that fills shapes with a solid color, gradient, or pattern.
 * The color is specified through a ColorSource node (SolidColor, LinearGradient, etc.) or
 * a reference to a defined color source (e.g., "@gradientId").
 */
class Fill : public Element {
 public:
  /**
   * The color source for this fill. Can be a SolidColor, LinearGradient, RadialGradient,
   * ConicGradient, DiamondGradient, or ImagePattern.
   */
  ColorSource* color = nullptr;

  /**
   * The opacity of the fill, ranging from 0 (transparent) to 1 (opaque). The default value is 1.
   */
  float alpha = 1.0f;

  /**
   * The blend mode used when compositing the fill. The default value is Normal.
   */
  BlendMode blendMode = BlendMode::Normal;

  /**
   * The fill rule that determines the interior of self-intersecting paths. The default value is
   * Winding.
   */
  FillRule fillRule = FillRule::Winding;

  /**
   * The placement of the fill relative to strokes (Background or Foreground). The default value is
   * Background.
   */
  LayerPlacement placement = LayerPlacement::Background;

  NodeType nodeType() const override {
    return NodeType::Fill;
  }

 private:
  Fill() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
