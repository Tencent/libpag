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

#include "Bitmap.h"
#include "Image.h"
#include "PixelMap.h"
#include "image/PixelBuffer.h"
#include "platform/Platform.h"

namespace pag {
class EmptyPixelBuffer : public PixelBuffer {
 public:
  EmptyPixelBuffer() : PixelBuffer({}) {
  }

  void* lockPixels() override {
    return nullptr;
  }

  void unlockPixels() override {
  }

  std::shared_ptr<Texture> makeTexture(Context*) const override {
    return nullptr;
  }
};

std::shared_ptr<PixelBuffer> MakeEmptyPixelBuffer() {
  static auto emptyBuffer = std::make_shared<EmptyPixelBuffer>();
  return emptyBuffer;
}

Bitmap::Bitmap() : pixelBuffer(MakeEmptyPixelBuffer()) {
}

Bitmap::Bitmap(std::shared_ptr<PixelBuffer> buffer) : pixelBuffer(std::move(buffer)) {
  if (pixelBuffer == nullptr) {
    pixelBuffer = MakeEmptyPixelBuffer();
  }
}

bool Bitmap::allocPixels(int width, int height, bool alphaOnly, bool tryHardware) {
  auto buffer = PixelBuffer::Make(width, height, alphaOnly, tryHardware);
  if (buffer == nullptr) {
    reset();
    return false;
  }
  pixelBuffer = buffer;
  return true;
}

void Bitmap::reset() {
  pixelBuffer = MakeEmptyPixelBuffer();
}

int Bitmap::width() const {
  return info().width();
}

int Bitmap::height() const {
  return info().height();
}

AlphaType Bitmap::alphaType() const {
  return info().alphaType();
}

ColorType Bitmap::colorType() const {
  return info().colorType();
}

size_t Bitmap::rowBytes() const {
  return info().rowBytes();
}

size_t Bitmap::byteSize() const {
  return info().byteSize();
}

bool Bitmap::isEmpty() const {
  return info().isEmpty();
}

const ImageInfo& Bitmap::info() const {
  return pixelBuffer->info();
}

void* Bitmap::lockPixels() const {
  return pixelBuffer->lockPixels();
}

void Bitmap::unlockPixels() const {
  pixelBuffer->unlockPixels();
}

void Bitmap::eraseAll() {
  pixelBuffer->eraseAll();
}

std::shared_ptr<Data> Bitmap::encode(EncodedFormat format, int quality) const {
  auto srcPixels = pixelBuffer->lockPixels();
  auto byteData = Image::Encode(info(), srcPixels, format, quality);
  pixelBuffer->unlockPixels();
  return byteData;
}

std::shared_ptr<Texture> Bitmap::makeTexture(Context* context) const {
  return pixelBuffer->makeTexture(context);
}

bool Bitmap::readPixels(const ImageInfo& dstInfo, void* dstPixels, int srcX, int srcY) const {
  auto srcPixels = pixelBuffer->lockPixels();
  PixelMap pixelMap(info(), srcPixels);
  auto result = pixelMap.readPixels(dstInfo, dstPixels, srcX, srcY);
  pixelBuffer->unlockPixels();
  return result;
}

bool Bitmap::writePixels(const ImageInfo& srcInfo, const void* srcPixels, int dstX, int dstY) {
  auto dstPixels = pixelBuffer->lockPixels();
  PixelMap pixelMap(srcInfo, srcPixels);
  auto result = pixelMap.readPixels(info(), dstPixels, -dstX, -dstY);
  pixelBuffer->unlockPixels();
  return result;
}

void Trace(const Bitmap& bitmap, const std::string& tag) {
  BitmapLock lock(bitmap);
  PixelMap pixelMap(bitmap.info(), lock.pixels());
  Platform::Current()->traceImage(pixelMap, tag);
}
}  // namespace pag
