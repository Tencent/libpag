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

#include "CocoaPlatform.h"
#include "FontConfig.h"
#include "NativeHardwareBuffer.h"
#include "NativeImage.h"
#include "PixelBufferUtils.h"
#include "TraceImage.h"
#include "base/utils/USE.h"

namespace pag {

std::shared_ptr<PixelBuffer> CocoaPlatform::makeHardwareBuffer(int width, int height,
                                                               bool alphaOnly) const {
#if TARGET_IPHONE_SIMULATOR
  USE(width);
  USE(height);
  USE(alphaOnly);
  return nullptr;
#else
  @autoreleasepool {
    auto pixelBuffer = PixelBufferUtils::Make(width, height, alphaOnly);
    if (pixelBuffer == nil) {
      return nullptr;
    }
    return std::shared_ptr<PixelBuffer>(new NativeHardwareBuffer(pixelBuffer, false));
  }
#endif
}

std::shared_ptr<Image> CocoaPlatform::makeImage(const std::string& filePath) const {
  return NativeImage::MakeFrom(filePath);
}

std::shared_ptr<Image> CocoaPlatform::makeImage(std::shared_ptr<Data> imageBytes) const {
  return NativeImage::MakeFrom(imageBytes);
}

PAGFont CocoaPlatform::parseFont(const std::string& fontPath, int ttcIndex) const {
  return FontConfig::Parse(fontPath, ttcIndex);
}

PAGFont CocoaPlatform::parseFont(const void* data, size_t length, int ttcIndex) const {
  return FontConfig::Parse(data, length, ttcIndex);
}

bool CocoaPlatform::registerFallbackFonts() const {
  return FontConfig::RegisterFallbackFonts();
}

void CocoaPlatform::traceImage(const PixelMap& pixelMap, const std::string& tag) const {
  TraceImage(pixelMap, tag);
}
}
