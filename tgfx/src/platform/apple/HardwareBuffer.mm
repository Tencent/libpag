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

#include "tgfx/platform/HardwareBuffer.h"
#import <CoreVideo/CoreVideo.h>
#include "core/PixelBuffer.h"
#include "platform/apple/NV12HardwareBuffer.h"

namespace tgfx {
std::shared_ptr<ImageBuffer> ImageBuffer::MakeFrom(HardwareBufferRef hardwareBuffer,
                                                   YUVColorSpace colorSpace) {
  if (hardwareBuffer == nullptr) {
    return nullptr;
  }
  auto planeCount = CVPixelBufferGetPlaneCount(hardwareBuffer);
  if (planeCount > 1) {
    return NV12HardwareBuffer::MakeFrom(hardwareBuffer, colorSpace);
  }
  return PixelBuffer::MakeFrom(hardwareBuffer);
}

bool HardwareBufferCheck(HardwareBufferRef buffer) {
  if (!HardwareBufferAvailable()) {
    return false;
  }
  return CVPixelBufferGetIOSurface(buffer) != nil;
}

HardwareBufferRef HardwareBufferAllocate(int width, int height, bool alphaOnly) {
  if (width <= 0 || height <= 0) {
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
  return pixelBuffer;
}

HardwareBufferRef HardwareBufferRetain(HardwareBufferRef buffer) {
  return CVPixelBufferRetain(buffer);
}

void HardwareBufferRelease(HardwareBufferRef buffer) {
  CVPixelBufferRelease(buffer);
}

void* HardwareBufferLock(HardwareBufferRef buffer) {
  CVPixelBufferLockBaseAddress(buffer, 0);
  return CVPixelBufferGetBaseAddress(buffer);
}

void HardwareBufferUnlock(HardwareBufferRef buffer) {
  CVPixelBufferUnlockBaseAddress(buffer, 0);
}

ImageInfo HardwareBufferGetInfo(HardwareBufferRef buffer) {
  if (buffer == nil) {
    return {};
  }
  auto planeCount = CVPixelBufferGetPlaneCount(buffer);
  if (planeCount > 1) {
    return {};
  }
  auto width = static_cast<int>(CVPixelBufferGetWidth(buffer));
  auto height = static_cast<int>(CVPixelBufferGetHeight(buffer));
  auto pixelFormat = CVPixelBufferGetPixelFormatType(buffer);
  ColorType colorType = ColorType::Unknown;
  switch (pixelFormat) {
    case kCVPixelFormatType_OneComponent8:
      colorType = ColorType::ALPHA_8;
      break;
    case kCVPixelFormatType_32BGRA:
      colorType = ColorType::BGRA_8888;
      break;
    default:
      break;
  }
  auto rowBytes = CVPixelBufferGetBytesPerRow(buffer);
  return ImageInfo::Make(width, height, colorType, AlphaType::Premultiplied, rowBytes);
}
}  // namespace tgfx
