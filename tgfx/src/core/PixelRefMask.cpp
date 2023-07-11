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

static float LinearToSRGB(float linear) {
  // The magic numbers are derived from the sRGB specification.
  // See http://www.color.org/chardata/rgb/srgb.xalter.
  if (linear <= 0.0031308f) {
    return linear * 12.92f;
  }
  return 1.055f * std::pow(linear, 1.f / 2.4f) - 0.055f;
}

const std::array<uint8_t, 256>& PixelRefMask::GammaTable() {
  static const std::array<uint8_t, 256> table = [] {
    std::array<uint8_t, 256> table{};
    table[0] = 0;
    table[255] = 255;
    for (int i = 1; i < 255; ++i) {
      auto v = std::roundf(LinearToSRGB(static_cast<float>(i) / 255.f) * 255.f);
      table[i] = static_cast<uint8_t>(v);
    }
    return table;
  }();
  return table;
}
}  // namespace tgfx
