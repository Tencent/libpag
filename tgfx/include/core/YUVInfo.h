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
 * Defines pixel formats for YUV pixels.
 */
enum class YUVPixelFormat {
  /**
   * uninitialized.
   */
  Unknown,
  /**
   * 8 bit Y plane followed by 8 bit 2x2 subsampled U and V planes.
   */
  I420,
  /**
   * 8-bit Y plane followed by an interleaved U/V plane with 2x2 subsampling.
   */
  NV12
};

/**
 *  Describes color space of YUV pixels. The color mapping from YUV to RGB varies depending on the
 *  source.
 */
enum class YUVColorSpace {
  /**
   * uninitialized.
   */
  Unknown,
  /**
   * Describes SDTV range.
   */
  Rec601,
  /**
   * Describes HDTV range.
   */
  Rec709,
  /**
   * Describes UHDTV range.
   */
  Rec2020
};

/**
 *  Describes color range of YUV pixels.
 */
enum class YUVColorRange {
  /**
   * uninitialized.
   */
  Unknown,
  /**
   * the normal 219*2^(n-8) "MPEG" YUV ranges
   */
  MPEG,
  /**
   * the normal 2^n-1 "JPEG" YUV ranges
   */
  JPEG
};

}  // namespace tgfx
