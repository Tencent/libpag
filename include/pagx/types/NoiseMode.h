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

namespace pagx {

/**
 * Noise modes for noise filters and noise layer styles.
 */
enum class NoiseMode {
  /**
   * Single-color noise. Noise pixels are filled with one color.
   */
  Mono,
  /**
   * Dual-color noise. The noise source is split into two complementary regions.
   */
  Duo,
  /**
   * Multi-color noise preserving original Perlin noise RGB with enhanced contrast.
   */
  Multi
};

}  // namespace pagx
