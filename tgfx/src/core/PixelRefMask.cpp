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

#include "PixelRefMask.h"
#include "gpu/Gpu.h"
#include "tgfx/core/Pixmap.h"
#include "utils/PixelFormatUtil.h"

namespace tgfx {
PixelRefMask::PixelRefMask(std::shared_ptr<PixelRef> pixelRef) : pixelRef(std::move(pixelRef)) {
}

void PixelRefMask::markContentDirty(const Rect& bounds, bool flipY) {
  if (flipY) {
    auto rect = bounds;
    auto height = rect.height();
    rect.top = static_cast<float>(pixelRef->height()) - rect.bottom;
    rect.bottom = rect.top + height;
    pixelRef->markContentDirty(rect);
  } else {
    pixelRef->markContentDirty(bounds);
  }
}
}  // namespace tgfx
