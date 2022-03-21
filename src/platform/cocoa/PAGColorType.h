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

/**
 * Describes how pixel bits encode color. These values match up with the enum in Bitmap.
 */
typedef enum {
  /**
     * uninitialized.
     */
  PAGColorTypeUnknown = 0,
  /**
     * Each pixel is stored as a single translucency (alpha) channel. This is very useful to
     * efficiently store masks for instance. No color information is stored. With this configuration,
     * each pixel requires 1 byte of memory.
     */
  PAGColorTypeALPHA_8 = 1,
  /**
     * Each pixel is stored on 4 bytes. Each channel (RGB and alpha for translucency) is stored with 8
     * bits of precision (256 possible values). The channel order is: red, green, blue, alpha.
     */
  PAGColorTypeRGBA_8888 = 2,
  /**
     * Each pixel is stored on 4 bytes. Each channel (RGB and alpha for translucency) is stored with 8
     * bits of precision (256 possible values). The channel order is: blue, green, red, alpha.
     */
  PAGColorTypeBGRA_8888 = 3,
} PAGColorType;
