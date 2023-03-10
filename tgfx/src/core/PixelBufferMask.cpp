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

#include "PixelBufferMask.h"
#include "gpu/Gpu.h"
#include "tgfx/core/Pixmap.h"
#include "utils/PixelFormatUtil.h"

namespace tgfx {
PixelBufferMask::PixelBufferMask(std::shared_ptr<PixelBuffer> buffer)
    : Mask(buffer->width(), buffer->height()), buffer(std::move(buffer)) {
}

std::shared_ptr<Texture> PixelBufferMask::updateTexture(Context* context) {
  if (texture == nullptr) {
    texture = Texture::MakeFrom(context, buffer);
    dirtyRect.setEmpty();
  }
  if (texture && !buffer->isHardwareBacked() && !dirtyRect.isEmpty()) {
    if (auto pixels = static_cast<uint8_t*>(buffer->lockPixels())) {
      int x = static_cast<int>(dirtyRect.left);
      int y = static_cast<int>(dirtyRect.top);
      pixels += y * buffer->width() + x;
      auto rowBytes = buffer->info().rowBytes();
      context->gpu()->writePixels(texture->getSampler(), dirtyRect, pixels, rowBytes,
                                  ColorTypeToPixelFormat(buffer->info().colorType()));
      buffer->unlockPixels();
      dirtyRect.setEmpty();
    }
  }
  return texture;
}

void PixelBufferMask::clear() {
  dirty(Rect::MakeWH(width(), height()), false);
  auto pixels = buffer->lockPixels();
  Pixmap(buffer->info(), pixels).eraseAll();
  buffer->unlockPixels();
}

void PixelBufferMask::dirty(Rect rect, bool flipY) {
  if (buffer->isHardwareBacked()) {
    return;
  }
  if (flipY) {
    auto height = rect.height();
    rect.top = static_cast<float>(buffer->height()) - rect.bottom;
    rect.bottom = rect.top + height;
  }
  dirtyRect.join(rect);
}
}  // namespace tgfx
