/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "TraceImage.h"
#include <CoreVideo/CoreVideo.h>
#include "base/utils/Log.h"
#include "platform/cocoa/private/PixelBufferUtil.h"
#include "tgfx/core/Pixmap.h"

namespace pag {
void TraceImage(const tgfx::ImageInfo& info, const void* pixels, const std::string& tag) {
  if (info.isEmpty() || pixels == nullptr) {
    return;
  }
  @autoreleasepool {
    auto pixelBuffer = PixelBufferUtil::Make(info.width(), info.height());
    if (pixelBuffer == nil) {
      return;
    }
    auto dstPixels = tgfx::HardwareBufferLock(pixelBuffer);
    auto dstInfo = tgfx::HardwareBufferGetInfo(pixelBuffer);
    tgfx::Pixmap pixmap(dstInfo, dstPixels);
    pixmap.writePixels(info, pixels);
    tgfx::HardwareBufferUnlock(pixelBuffer);
    LOGI("%s : Image(width = %d, height = %d)", tag.c_str(), info.width(), info.height());
  }
}
}  // namespace pag
