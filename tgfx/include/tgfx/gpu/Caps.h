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

#include "tgfx/gpu/PixelFormat.h"

namespace tgfx {
class Swizzle;

class Caps {
 public:
  virtual ~Caps() = default;

  virtual const Swizzle& getWriteSwizzle(PixelFormat pixelFormat) const = 0;

  virtual bool isFormatRenderable(PixelFormat pixelFormat) const = 0;

  virtual int getSampleCount(int requestedCount, PixelFormat pixelFormat) const = 0;

  virtual int getMaxMipmapLevel(int width, int height) const = 0;

  bool floatIs32Bits = true;
  int maxTextureSize = 0;
  bool semaphoreSupport = false;
  bool multisampleDisableSupport = false;
  /**
   * The CLAMP_TO_BORDER wrap mode for texture coordinates was added to desktop GL in 1.3, and
   * GLES 3.2, but is also available in extensions. Vulkan and Metal always have support.
   */
  bool clampToBorderSupport = true;
  bool npotTextureTileSupport = true;  // Vulkan and Metal always have support.
  bool mipMapSupport = true;
  bool textureBarrierSupport = false;
  bool frameBufferFetchSupport = false;
};
}  // namespace tgfx
