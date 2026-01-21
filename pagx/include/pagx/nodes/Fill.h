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
#include "pagx/nodes/ColorSource.h"
#include "pagx/nodes/VectorElement.h"
#include "pagx/types/BlendMode.h"
#include "pagx/types/FillRule.h"
#include "pagx/types/Placement.h"

namespace pagx {

/**
 * Fill represents a fill painter that fills shapes with a solid color, gradient, or pattern. The
 * color can be specified as a simple color string (e.g., "#FF0000"), a reference to a defined
 * color source (e.g., "#gradientId"), or an inline ColorSource node.
 */
class Fill : public VectorElement {
 public:
  /**
   * The fill color as a string. Can be a hex color (e.g., "#FF0000"), a reference to a color
   * source (e.g., "#gradientId"), or empty if colorSource is used.
   */
  std::string color = {};

  /**
   * An inline color source node (SolidColor, LinearGradient, etc.) for complex fills. If provided,
   * this takes precedence over the color string.
   */
  std::unique_ptr<Node> colorSource = nullptr;

  /**
   * The opacity of the fill, ranging from 0 (transparent) to 1 (opaque). The default value is 1.
   */
  float alpha = 1;

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
  Placement placement = Placement::Background;

  NodeType type() const override {
    return NodeType::Fill;
  }

  VectorElementType vectorElementType() const override {
    return VectorElementType::Fill;
  }
};

}  // namespace pagx
