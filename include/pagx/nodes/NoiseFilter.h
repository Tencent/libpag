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

#include "pagx/nodes/LayerFilter.h"
#include "pagx/types/BlendMode.h"
#include "pagx/types/Color.h"
#include "pagx/types/NoiseMode.h"

namespace pagx {

/**
 * A noise filter that overlays procedural Perlin noise on the layer. Three noise modes are
 * available: Mono (single color), Duo (two complementary colors), and Multi (preserving original
 * noise RGB with enhanced contrast).
 */
class NoiseFilter : public LayerFilter {
 public:
  /**
   * The noise mode. The default value is Mono.
   */
  NoiseMode mode = NoiseMode::Mono;

  /**
   * The noise grain size. Larger values produce coarser grains. Must be positive. The default value
   * is 4.0.
   */
  float size = 4.0f;

  /**
   * The noise density in [0, 1]. Controls the proportion of visible noise pixels. The default value
   * is 0.5.
   */
  float density = 0.5f;

  /**
   * The random seed for the noise pattern. The default value is 0.
   */
  float seed = 0.0f;

  /**
   * The blend mode used to composite the noise with the source image. The default value is Normal.
   */
  BlendMode blendMode = BlendMode::Normal;

  /**
   * The noise color for Mono mode. The alpha component controls the noise opacity.
   */
  Color color = {};

  /**
   * The first noise color for Duo mode. The alpha component controls its opacity.
   */
  Color firstColor = {};

  /**
   * The second noise color for Duo mode. The alpha component controls its opacity.
   */
  Color secondColor = {};

  /**
   * The overall noise opacity for Multi mode, in [0, 1]. The default value is 0.15.
   */
  float opacity = 0.15f;

  NodeType nodeType() const override {
    return NodeType::NoiseFilter;
  }

 private:
  NoiseFilter() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
