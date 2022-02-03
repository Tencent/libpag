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

#include "PixelBufferUtils.h"

namespace pag {
CVPixelBufferRef PixelBufferUtils::Make(int width, int height) {
  if (width == 0 || height == 0) {
    return nil;
  }
  CFDictionaryRef empty =
      CFDictionaryCreate(kCFAllocatorDefault, nullptr, nullptr, 0, &kCFTypeDictionaryKeyCallBacks,
                         &kCFTypeDictionaryValueCallBacks);
  CFMutableDictionaryRef attrs = CFDictionaryCreateMutable(
      kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  CFDictionarySetValue(attrs, kCVPixelBufferIOSurfacePropertiesKey, empty);
  CVPixelBufferRef pixelBuffer = nil;
  CVReturn status = CVPixelBufferCreate(kCFAllocatorDefault, static_cast<size_t>(width),
                                        static_cast<size_t>(height), kCVPixelFormatType_32BGRA,
                                        attrs, &pixelBuffer);
  CFRelease(attrs);
  CFRelease(empty);
  if (status != kCVReturnSuccess) {
    return nil;
  }
  CFAutorelease(pixelBuffer);
  return pixelBuffer;
}
}
