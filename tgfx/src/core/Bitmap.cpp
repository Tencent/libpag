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

#include "tgfx/core/Bitmap.h"
#include "core/PixelRef.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Pixmap.h"

namespace tgfx {
Bitmap::Bitmap(int width, int height, bool alphaOnly, bool tryHardware) {
  allocPixels(width, height, alphaOnly, tryHardware);
}

Bitmap::Bitmap(const Bitmap& src) : _info(src._info), pixelRef(src.pixelRef) {
}

Bitmap::Bitmap(Bitmap&& src) : _info(src._info), pixelRef(std::move(src.pixelRef)) {
}

Bitmap::Bitmap(HardwareBufferRef hardwareBuffer) {
  auto pixelBuffer = PixelBuffer::MakeFrom(hardwareBuffer);
  pixelRef = PixelRef::Wrap(pixelBuffer);
  if (pixelRef != nullptr) {
    _info = pixelRef->info();
  }
}

Bitmap& Bitmap::operator=(const Bitmap& src) {
  if (this != &src) {
    _info = src._info;
    pixelRef = src.pixelRef;
  }
  return *this;
}

Bitmap& Bitmap::operator=(Bitmap&& src) {
  if (this != &src) {
    _info = src._info;
    pixelRef = std::move(src.pixelRef);
  }
  return *this;
}

bool Bitmap::allocPixels(int width, int height, bool alphaOnly, bool tryHardware) {
  pixelRef = PixelRef::Make(width, height, alphaOnly, tryHardware);
  if (pixelRef != nullptr) {
    _info = pixelRef->info();
  }
  return pixelRef != nullptr;
}

void* Bitmap::lockPixels() {
  if (pixelRef == nullptr) {
    return nullptr;
  }
  auto pixels = pixelRef->lockWritablePixels();
  if (pixels != nullptr) {
    pixelRef->markContentDirty(Rect::MakeWH(width(), height()));
  }
  return pixels;
}

const void* Bitmap::lockPixels() const {
  if (pixelRef == nullptr) {
    return nullptr;
  }
  return pixelRef->lockPixels();
}

void Bitmap::unlockPixels() const {
  if (pixelRef != nullptr) {
    pixelRef->unlockPixels();
  }
}

bool Bitmap::isHardwareBacked() const {
  return pixelRef ? pixelRef->isHardwareBacked() : false;
}

HardwareBufferRef Bitmap::getHardwareBuffer() const {
  return pixelRef ? pixelRef->getHardwareBuffer() : nullptr;
}

std::shared_ptr<Data> Bitmap::encode(EncodedFormat format, int quality) const {
  Pixmap pixmap(*this);
  return ImageCodec::Encode(pixmap, format, quality);
}

Color Bitmap::getColor(int x, int y) const {
  return Pixmap(*this).getColor(x, y);
}

bool Bitmap::readPixels(const ImageInfo& dstInfo, void* dstPixels, int srcX, int srcY) const {
  return Pixmap(*this).readPixels(dstInfo, dstPixels, srcX, srcY);
}

bool Bitmap::writePixels(const ImageInfo& srcInfo, const void* srcPixels, int dstX, int dstY) {
  auto success = Pixmap(*this).writePixels(srcInfo, srcPixels, dstX, dstY);
  if (success) {
    pixelRef->markContentDirty(Rect::MakeXYWH(dstX, dstY, srcInfo.width(), srcInfo.height()));
  }
  return success;
}

void Bitmap::clear() {
  if (pixelRef == nullptr) {
    return;
  }
  pixelRef->clear();
}

std::shared_ptr<ImageBuffer> Bitmap::makeBuffer() const {
  if (pixelRef == nullptr) {
    return nullptr;
  }
  return pixelRef->makeBuffer();
}
}  // namespace tgfx
