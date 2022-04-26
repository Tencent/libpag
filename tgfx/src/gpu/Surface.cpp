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

#include "tgfx/gpu/Surface.h"
#include "core/utils/Log.h"

namespace tgfx {
Surface::Surface(Context* context) : context(context) {
  DEBUG_ASSERT(context != nullptr);
}

bool Surface::readPixels(const ImageInfo& dstInfo, void* dstPixels, int srcX, int srcY) const {
  return onReadPixels(dstInfo, dstPixels, srcX, srcY);
}

bool Surface::hitTest(float x, float y) const {
  uint8_t pixel[4];
  auto info = ImageInfo::Make(1, 1, ColorType::RGBA_8888, AlphaType::Premultiplied);
  auto result = onReadPixels(info, pixel, static_cast<int>(x), static_cast<int>(y));
  return result && pixel[3] > 0;
}
}  // namespace tgfx