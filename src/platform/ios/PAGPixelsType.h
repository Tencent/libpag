/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

// Describes how to interpret the alpha component of a pixel.
typedef enum {
  // uninitialized.
  PAGAlphaTypeUnknow = 0,
  // pixel is opaque.
  PAGAlphaTypeOpaque = 1,
  // pixel components are premultiplied by alpha.
  PAGAlphaTypePremultiplied = 2,
  // ixel components are independent of alpha.
  PAGAlphaTypeUnpremultiplied = 3
} PAGAlphaType;

/**
 * Describes how pixel bits encode color. These values match up with the enum in Bitmap.Config on
 * Android platform.
 */
typedef enum {
  // uninitialized.
  PAGColorTypeUnknown = 0,
  /**
     * Each pixel is stored as a single translucency (alpha) channel. This is very useful for
     * storing masks efficiently, for instance. No color information is stored. With this
     * configuration, each pixel requires 1 byte of memory.
     */
  PAGColorTypeALPHA_8 = 1,
  /**
     * Each pixel is stored on 4 bytes. Each channel (RGB and alpha for translucency) is stored with 8
     * bits of precision (256 possible values). The channel order is: blue, green, red, alpha.
     */
  PAGColorTypeRGBA_8888 = 2,
  /**
     * Each pixel is stored on 2 bytes, and only the RGB channels are encoded: red is stored with 5
     * bits of precision (32 possible values), green is stored with 6 bits of precision (64 possible
     * values), and blue is stored with 5 bits of precision.
     */
  PAGColorTypeBGRA_8888 = 3,
  /**
     * Each pixel is stored as a single grayscale level. No color information is stored. With this
     * configuration, each pixel requires 1 byte of memory.
     */
  PAGColorTypeRGB_565 = 4,
  /**
     * Each pixel is stored on 8 bytes. Each channel (RGB and alpha for translucency) is stored as a
     * half-precision floating point value. This configuration is particularly suited for wide-gamut
     * and HDR content.
     */
  PAGColorTypeGray_8 = 5,
  /**
     * Each pixel is stored on 8 bytes. Each channel (RGB and alpha for translucency) is stored as a
     * half-precision floating point value. This configuration is particularly suited for wide-gamut
     * and HDR content.
     */
  PAGColorTypeRGBA_F16 = 6,
  /**
     * Each pixel is stored on 4 bytes. Each RGB channel is stored with 10 bits of precision (1024
     * possible values). There is an additional alpha channel that is stored with 2 bits of precision
     * (4 possible values). This configuration is suited for wide-gamut and HDR content which does not
     * require alpha blending, such that the memory cost is the same as RGBA_8888 while enabling
     * higher color precision.
     */
  PAGColorTypeRGBA_1010102 = 7
} PAGColorType;
