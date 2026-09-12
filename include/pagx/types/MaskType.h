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
 * Mask types that define how a mask layer affects its target.
 */
enum class MaskType {
  /**
   * Use the alpha channel of the mask to determine visibility.
   */
  Alpha,
  /**
   * Use the luminance (brightness) of the mask to determine visibility.
   */
  Luminance,
  /**
   * Use the contour (outline) of the mask for masking.
   */
  Contour,
  /**
   * Use the inverted alpha channel of the mask to determine visibility. The layer content is
   * visible where the mask is transparent and hidden where the mask is opaque.
   */
  AlphaInverted,
  /**
   * Use the inverted luminance of the mask to determine visibility. The layer content is visible
   * where the mask is dark and hidden where the mask is bright.
   */
  LuminanceInverted,
  /**
   * Use the inverted contour (outline) of the mask for masking. The layer content is visible
   * outside the mask's contour and hidden inside it.
   */
  ContourInverted
};

}  // namespace pagx
