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

#include "BitmapBuffer.h"
#include "HardwareBufferUtil.h"

namespace pag {

std::shared_ptr<BitmapBuffer> BitmapBuffer::Wrap(pag::HardwareBufferRef hardwareBuffer) {
  auto hwInfo = tgfx::HardwareBufferGetInfo(hardwareBuffer);
  if (hwInfo.width <= 0 || hwInfo.height <= 0) {
    return nullptr;
  }
  auto info = HardwareBufferInfoToImageInfo(hwInfo);
  if (info.isEmpty()) {
    return nullptr;
  }
  auto bitmap = std::shared_ptr<BitmapBuffer>(new BitmapBuffer(info));
  bitmap->hardwareBuffer = hardwareBuffer;
  bitmap->hardwareBacked = tgfx::HardwareBufferCheck(hardwareBuffer);
  return bitmap;
}

std::shared_ptr<BitmapBuffer> BitmapBuffer::Wrap(const tgfx::ImageInfo& info, void* pixels) {
  if (info.isEmpty()) {
    return nullptr;
  }
  auto bitmap = std::shared_ptr<BitmapBuffer>(new BitmapBuffer(info));
  bitmap->pixels = pixels;
  return bitmap;
}

BitmapBuffer::BitmapBuffer(const tgfx::ImageInfo& info) : _info(info) {
}

HardwareBufferRef BitmapBuffer::getHardwareBuffer() const {
  return hardwareBacked ? hardwareBuffer : nullptr;
}

void* BitmapBuffer::lockPixels() {
  return hardwareBuffer != nullptr ? tgfx::HardwareBufferLock(hardwareBuffer) : pixels;
}

void BitmapBuffer::unlockPixels() {
  if (hardwareBuffer != nullptr) {
    tgfx::HardwareBufferUnlock(hardwareBuffer);
  }
}

}  // namespace pag
