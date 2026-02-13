/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "pagx/types/ColorSpace.h"

namespace pagx {

/**
 * An RGBA color with floating-point components and color space.
 */
struct Color {
  /**
   * Red component in [0, 1] range.
   */
  float red = 0;

  /**
   * Green component in [0, 1] range.
   */
  float green = 0;

  /**
   * Blue component in [0, 1] range.
   */
  float blue = 0;

  /**
   * Alpha component, in [0, 1] range. Default is 1 (fully opaque).
   */
  float alpha = 1;

  /**
   * The color space of this color. Default is sRGB.
   */
  ColorSpace colorSpace = ColorSpace::SRGB;

  bool operator==(const Color& other) const {
    return red == other.red && green == other.green && blue == other.blue && alpha == other.alpha &&
           colorSpace == other.colorSpace;
  }

  bool operator!=(const Color& other) const {
    return !(*this == other);
  }
};

}  // namespace pagx
