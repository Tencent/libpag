/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "PixelRef.h"
#include "gpu/Gpu.h"

namespace tgfx {
std::shared_ptr<PixelRef> PixelRef::Make(int width, int height, bool alphaOnly, bool tryHardware) {
  auto pixelBuffer = PixelBuffer::Make(width, height, alphaOnly, tryHardware);
  if (pixelBuffer == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<PixelRef>(new PixelRef(pixelBuffer));
}

PixelRef::PixelRef(std::shared_ptr<PixelBuffer> pixelBuffer) : pixelBuffer(std::move(pixelBuffer)) {
}

void* PixelRef::lockWritablePixels() {
  auto pixels = pixelBuffer->lockPixels();
  if (pixels == nullptr) {
    return nullptr;
  }
  if (!pixelBuffer.unique()) {
    auto& info = pixelBuffer->info();
    auto newBuffer = PixelBuffer::Make(info.width(), info.height(), info.isAlphaOnly(),
                                       pixelBuffer->isHardwareBacked());
    if (newBuffer == nullptr) {
      pixelBuffer->unlockPixels();
      return nullptr;
    }
    auto dstPixels = newBuffer->lockPixels();
    memcpy(dstPixels, pixels, info.byteSize());
    pixelBuffer->unlockPixels();
    pixels = dstPixels;
    pixelBuffer = newBuffer;
  }
  return pixels;
}

std::shared_ptr<Texture> PixelRef::onMakeTexture(Context* context, bool mipMapped) {
  return Texture::MakeFrom(context, pixelBuffer, mipMapped);
}

bool PixelRef::onUpdateTexture(std::shared_ptr<Texture> texture, const Rect& bounds) {
  auto gpu = texture->getContext()->gpu();
  auto pixels = lockPixels();
  if (pixels == nullptr) {
    return false;
  }
  gpu->writePixels(texture->getSampler(), bounds, pixels, info().rowBytes());
  unlockPixels();
  return true;
}
}  // namespace tgfx
