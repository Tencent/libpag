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

#include "NativeHardwareBuffer.h"
#include "PixelBufferUtils.h"

namespace pag {
static std::mutex cacheLocker = {};
static std::unordered_map<CVPixelBufferRef, std::weak_ptr<NativeHardwareBuffer>> graphicBufferMap =
    {};

static ImageInfo GetImageInfo(CVPixelBufferRef pixelBuffer) {
  auto width = static_cast<int>(CVPixelBufferGetWidth(pixelBuffer));
  auto height = static_cast<int>(CVPixelBufferGetHeight(pixelBuffer));
  auto pixelFormat = CVPixelBufferGetPixelFormatType(pixelBuffer);
  auto colorType =
      pixelFormat == kCVPixelFormatType_OneComponent8 ? ColorType::ALPHA_8 : ColorType::BGRA_8888;
  auto rowBytes = CVPixelBufferGetBytesPerRow(pixelBuffer);
  return ImageInfo::Make(width, height, colorType, AlphaType::Premultiplied, rowBytes);
}

std::shared_ptr<NativeHardwareBuffer> NativeHardwareBuffer::MakeAdopted(
    CVPixelBufferRef pixelBuffer) {
  if (pixelBuffer == nil) {
    return nullptr;
  }
  auto pixelFormat = CVPixelBufferGetPixelFormatType(pixelBuffer);
  if (pixelFormat != kCVPixelFormatType_32BGRA && pixelFormat != kCVPixelFormatType_OneComponent8) {
    return nullptr;
  }
  std::lock_guard<std::mutex> cacheLock(cacheLocker);
  auto result = graphicBufferMap.find(pixelBuffer);
  if (result != graphicBufferMap.end()) {
    auto graphicBuffer = result->second.lock();
    if (graphicBuffer) {
      return graphicBuffer;
    }
    graphicBufferMap.erase(result);
  }
  auto hardwareBuffer =
      std::shared_ptr<NativeHardwareBuffer>(new NativeHardwareBuffer(pixelBuffer, true));
  graphicBufferMap[pixelBuffer] = hardwareBuffer;
  return hardwareBuffer;
}

NativeHardwareBuffer::NativeHardwareBuffer(CVPixelBufferRef pixelBuffer, bool adopted)
    : PixelBuffer(GetImageInfo(pixelBuffer)), pixelBuffer(pixelBuffer), adopted(adopted) {
  CFRetain(pixelBuffer);
}

NativeHardwareBuffer::~NativeHardwareBuffer() {
  CFRelease(pixelBuffer);
  std::lock_guard<std::mutex> cacheLock(cacheLocker);
  graphicBufferMap.erase(pixelBuffer);
}

void* NativeHardwareBuffer::lockPixels() {
  CVPixelBufferLockBaseAddress(pixelBuffer, 0);
  return CVPixelBufferGetBaseAddress(pixelBuffer);
}

void NativeHardwareBuffer::unlockPixels() {
  CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
}

}
