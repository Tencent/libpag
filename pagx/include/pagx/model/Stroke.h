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
#include "pagx/model/ColorSource.h"
#include "pagx/model/Element.h"
#include "pagx/model/types/BlendMode.h"
#include "pagx/model/types/LineCap.h"
#include "pagx/model/types/LineJoin.h"
#include "pagx/model/types/Placement.h"
#include "pagx/model/types/StrokeAlign.h"

namespace pagx {

/**
 * Stroke represents a stroke painter that outlines shapes with a solid color, gradient, or
 * pattern. It supports various line cap styles, join styles, dash patterns, and stroke alignment.
 */
class Stroke : public Element {
 public:
  /**
   * The stroke color as a string. Can be a hex color (e.g., "#FF0000"), a reference to a color
   * source (e.g., "#gradientId"), or empty if colorSource is used.
   */
  std::string color = {};

  /**
   * An inline color source node (SolidColor, LinearGradient, etc.) for complex strokes. If
   * provided, this takes precedence over the color string.
   */
  std::unique_ptr<ColorSource> colorSource = nullptr;

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
  Placement placement = Placement::Background;

  ElementType elementType() const override {
    return ElementType::Stroke;
  }

  NodeType type() const override {
    return NodeType::Stroke;
  }
};

}  // namespace pagx
