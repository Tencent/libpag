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

#include "CGLHardwareTexture.h"
#include "platform/apple/HardwareBuffer.h"
#include "tgfx/core/YUVBuffer.h"
#include "tgfx/gpu/YUVTexture.h"

namespace tgfx {
std::shared_ptr<PixelBuffer> PixelBuffer::MakeFrom(HardwareBufferRef hardwareBuffer) {
  return HardwareBuffer::MakeFrom(hardwareBuffer);
}

std::shared_ptr<PixelBuffer> PixelBuffer::MakeHardwareBuffer(int width, int height,
                                                             bool alphaOnly) {
  return HardwareBuffer::Make(width, height, alphaOnly);
}

std::shared_ptr<ImageBuffer> ImageBuffer::MakeFromHardwareBuffer(HardwareBufferRef hardwareBuffer) {
  return HardwareBuffer::MakeFrom(hardwareBuffer);
}

std::shared_ptr<Texture> Texture::MakeFrom(Context* context, HardwareBufferRef hardwareBuffer) {
  return CGLHardwareTexture::MakeFrom(context, hardwareBuffer);
}

std::shared_ptr<YUVBuffer> YUVBuffer::MakeFrom(HardwareBufferRef, YUVColorSpace, YUVColorRange) {
  return nullptr;
}

std::shared_ptr<YUVTexture> YUVTexture::MakeFrom(Context*, HardwareBufferRef, YUVColorSpace,
                                                 YUVColorRange) {
  return nullptr;
}
}  // namespace tgfx
