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
#include "base/utils/USE.h"

namespace pag {
static std::mutex cacheLocker = {};
static std::unordered_map<CVPixelBufferRef, std::weak_ptr<HardwareBuffer>> hardwareBufferMap = {};

static ImageInfo GetImageInfo(CVPixelBufferRef pixelBuffer) {
  auto width = static_cast<int>(CVPixelBufferGetWidth(pixelBuffer));
  auto height = static_cast<int>(CVPixelBufferGetHeight(pixelBuffer));
  auto pixelFormat = CVPixelBufferGetPixelFormatType(pixelBuffer);
  auto colorType =
      pixelFormat == kCVPixelFormatType_OneComponent8 ? ColorType::ALPHA_8 : ColorType::BGRA_8888;
  auto rowBytes = CVPixelBufferGetBytesPerRow(pixelBuffer);
  return ImageInfo::Make(width, height, colorType, AlphaType::Premultiplied, rowBytes);
}

std::shared_ptr<HardwareBuffer> HardwareBuffer::Make(int width, int height, bool alphaOnly) {
#if TARGET_IPHONE_SIMULATOR
    
  // We cannot bind CVPixelBuffer to GL on iOS simulator.
  USE(width);
  USE(height);
  USE(alphaOnly);
  return nullptr;
    
#else

  if (width == 0 || height == 0) {
    return nil;
  }
  OSType pixelFormat = alphaOnly ? kCVPixelFormatType_OneComponent8 : kCVPixelFormatType_32BGRA;
  CFDictionaryRef empty =
      CFDictionaryCreate(kCFAllocatorDefault, nullptr, nullptr, 0, &kCFTypeDictionaryKeyCallBacks,
                         &kCFTypeDictionaryValueCallBacks);
  CFMutableDictionaryRef attrs = CFDictionaryCreateMutable(
      kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  CFDictionarySetValue(attrs, kCVPixelBufferIOSurfacePropertiesKey, empty);
  CVPixelBufferRef pixelBuffer = nil;
  CVReturn status =
      CVPixelBufferCreate(kCFAllocatorDefault, static_cast<size_t>(width),
                          static_cast<size_t>(height), pixelFormat, attrs, &pixelBuffer);
  CFRelease(attrs);
  CFRelease(empty);
  if (status != kCVReturnSuccess) {
    return nil;
  }
  auto hardwareBuffer = HardwareBuffer::MakeFrom(pixelBuffer);
  CFRelease(pixelBuffer);
  return hardwareBuffer;
#endif
}

std::shared_ptr<HardwareBuffer> HardwareBuffer::MakeFrom(CVPixelBufferRef pixelBuffer) {
#if TARGET_IPHONE_SIMULATOR
    
  // We cannot bind CVPixelBuffer to GL on iOS simulator.
  USE(pixelBuffer);
  return nullptr;
    
#else

  if (pixelBuffer == nil) {
    return nullptr;
  }
  auto pixelFormat = CVPixelBufferGetPixelFormatType(pixelBuffer);
  if (pixelFormat != kCVPixelFormatType_32BGRA && pixelFormat != kCVPixelFormatType_OneComponent8) {
    return nullptr;
  }
  std::lock_guard<std::mutex> cacheLock(cacheLocker);
  auto result = hardwareBufferMap.find(pixelBuffer);
  if (result != hardwareBufferMap.end()) {
    auto hardwareBuffer = result->second.lock();
    if (hardwareBuffer) {
      return hardwareBuffer;
    }
    hardwareBufferMap.erase(result);
  }
  auto hardwareBuffer = std::shared_ptr<HardwareBuffer>(new HardwareBuffer(pixelBuffer));
  hardwareBufferMap[pixelBuffer] = hardwareBuffer;
  return hardwareBuffer;
    
#endif
}

HardwareBuffer::HardwareBuffer(CVPixelBufferRef pixelBuffer)
    : PixelBuffer(GetImageInfo(pixelBuffer)), pixelBuffer(pixelBuffer) {
  CFRetain(pixelBuffer);
}

HardwareBuffer::~HardwareBuffer() {
  CFRelease(pixelBuffer);
  std::lock_guard<std::mutex> cacheLock(cacheLocker);
  hardwareBufferMap.erase(pixelBuffer);
}

void* HardwareBuffer::lockPixels() {
  CVPixelBufferLockBaseAddress(pixelBuffer, 0);
  return CVPixelBufferGetBaseAddress(pixelBuffer);
}

void HardwareBuffer::unlockPixels() {
  CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
}

std::shared_ptr<Texture> HardwareBuffer::makeTexture(Context* context) const {
  return Texture::MakeFrom(context, pixelBuffer);
}

}
