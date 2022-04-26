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

#include "tgfx/gpu/opengl/egl/EGLWindow.h"
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include "tgfx/gpu/opengl/GLRenderTarget.h"

namespace tgfx {
std::shared_ptr<EGLWindow> EGLWindow::Current() {
  auto device = EGLDevice::Current();
  if (device == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<EGLWindow>(new EGLWindow(device));
}

std::shared_ptr<EGLWindow> EGLWindow::MakeFrom(EGLNativeWindowType nativeWindow,
                                               EGLContext sharedContext) {
  if (!nativeWindow) {
    return nullptr;
  }
  auto device = EGLDevice::MakeFrom(nativeWindow, sharedContext);
  if (device == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<EGLWindow>(new EGLWindow(device));
}

EGLWindow::EGLWindow(std::shared_ptr<Device> device) : Window(std::move(device)) {
}

std::shared_ptr<Surface> EGLWindow::onCreateSurface(Context* context) {
  EGLint width = 0;
  EGLint height = 0;
  auto eglDevice = static_cast<EGLDevice*>(device.get());
  eglQuerySurface(eglDevice->eglDisplay, eglDevice->eglSurface, EGL_WIDTH, &width);
  eglQuerySurface(eglDevice->eglDisplay, eglDevice->eglSurface, EGL_HEIGHT, &height);
  if (width <= 0 || height <= 0) {
    return nullptr;
  }
  GLFrameBuffer frameBuffer = {};
  frameBuffer.id = 0;
  frameBuffer.format = PixelFormat::RGBA_8888;
  auto renderTarget =
      GLRenderTarget::MakeFrom(context, frameBuffer, width, height, ImageOrigin::BottomLeft);
  return Surface::MakeFrom(renderTarget);
}

void EGLWindow::onPresent(Context*, int64_t presentationTime) {
  std::static_pointer_cast<EGLDevice>(device)->swapBuffers(presentationTime);
}
}  // namespace tgfx