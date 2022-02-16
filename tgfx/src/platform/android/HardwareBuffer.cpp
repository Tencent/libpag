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

#include "HardwareBuffer.h"
#include "HardwareBufferInterface.h"

namespace tgfx {

std::shared_ptr<PixelBuffer> HardwareBuffer::MakeFrom(AHardwareBuffer* hardwareBuffer) {
  if (!hardwareBuffer || !HardwareBufferInterface::Available()) {
    return nullptr;
  }
  return std::shared_ptr<HardwareBuffer>(new HardwareBuffer(hardwareBuffer));
}

std::shared_ptr<PixelBuffer> HardwareBuffer::Make(int width, int height, bool alphaOnly) {
  if (alphaOnly || !HardwareBufferInterface::Available()) {
    return nullptr;
  }
  AHardwareBuffer* buffer = nullptr;
  AHardwareBuffer_Desc desc = {
      static_cast<uint32_t>(width),
      static_cast<uint32_t>(height),
      1,
      AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM,
      AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN | AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN |
          AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE | AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT,
      0,
      0,
      0};
  HardwareBufferInterface::Allocate(&desc, &buffer);
  if (!buffer) {
    return nullptr;
  }
  auto hardwareBuffer = std::shared_ptr<HardwareBuffer>(new HardwareBuffer(buffer));
  HardwareBufferInterface::Release(buffer);
  return hardwareBuffer;
}

static ImageInfo GetImageInfo(AHardwareBuffer* hardwareBuffer) {
  AHardwareBuffer_Desc desc;
  HardwareBufferInterface::Describe(hardwareBuffer, &desc);
  return ImageInfo::Make(desc.width, desc.height, ColorType::RGBA_8888, AlphaType::Premultiplied,
                         desc.stride * 4);
}

HardwareBuffer::HardwareBuffer(AHardwareBuffer* hardwareBuffer)
    : PixelBuffer(GetImageInfo(hardwareBuffer)), hardwareBuffer(hardwareBuffer) {
  HardwareBufferInterface::Acquire(hardwareBuffer);
}

std::shared_ptr<Texture> HardwareBuffer::makeTexture(Context* context) const {
  return Texture::MakeFrom(context, hardwareBuffer);
}

void* HardwareBuffer::lockPixels() {
  uint8_t* pixels = nullptr;
  HardwareBufferInterface::Lock(
      hardwareBuffer, AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN | AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN,
      -1, nullptr, reinterpret_cast<void**>(&pixels));
  return pixels;
}

void HardwareBuffer::unlockPixels() {
  HardwareBufferInterface::Unlock(hardwareBuffer, nullptr);
}

HardwareBuffer::~HardwareBuffer() {
  HardwareBufferInterface::Release(hardwareBuffer);
}
}  // namespace tgfx
