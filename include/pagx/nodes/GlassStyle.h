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

#include "pagx/nodes/LayerStyle.h"

namespace pagx {

/**
 * A glass layer style that simulates the physical behavior of light passing through a glass
 * surface, producing refraction, chromatic dispersion, frosted blur, and specular highlights. It
 * captures the background content below the layer and renders it with optical distortion shaped by
 * the layer's content. The background comes implicitly from the full PAGX layer tree below the
 * layer; no background reference attribute is used. All seven parameters are animatable float
 * channels with fields matching tgfx::GlassStyle.
 */
class GlassStyle : public LayerStyle {
 public:
  /**
   * The amount of optical distortion along curved edges, range [0, 100]. The default value is 80.
   */
  float refraction = 80.0f;

  /**
   * The inward extent of the refraction region from edges, range [1, 100]. The default value is 20.
   */
  float depth = 20.0f;

  /**
   * The amount of background blur (frosted glass), range [0, 100]. The default value is 5.
   */
  float frost = 5.0f;

  /**
   * The intensity of chromatic aberration (rainbow prism effect), range [0, 100]. The default
   * value is 50.
   */
  float dispersion = 50.0f;

  /**
   * The blend factor for the refraction direction, range [0, 100]. At 0, refraction follows the
   * curvature of the shape's edges; at 100, it points toward the shape center. The default value
   * is 0.
   */
  float splay = 0.0f;

  /**
   * The direction of the light source in degrees. 0 means light from directly above, positive
   * values rotate clockwise. Range [-179, 180]. The default value is 45.
   */
  float lightAngle = 45.0f;

  /**
   * The brightness of edge highlights, range [0, 100]. The default value is 80.
   */
  float lightIntensity = 80.0f;

  NodeType nodeType() const override {
    return NodeType::GlassStyle;
  }

 private:
  GlassStyle() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
