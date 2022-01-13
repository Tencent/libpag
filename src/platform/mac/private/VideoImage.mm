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

#include "VideoImage.h"

namespace pag {
static std::mutex cacheLocker = {};
static std::unordered_map<CVPixelBufferRef, std::weak_ptr<VideoImage>> videoImageMap = {};

std::shared_ptr<VideoImage> VideoImage::MakeFrom(CVPixelBufferRef pixelBuffer) {
  if (pixelBuffer == nil) {
    return nullptr;
  }
  auto format = CVPixelBufferGetPixelFormatType(pixelBuffer);
  if (format != kCVPixelFormatType_32BGRA) {
    return nullptr;
  }
  std::lock_guard<std::mutex> cacheLock(cacheLocker);
  auto result = videoImageMap.find(pixelBuffer);
  if (result != videoImageMap.end()) {
    auto videoFrame = result->second.lock();
    if (videoFrame) {
      return videoFrame;
    }
    videoImageMap.erase(result);
  }
  auto videoImage = std::shared_ptr<VideoImage>(new VideoImage(pixelBuffer));
  videoImageMap[pixelBuffer] = videoImage;
  return videoImage;
}

VideoImage::VideoImage(CVPixelBufferRef pixelBuffer)
    : VideoBuffer(static_cast<int>(CVPixelBufferGetWidth(pixelBuffer)),
                  static_cast<int>(CVPixelBufferGetHeight(pixelBuffer))),
      pixelBuffer(pixelBuffer) {
  CFRetain(pixelBuffer);
}

size_t VideoImage::planeCount() const {
  return 1;
}

VideoImage::~VideoImage() {
  CFRelease(pixelBuffer);
  std::lock_guard<std::mutex> cacheLock(cacheLocker);
  videoImageMap.erase(pixelBuffer);
}
}
