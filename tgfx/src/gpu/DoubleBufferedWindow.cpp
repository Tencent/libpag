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

#include "tgfx/gpu/DoubleBufferedWindow.h"
#include "gpu/Texture.h"

namespace tgfx {
class DoubleBufferedWindowExternal : public DoubleBufferedWindow {
 public:
  DoubleBufferedWindowExternal(std::shared_ptr<Device> device, HardwareBufferRef frontBuffer,
                               HardwareBufferRef backBuffer)
      : DoubleBufferedWindow(std::move(device)), frontBuffer(frontBuffer), backBuffer(backBuffer) {
    HardwareBufferRetain(frontBuffer);
    HardwareBufferRetain(backBuffer);
  }

  ~DoubleBufferedWindowExternal() override {
    HardwareBufferRelease(frontBuffer);
    HardwareBufferRelease(backBuffer);
  }

  bool isFront(HardwareBufferRef buffer) const override {
    return frontBuffer == buffer;
  }

 private:
  std::shared_ptr<Surface> onCreateSurface(Context* context) override {
    if (frontSurface == nullptr) {
      frontSurface = Surface::MakeFrom(context, frontBuffer);
    }
    if (backSurface == nullptr) {
      backSurface = Surface::MakeFrom(context, backBuffer);
    }
    return backSurface;
  }

  void onSwapSurfaces(Context*) override {
    std::swap(frontBuffer, backBuffer);
  }

  HardwareBufferRef frontBuffer = nullptr;
  HardwareBufferRef backBuffer = nullptr;
};

class DoubleBufferedWindowTexture : public DoubleBufferedWindow {
 public:
  DoubleBufferedWindowTexture(std::shared_ptr<Device> device,
                              const BackendTexture& frontBackendTexture,
                              const BackendTexture& backBackendTexture)
      : DoubleBufferedWindow(std::move(device)),
        frontBackendTexture(frontBackendTexture),
        backBackendTexture(backBackendTexture) {
  }

  bool isFront(const BackendTexture& texture) const override {
    if (!texture.isValid() || !frontBackendTexture.isValid()) {
      return false;
    }
    if (texture.backend() != Backend::OPENGL) {
      return false;
    }
    GLTextureInfo info1;
    texture.getGLTextureInfo(&info1);
    GLTextureInfo info2;
    frontBackendTexture.getGLTextureInfo(&info2);
    if (info1.format != info2.format) {
      return false;
    }
    if (info1.target != info2.target) {
      return false;
    }
    return info1.id != info2.id;
  }

 private:
  std::shared_ptr<Surface> onCreateSurface(Context* context) override {
    if (frontSurface == nullptr) {
      frontSurface = Surface::MakeFrom(context, frontBackendTexture, ImageOrigin::TopLeft);
    }
    if (backSurface == nullptr) {
      backSurface = Surface::MakeFrom(context, backBackendTexture, ImageOrigin::TopLeft);
    }
    return backSurface;
  }

  void onSwapSurfaces(Context*) override {
    std::swap(frontBackendTexture, backBackendTexture);
  }

  BackendTexture frontBackendTexture;
  BackendTexture backBackendTexture;
};

std::shared_ptr<DoubleBufferedWindow> DoubleBufferedWindow::Make(std::shared_ptr<Device> device,
                                                                 HardwareBufferRef frontBuffer,
                                                                 HardwareBufferRef backBuffer) {
  if (device == nullptr || frontBuffer == nullptr || backBuffer == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<DoubleBufferedWindow>(
      new DoubleBufferedWindowExternal(std::move(device), frontBuffer, backBuffer));
}

std::shared_ptr<DoubleBufferedWindow> DoubleBufferedWindow::Make(
    std::shared_ptr<Device> device, const BackendTexture& frontBackendTexture,
    const BackendTexture& backBackendTexture) {
  if (device == nullptr || !frontBackendTexture.isValid() || !backBackendTexture.isValid()) {
    return nullptr;
  }
  return std::shared_ptr<DoubleBufferedWindow>(
      new DoubleBufferedWindowTexture(std::move(device), frontBackendTexture, backBackendTexture));
}

DoubleBufferedWindow::DoubleBufferedWindow(std::shared_ptr<Device> device)
    : Window(std::move(device)) {
}

DoubleBufferedWindow::~DoubleBufferedWindow() {
  frontSurface = nullptr;
  backSurface = nullptr;
}

void DoubleBufferedWindow::onPresent(Context* context, int64_t) {
  if (frontSurface == nullptr) {
    return;
  }
  std::swap(frontSurface, backSurface);
  onSwapSurfaces(context);
}
}  // namespace tgfx
