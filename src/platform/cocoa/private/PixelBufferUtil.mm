/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "PixelBufferUtil.h"
#import <Foundation/Foundation.h>

namespace pag {
CVPixelBufferRef PixelBufferUtil::Make(int width, int height, bool alphaOnly) {
  OSType pixelFormat = alphaOnly ? kCVPixelFormatType_OneComponent8 : kCVPixelFormatType_32BGRA;
  NSDictionary* options = @{(id)kCVPixelBufferIOSurfacePropertiesKey : @{}};
  CVPixelBufferRef pixelBuffer = nil;
  CVReturn status = CVPixelBufferCreate(kCFAllocatorDefault, static_cast<size_t>(width),
                                        static_cast<size_t>(height), pixelFormat,
                                        (CFDictionaryRef)options, &pixelBuffer);
  if (status != kCVReturnSuccess) {
    return nil;
  }
  CFAutorelease(pixelBuffer);
  return pixelBuffer;
}
}  // namespace pag
