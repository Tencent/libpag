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

#include "NativeGraphicBuffer.h"

#include "GLHardwareTexture.h"

namespace pag {
std::shared_ptr<NativeGraphicBuffer> NativeGraphicBuffer::MakeAdopted(
    android::GraphicBuffer* graphicBuffer) {
  if (!graphicBuffer || !NativeGraphicBufferInterface::Get()->constructor) {
    return nullptr;
  }
  return std::shared_ptr<NativeGraphicBuffer>(new NativeGraphicBuffer(graphicBuffer));
}

std::shared_ptr<PixelBuffer> NativeGraphicBuffer::Make(int width, int height, bool alphaOnly) {
  if (alphaOnly) {
    return nullptr;
  }
  if (!NativeGraphicBufferInterface::Get()->constructor) {
    return nullptr;
  }
  auto buffer = NativeGraphicBufferInterface::MakeGraphicBuffer(
      width, height, PIXEL_FORMAT_RGBA_8888,
      NativeGraphicBufferInterface::USAGE_HW_RENDER |
          NativeGraphicBufferInterface::USAGE_HW_TEXTURE |
          NativeGraphicBufferInterface::USAGE_HW_RENDER |
          NativeGraphicBufferInterface::USAGE_SW_READ_OFTEN |
          NativeGraphicBufferInterface::USAGE_SW_WRITE_OFTEN);
  if (!buffer) {
    return nullptr;
  }
  return std::shared_ptr<NativeGraphicBuffer>(new NativeGraphicBuffer(buffer));
}

std::shared_ptr<Texture> NativeGraphicBuffer::makeTexture(Context* context) const {
  return GLHardwareTexture::MakeFrom(context, graphicBuffer);
}

void* NativeGraphicBuffer::lockPixels() {
  uint8_t* readPtr = nullptr;
  if (graphicBuffer) {
    NativeGraphicBufferInterface::Get()->lock(
        graphicBuffer,
        NativeGraphicBufferInterface::USAGE_SW_READ_OFTEN |
            NativeGraphicBufferInterface::USAGE_SW_WRITE_OFTEN,
        reinterpret_cast<void**>(&readPtr));
  }
  return readPtr;
}

void NativeGraphicBuffer::unlockPixels() {
  if (graphicBuffer) {
    NativeGraphicBufferInterface::Get()->unlock(graphicBuffer);
  }
}

static ImageInfo GetImageInfo(android::GraphicBuffer* graphicBuffer) {
  auto desc =
      (android::android_native_buffer_t*)NativeGraphicBufferInterface::Get()->getNativeBuffer(
          graphicBuffer);
  return ImageInfo::Make(desc->width, desc->height, ColorType::RGBA_8888, AlphaType::Premultiplied,
                         desc->stride * 4);
}

NativeGraphicBuffer::NativeGraphicBuffer(android::GraphicBuffer* graphicBuffer)
    : PixelBuffer(GetImageInfo(graphicBuffer)), graphicBuffer(graphicBuffer) {
  if (graphicBuffer) {
    NativeGraphicBufferInterface::Acquire(graphicBuffer);
  }
}

NativeGraphicBuffer::~NativeGraphicBuffer() {
  if (graphicBuffer) {
    NativeGraphicBufferInterface::Release(graphicBuffer);
  }
}
}  // namespace pag
