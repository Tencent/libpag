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

#pragma once
#include "rendering/Drawable.h"
#include "tgfx/src/platform/android/HardwareBuffer.h"

namespace pag {
class HardwareBufferDrawable : public Drawable {
 public:
  static std::shared_ptr<HardwareBufferDrawable> Make(int width, int height);

  static std::shared_ptr<HardwareBufferDrawable> Make(std::shared_ptr<tgfx::HardwareBuffer> buffer,
                                                      std::shared_ptr<tgfx::Device> device);

  int width() const override {
    return _width;
  }
  int height() const override {
    return _height;
  }

  void updateSize() override {
  }

  std::shared_ptr<tgfx::Surface> createSurface(tgfx::Context* context) override;

  void present(tgfx::Context*) override;

  bool readPixels(tgfx::ColorType colorType, tgfx::AlphaType alphaType, void* dstPixels,
                  size_t dstRowBytes);

  std::shared_ptr<tgfx::Device> getDevice() override {
    return device;
  }

  AHardwareBuffer* aHardwareBuffer() {
    if (hardwareBuffer) {
      return hardwareBuffer->aHardwareBuffer();
    }
    return nullptr;
  }

 private:
  HardwareBufferDrawable(std::shared_ptr<tgfx::Device> device,
                         std::shared_ptr<tgfx::HardwareBuffer> buffer);
  int _width = 0;
  int _height = 0;
  std::shared_ptr<tgfx::Device> device = nullptr;
  std::shared_ptr<tgfx::HardwareBuffer> hardwareBuffer;
};

}  // namespace pag