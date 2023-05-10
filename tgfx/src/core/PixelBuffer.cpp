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

#include "core/PixelBuffer.h"
#include "tgfx/gpu/Device.h"
#include "utils/PixelFormatUtil.h"

namespace tgfx {
class RasterPixelBuffer : public PixelBuffer {
 public:
  RasterPixelBuffer(const ImageInfo& info, uint8_t* pixels) : PixelBuffer(info), _pixels(pixels) {
  }

  ~RasterPixelBuffer() override {
    delete[] _pixels;
  }

  bool isHardwareBacked() const override {
    return false;
  }

  HardwareBufferRef getHardwareBuffer() const override {
    return nullptr;
  }

 protected:
  void* onLockPixels() const override {
    return _pixels;
  }

  void onUnlockPixels() const override {
  }

  std::shared_ptr<Texture> onBindToHardwareTexture(Context*) const override {
    return nullptr;
  }

 private:
  uint8_t* _pixels = nullptr;
};

class HardwarePixelBuffer : public PixelBuffer {
 public:
  HardwarePixelBuffer(const ImageInfo& info, HardwareBufferRef hardwareBuffer)
      : PixelBuffer(info), hardwareBuffer(HardwareBufferRetain(hardwareBuffer)) {
  }

  ~HardwarePixelBuffer() override {
    HardwareBufferRelease(hardwareBuffer);
  }

  bool isHardwareBacked() const override {
    return HardwareBufferCheck(hardwareBuffer);
  }

  HardwareBufferRef getHardwareBuffer() const override {
    return isHardwareBacked() ? hardwareBuffer : nullptr;
  }

 protected:
  void* onLockPixels() const override {
    return HardwareBufferLock(hardwareBuffer);
  }

  void onUnlockPixels() const override {
    HardwareBufferUnlock(hardwareBuffer);
  }

  std::shared_ptr<Texture> onBindToHardwareTexture(Context* context) const override {
    return Texture::MakeFrom(context, hardwareBuffer);
  }

 private:
  HardwareBufferRef hardwareBuffer;
};

std::shared_ptr<PixelBuffer> PixelBuffer::Make(int width, int height, bool alphaOnly,
                                               bool tryHardware) {
  if (width <= 0 || height <= 0) {
    return nullptr;
  }
  if (tryHardware && tgfx::HardwareBufferAvailable()) {
    auto hardwareBuffer = HardwareBufferAllocate(width, height, alphaOnly);
    auto pixelBuffer = PixelBuffer::MakeFrom(hardwareBuffer);
    tgfx::HardwareBufferRelease(hardwareBuffer);
    if (pixelBuffer != nullptr) {
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

std::shared_ptr<PixelBuffer> PixelBuffer::MakeFrom(HardwareBufferRef hardwareBuffer) {
  auto info = HardwareBufferGetInfo(hardwareBuffer);
  return info.isEmpty() ? nullptr : std::make_shared<HardwarePixelBuffer>(info, hardwareBuffer);
}

PixelBuffer::PixelBuffer(const tgfx::ImageInfo& info) : _info(info) {
}

void* PixelBuffer::lockPixels() {
  locker.lock();
  auto pixels = onLockPixels();
  if (pixels == nullptr) {
    locker.unlock();
  }
  return pixels;
}

void PixelBuffer::unlockPixels() {
  onUnlockPixels();
  locker.unlock();
}

std::shared_ptr<Texture> PixelBuffer::onMakeTexture(Context* context, bool mipMapped) const {
  std::lock_guard<std::mutex> autoLock(locker);
  if (!mipMapped && isHardwareBacked()) {
    return onBindToHardwareTexture(context);
  }
  auto pixels = onLockPixels();
  if (pixels == nullptr) {
    return nullptr;
  }
  auto format = ColorTypeToPixelFormat(_info.colorType());
  auto texture = Texture::MakeFormat(context, width(), height(), pixels, _info.rowBytes(), format,
                                     ImageOrigin::TopLeft, mipMapped);
  onUnlockPixels();
  return texture;
}
}  // namespace tgfx
