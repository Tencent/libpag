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

#include <android/native_window.h>
#include <android/native_window_jni.h>
#include "rendering/Drawable.h"
#include "tgfx/gpu/opengl/egl/EGLWindow.h"
#include "tgfx/src/platform/android/HardwareBuffer.h"

namespace pag {

class GPUDrawable : public Drawable {
 public:
  static std::shared_ptr<GPUDrawable> FromWindow(ANativeWindow* nativeWindow,
                                                 EGLContext sharedContext = EGL_NO_CONTEXT);

  static std::shared_ptr<GPUDrawable> FromHardwareBuffer(
      std::shared_ptr<tgfx::HardwareBuffer> hardwareBuffer);

  ~GPUDrawable() override;

  int width() const override {
    return _width;
  }

  int height() const override {
    return _height;
  }

  void updateSize() override;

  std::shared_ptr<tgfx::Surface> createSurface(tgfx::Context* context) override;

  void present(tgfx::Context* context) override;

  void setTimeStamp(int64_t timeStamp) override;

 protected:
  std::shared_ptr<tgfx::Device> getDevice() override;

 private:
  int _width = 0;
  int _height = 0;
  ANativeWindow* nativeWindow = nullptr;
  EGLContext sharedContext = nullptr;
  std::shared_ptr<tgfx::HardwareBuffer> hardwareBuffer;
  int64_t currentTimeStamp = 0;
  std::shared_ptr<tgfx::EGLWindow> window = nullptr;

  explicit GPUDrawable(ANativeWindow* nativeWindow, EGLContext eglContext = EGL_NO_CONTEXT);
  explicit GPUDrawable(std::shared_ptr<tgfx::HardwareBuffer> hardwareBuffer);
};
}  // namespace pag
