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

#include "tgfx/core/PixelBuffer.h"
#include "tgfx/gpu/Device.h"

namespace tgfx {
class RasterPixelBuffer : public PixelBuffer {
 public:
  RasterPixelBuffer(const ImageInfo& info, uint8_t* pixels) : PixelBuffer(info), _pixels(pixels) {
  }

  ~RasterPixelBuffer() override {
    delete[] _pixels;
  }

  void* lockPixels() override {
    locker.lock();
    return _pixels;
  }

  void unlockPixels() override {
    locker.unlock();
  }

 protected:
  std::shared_ptr<Texture> onMakeTexture(Context* context, bool mipMapped) const override {
    std::lock_guard<std::mutex> autoLock(locker);
    if (isAlphaOnly()) {
      return Texture::MakeAlpha(context, _info.width(), _info.height(), _pixels, _info.rowBytes(),
                                SurfaceOrigin::TopLeft, mipMapped);
    }
    return Texture::MakeRGBA(context, _info.width(), _info.height(), _pixels, _info.rowBytes(),
                             SurfaceOrigin::TopLeft, mipMapped);
  }

 private:
  mutable std::mutex locker = {};
  uint8_t* _pixels = nullptr;
};

std::shared_ptr<PixelBuffer> PixelBuffer::Make(int width, int height, bool alphaOnly,
                                               bool tryHardware) {
  if (width <= 0 || height <= 0) {
    return nullptr;
  }
  std::shared_ptr<PixelBuffer> pixelBuffer = nullptr;
  if (tryHardware) {
    pixelBuffer = MakeHardwareBuffer(width, height, alphaOnly);
    if (pixelBuffer != nullptr) {
      pixelBuffer->hardwareBacked = true;
      return pixelBuffer;
    }
  }

  auto colorType = alphaOnly ? ColorType::ALPHA_8 : ColorType::RGBA_8888;
  auto info = ImageInfo::Make(width, height, colorType);
  if (info.isEmpty()) {
    return nullptr;
  }
  auto pixels = new (std::nothrow) uint8_t[info.byteSize()];
  if (pixels == nullptr) {
    return nullptr;
  }
  return std::make_shared<RasterPixelBuffer>(info, pixels);
}
}  // namespace tgfx
