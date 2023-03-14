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

#include "core/PixelBuffer.h"
#include "gpu/Texture.h"
#include "tgfx/opengl/qt/QGLDevice.h"
#ifdef __APPLE__
#include "opengl/cgl/CGLHardwareTexture.h"
#include "platform/apple/HardwareBuffer.h"
#endif

namespace tgfx {
#ifdef __APPLE__

std::shared_ptr<PixelBuffer> PixelBuffer::MakeHardwareBuffer(int width, int height,
                                                             bool alphaOnly) {
  return HardwareBuffer::Make(width, height, alphaOnly);
}

std::shared_ptr<ImageBuffer> ImageBuffer::MakeFrom(HardwareBufferRef hardwareBuffer,
                                                   YUVColorSpace) {
  return HardwareBuffer::MakeFrom(hardwareBuffer);
}

std::shared_ptr<Texture> Texture::MakeFrom(Context* context, HardwareBufferRef hardwareBuffer,
                                           YUVColorSpace) {
  auto qglDevice = static_cast<QGLDevice*>(context->device());
  if (qglDevice == nullptr) {
    return nullptr;
  }
  auto textureCache = qglDevice->getTextureCache();
  return CGLHardwareTexture::MakeFrom(context, hardwareBuffer, textureCache);
}

#else

std::shared_ptr<PixelBuffer> PixelBuffer::MakeHardwareBuffer(int, int, bool) {
  return nullptr;
}

std::shared_ptr<ImageBuffer> ImageBuffer::MakeFrom(HardwareBufferRef, YUVColorSpace) {
  return nullptr;
}

std::shared_ptr<Texture> Texture::MakeFrom(Context*, HardwareBufferRef, YUVColorSpace) {
  return nullptr;
}

#endif
}  // namespace tgfx