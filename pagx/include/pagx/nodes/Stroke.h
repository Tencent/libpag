/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include <memory>
#include <string>
#include <vector>
#include "pagx/nodes/ColorSource.h"
#include "pagx/nodes/Element.h"
#include "pagx/nodes/BlendMode.h"
#include "pagx/nodes/LayerPlacement.h"

namespace pagx {

/**
 * Line cap styles that define the shape at the endpoints of open paths.
 */
enum class LineCap {
  /**
   * A flat cap that ends exactly at the path endpoint.
   */
  Butt,
  /**
   * A rounded cap that extends beyond the endpoint by half the stroke width.
   */
  Round,
  /**
   * A square cap that extends beyond the endpoint by half the stroke width.
   */
  Square
};

/**
 * Line join styles that define the shape at the corners of paths.
 */
enum class LineJoin {
  /**
   * A sharp join that extends to a point.
   */
  Miter,
  /**
   * A rounded join with a circular arc.
   */
  Round,
  /**
   * A beveled join that cuts off the corner.
   */
  Bevel
};

/**
 * Stroke alignment relative to the path.
 */
enum class StrokeAlign {
  /**
   * Stroke is centered on the path.
   */
  Center,
  /**
   * Stroke is drawn inside the path boundary.
   */
  Inside,
  /**
   * Stroke is drawn outside the path boundary.
   */
  Outside
};

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
   * ConicGradient, DiamondGradient, or ImagePattern. If null, uses the colorRef reference.
   */
  std::unique_ptr<ColorSource> color = nullptr;

  /**
   * A reference to a color source defined in Resources (e.g., "@gradientId").
   * Only used if color is null.
   */
  std::string colorRef = {};

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
};

}  // namespace pagx
