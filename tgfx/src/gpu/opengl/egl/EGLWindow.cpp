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

#if defined(__ANDROID__) || defined(ANDROID)
#include <android/native_window.h>
#include "platform/android/HardwareBuffer.h"
#endif
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include "core/utils/USE.h"
#include "tgfx/gpu/opengl/GLFunctions.h"
#include "tgfx/gpu/opengl/GLRenderTarget.h"

namespace tgfx {
std::shared_ptr<EGLWindow> EGLWindow::Current() {
  auto device = EGLDevice::Current();
  if (device == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<EGLWindow>(new EGLWindow(device));
}

#if defined(__ANDROID__) || defined(ANDROID)
std::shared_ptr<EGLWindow> EGLWindow::MakeFrom(std::shared_ptr<tgfx::HardwareBuffer> hardwareBuffer,
                                               std::shared_ptr<GLDevice> device) {
  if (!hardwareBuffer) {
    return nullptr;
  }
  auto eglDevice = device;
  if (!eglDevice) {
    eglDevice = EGLDevice::Make();
  }
  if (!eglDevice) {
    return nullptr;
  }
  auto eglWindow = std::shared_ptr<EGLWindow>(new EGLWindow(eglDevice));
  eglWindow->hardwareBuffer = hardwareBuffer;
  return eglWindow;
}
#endif

std::shared_ptr<EGLWindow> EGLWindow::MakeFrom(EGLNativeWindowType nativeWindow,
                                               EGLContext sharedContext) {
  if (!nativeWindow) {
    return nullptr;
  }
  auto device = EGLDevice::MakeFrom(nativeWindow, sharedContext);
  if (device == nullptr) {
    return nullptr;
  }
  auto eglWindow = std::shared_ptr<EGLWindow>(new EGLWindow(device));
  eglWindow->nativeWindow = nativeWindow;
  return eglWindow;
}

EGLWindow::EGLWindow(std::shared_ptr<Device> device) : Window(std::move(device)) {
}

std::shared_ptr<Surface> EGLWindow::onCreateSurface(Context* context) {
  EGLint width = 0;
  EGLint height = 0;

  // If the rendering size changes，eglQuerySurface based on ANativeWindow may give the wrong size.
#if defined(__ANDROID__) || defined(ANDROID)
  if (hardwareBuffer) {
    return tgfx::Surface::MakeFrom(hardwareBuffer->makeTexture(context));
  } else if (nativeWindow) {
    width = ANativeWindow_getWidth(nativeWindow);
    height = ANativeWindow_getHeight(nativeWindow);
  }
#endif
  if (width <= 0 || height <= 0) {
    auto eglDevice = static_cast<EGLDevice*>(device.get());
    eglQuerySurface(eglDevice->eglDisplay, eglDevice->eglSurface, EGL_WIDTH, &width);
    eglQuerySurface(eglDevice->eglDisplay, eglDevice->eglSurface, EGL_HEIGHT, &height);
  }

  if (width <= 0 || height <= 0) {
    return nullptr;
  }
  GLFrameBuffer frameBuffer = {};
  frameBuffer.id = 0;
  frameBuffer.format = PixelFormat::RGBA_8888;
  auto renderTarget =
      GLRenderTarget::MakeFrom(context, frameBuffer, width, height, SurfaceOrigin::BottomLeft);
  return Surface::MakeFrom(renderTarget);
}

void EGLWindow::onPresent(Context* context, int64_t presentationTime) {
  USE(context);
#if defined(__ANDROID__) || defined(ANDROID)
  if (hardwareBuffer) {
    auto gl = GLFunctions::Get(context);
    gl->flush();
    return;
  }
#endif
  std::static_pointer_cast<EGLDevice>(device)->swapBuffers(presentationTime);
}
}  // namespace tgfx