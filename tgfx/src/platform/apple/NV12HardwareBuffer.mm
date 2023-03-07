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

#include "NV12HardwareBuffer.h"
#include "gpu/YUVTexture.h"
#include "utils/USE.h"

namespace tgfx {
static std::mutex cacheLocker = {};
static std::unordered_map<CVPixelBufferRef, std::weak_ptr<NV12HardwareBuffer>> nv12BufferMap = {};

std::shared_ptr<NV12HardwareBuffer> NV12HardwareBuffer::MakeFrom(CVPixelBufferRef pixelBuffer,
                                                                 YUVColorSpace colorSpace) {
#if TARGET_IPHONE_SIMULATOR

  // We cannot bind CVPixelBuffer to GL on iOS simulator.
  USE(pixelBuffer);
  USE(colorSpace);
  return nullptr;

#else

  if (pixelBuffer == nil) {
    return nullptr;
  }
  auto format = CVPixelBufferGetPixelFormatType(pixelBuffer);
  if (format != kCVPixelFormatType_420YpCbCr8BiPlanarFullRange &&
      format != kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange) {
    return nullptr;
  }
  std::lock_guard<std::mutex> cacheLock(cacheLocker);
  auto result = nv12BufferMap.find(pixelBuffer);
  if (result != nv12BufferMap.end()) {
    auto buffer = result->second.lock();
    if (buffer) {
      return buffer;
    }
    nv12BufferMap.erase(result);
  }
  auto buffer =
      std::shared_ptr<NV12HardwareBuffer>(new NV12HardwareBuffer(pixelBuffer, colorSpace));
  nv12BufferMap[pixelBuffer] = buffer;
  return buffer;

#endif
}

NV12HardwareBuffer::NV12HardwareBuffer(CVPixelBufferRef pixelBuffer, YUVColorSpace colorSpace)
    : pixelBuffer(pixelBuffer), colorSpace(colorSpace) {
  CFRetain(pixelBuffer);
}

NV12HardwareBuffer::~NV12HardwareBuffer() {
  CFRelease(pixelBuffer);
  std::lock_guard<std::mutex> cacheLock(cacheLocker);
  nv12BufferMap.erase(pixelBuffer);
}

int NV12HardwareBuffer::width() const {
  return static_cast<int>(CVPixelBufferGetWidth(pixelBuffer));
}

int NV12HardwareBuffer::height() const {
  return static_cast<int>(CVPixelBufferGetHeight(pixelBuffer));
}

std::shared_ptr<Texture> NV12HardwareBuffer::onMakeTexture(Context* context, bool) const {
  return YUVTexture::MakeFrom(context, pixelBuffer, colorSpace);
}
}  // namespace tgfx
