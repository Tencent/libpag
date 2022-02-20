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

namespace tgfx {
/**
 * Describes the possible pixel formats of a TextureSampler.
 */
enum class PixelFormat {
  /**
   * Pixel with 8 bits for alpha. Each pixel is stored on 1 bytes.
   */
  ALPHA_8,

  /**
   * Pixel with 8 bits for grayscale. Each pixel is stored on 1 bytes.
   */
  GRAY_8,

  /**
   * Pixel with 8 bits for red, green. Each pixel is stored on 2 bytes.
   */
  RG_88,

  /**
   * Pixel with 8 bits for red, green, blue, alpha. Each pixel is stored on 4 bytes.
   */
  RGBA_8888
};
}  // namespace tgfx
