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
 * Blend modes for compositing layers and colors.
 */
enum class BlendMode {
  /**
   * Normal blending, the source color replaces the destination.
   */
  Normal,
  /**
   * Multiplies the source and destination colors.
   */
  Multiply,
  /**
   * Inverts, multiplies, and inverts again, resulting in a lighter image.
   */
  Screen,
  /**
   * Combines Multiply and Screen modes depending on the destination color.
   */
  Overlay,
  /**
   * Retains the darker of the source and destination colors.
   */
  Darken,
  /**
   * Retains the lighter of the source and destination colors.
   */
  Lighten,
  /**
   * Brightens the destination color to reflect the source color.
   */
  ColorDodge,
  /**
   * Darkens the destination color to reflect the source color.
   */
  ColorBurn,
  /**
   * Combines Multiply and Screen modes depending on the source color.
   */
  HardLight,
  /**
   * A softer version of Hard Light, producing a more subtle effect.
   */
  SoftLight,
  /**
   * Subtracts the darker color from the lighter color.
   */
  Difference,
  /**
   * Similar to Difference but with lower contrast.
   */
  Exclusion,
  /**
   * Creates a result color with the hue of the source and the saturation and luminosity of the
   * destination.
   */
  Hue,
  /**
   * Creates a result color with the saturation of the source and the hue and luminosity of
   * the destination.
   */
  Saturation,
  /**
   * Creates a result color with the hue and saturation of the source and the luminosity of the
   * destination.
   */
  Color,
  /**
   * Creates a result color with the luminosity of the source and the hue and saturation of the
   * destination.
   */
  Luminosity,
  /**
   * Adds the source and destination colors, clamping to white.
   */
  PlusLighter,
  /**
   * Adds the source and destination colors, clamping to black.
   */
  PlusDarker
};

}  // namespace pagx
