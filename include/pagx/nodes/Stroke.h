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

#include <vector>
#include "pagx/nodes/ColorSource.h"
#include "pagx/nodes/Element.h"
#include "pagx/types/BlendMode.h"
#include "pagx/types/LayerPlacement.h"
#include "pagx/types/StrokeStyle.h"

namespace pagx {

/**
 * Stroke represents a stroke painter that outlines shapes with a solid color, gradient, or
 * pattern. It supports various line cap styles, join styles, dash patterns, and stroke alignment.
 * The color is specified through a ColorSource node (SolidColor, LinearGradient, etc.) or
 * a reference to a defined color source (e.g., "@gradientId").
 */
class Stroke : public Element {
 public:
  /**
   * The color source for this stroke. Can be a SolidColor, LinearGradient, RadialGradient,
   * ConicGradient, DiamondGradient, or ImagePattern.
   */
  ColorSource* color = nullptr;

  /**
   * The stroke width in pixels. The default value is 1.
   */
  float width = 1;

  /**
   * The opacity of the stroke, ranging from 0 (transparent) to 1 (opaque). The default value is 1.
   */
  float alpha = 1;

  /**
   * The blend mode used when compositing the stroke. The default value is Normal.
   */
  BlendMode blendMode = BlendMode::Normal;

  /**
   * The line cap style at the endpoints of open paths. The default value is Butt.
   */
  LineCap cap = LineCap::Butt;

  /**
   * The line join style at path corners. The default value is Miter.
   */
  LineJoin join = LineJoin::Miter;

  /**
   * The limit for miter joins before they are beveled. The default value is 4.
   */
  float miterLimit = 4;

  /**
   * The dash pattern as an array of dash and gap lengths. An empty array means a solid line.
   */
  std::vector<float> dashes = {};

  /**
   * The offset into the dash pattern. The default value is 0.
   */
  float dashOffset = 0;

  /**
   * Whether to scale the dash intervals so that the dash segments have the same length. The default
   * value is false.
   */
  bool dashAdaptive = false;

  /**
   * The alignment of the stroke relative to the path (Center, Inner, or Outer). The default value
   * is Center.
   */
  StrokeAlign align = StrokeAlign::Center;

  /**
   * The placement of the stroke relative to fills (Background or Foreground). The default value is
   * Background.
   */
  LayerPlacement placement = LayerPlacement::Background;

  NodeType nodeType() const override {
    return NodeType::Stroke;
  }

 private:
  Stroke() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
