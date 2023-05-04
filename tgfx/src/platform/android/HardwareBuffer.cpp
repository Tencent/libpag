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

#include "AHardwareBufferFunctions.h"
#include "core/PixelBuffer.h"
#include "tgfx/platform/android/HardwareBufferJNI.h"

namespace tgfx {
std::shared_ptr<ImageBuffer> ImageBuffer::MakeFrom(HardwareBufferRef hardwareBuffer,
                                                   YUVColorSpace) {
  return PixelBuffer::MakeFrom(hardwareBuffer);
}

bool HardwareBufferCheck(HardwareBufferRef buffer) {
  if (!HardwareBufferAvailable()) {
    return false;
  }
  return buffer != nullptr;
}

HardwareBufferRef HardwareBufferAllocate(int width, int height, bool alphaOnly) {
  if (!HardwareBufferAvailable() || alphaOnly) {
    return nullptr;
  }
  AHardwareBuffer* hardwareBuffer = nullptr;
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
  AHardwareBufferFunctions::Get()->allocate(&desc, &hardwareBuffer);
  return hardwareBuffer;
}

HardwareBufferRef HardwareBufferRetain(HardwareBufferRef buffer) {
  static const auto acquire = AHardwareBufferFunctions::Get()->acquire;
  if (acquire != nullptr && buffer != nullptr) {
    acquire(buffer);
  }
  return buffer;
}

void HardwareBufferRelease(HardwareBufferRef buffer) {
  static const auto release = AHardwareBufferFunctions::Get()->release;
  if (release != nullptr && buffer != nullptr) {
    release(buffer);
  }
}

void* HardwareBufferLock(HardwareBufferRef buffer) {
  static const auto lock = AHardwareBufferFunctions::Get()->lock;
  if (lock == nullptr || buffer == nullptr) {
    return nullptr;
  }
  void* pixels = nullptr;
  lock(buffer, AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN | AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN, -1,
       nullptr, &pixels);
  return pixels;
}

void HardwareBufferUnlock(HardwareBufferRef buffer) {
  static const auto unlock = AHardwareBufferFunctions::Get()->unlock;
  if (unlock != nullptr && buffer != nullptr) {
    unlock(buffer, nullptr);
  }
}

ImageInfo HardwareBufferGetInfo(HardwareBufferRef buffer) {
  static const auto describe = AHardwareBufferFunctions::Get()->describe;
  if (!HardwareBufferAvailable() || buffer == nullptr) {
    return {};
  }
  AHardwareBuffer_Desc desc;
  describe(buffer, &desc);
  auto colorType = ColorType::Unknown;
  auto alphaType = AlphaType::Premultiplied;
  switch (desc.format) {
    case HARDWAREBUFFER_FORMAT_R8_UNORM:
      colorType = ColorType::ALPHA_8;
      break;
    case AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM:
      colorType = ColorType::RGBA_8888;
      break;
    case AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM:
      colorType = ColorType::RGBA_8888;
      alphaType = AlphaType::Opaque;
      break;
    default:
      break;
  }
  auto bytesPerPixel = ImageInfo::GetBytesPerPixel(colorType);
  return ImageInfo::Make(static_cast<int>(desc.width), static_cast<int>(desc.height), colorType,
                         alphaType, desc.stride * bytesPerPixel);
}

HardwareBufferRef HardwareBufferFromJavaObject(JNIEnv* env, jobject hardwareBufferObject) {
  static const auto fromHardwareBuffer = AHardwareBufferFunctions::Get()->fromHardwareBuffer;
  if (fromHardwareBuffer == nullptr || hardwareBufferObject == nullptr) {
    return nullptr;
  }
  return fromHardwareBuffer(env, hardwareBufferObject);
}

jobject HardwareBufferToJavaObject(JNIEnv* env, HardwareBufferRef hardwareBuffer) {
  static const auto toHardwareBuffer = AHardwareBufferFunctions::Get()->toHardwareBuffer;
  if (toHardwareBuffer == nullptr || hardwareBuffer == nullptr) {
    return nullptr;
  }
  return toHardwareBuffer(env, hardwareBuffer);
}
}  // namespace tgfx
