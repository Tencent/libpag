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

#include "tgfx/gpu/opengl/egl/EGLDevice.h"
#include "core/utils/Log.h"
#include "gpu/opengl/egl/EGLGlobals.h"
#include "gpu/opengl/egl/EGLProcGetter.h"

namespace tgfx {
static EGLContext CreateContext(EGLContext sharedContext, EGLConfig eglConfig) {
  static auto eglGlobals = EGLGlobals::Get();
  static const EGLint context3Attributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
  auto eglContext =
      eglCreateContext(eglGlobals->display, eglConfig, sharedContext, context3Attributes);
  if (eglContext != EGL_NO_CONTEXT) {
    return eglContext;
  }
  LOGE("EGLDevice CreateContext() version 3: error=%d", eglGetError());
  static const EGLint context2Attributes[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  eglContext = eglCreateContext(eglGlobals->display, eglConfig, sharedContext, context2Attributes);
  if (eglContext == EGL_NO_CONTEXT) {
    LOGE("EGLDevice CreateContext() version 2: error=%d", eglGetError());
  }
  return eglContext;
}

void* GLDevice::CurrentNativeHandle() {
  return eglGetCurrentContext();
}

std::shared_ptr<GLDevice> GLDevice::Current() {
  auto eglContext = eglGetCurrentContext();
  auto eglDisplay = eglGetCurrentDisplay();
  auto eglSurface = eglGetCurrentSurface(EGL_DRAW);
  return EGLDevice::Wrap(eglDisplay, eglSurface, eglContext, nullptr, true);
}

std::shared_ptr<GLDevice> GLDevice::Make(void* sharedContext) {
  static auto eglGlobals = EGLGlobals::Get();
  auto eglContext = EGL_NO_CONTEXT;
  EGLint surfaceAttributes[] = {EGL_WIDTH,           1,        EGL_HEIGHT, 1,
                                EGL_LARGEST_PBUFFER, EGL_TRUE, EGL_NONE};
  auto eglSurface =
      eglCreatePbufferSurface(eglGlobals->display, eglGlobals->pbufferConfig, surfaceAttributes);
  if (eglSurface == nullptr) {
    LOGE("GLDevice::Make() eglCreatePbufferSurface error=%d", eglGetError());
    return nullptr;
  }
  auto eglShareContext = reinterpret_cast<EGLContext>(sharedContext);
  eglContext = CreateContext(eglShareContext, eglGlobals->pbufferConfig);
  if (eglContext == nullptr) {
    eglDestroySurface(eglGlobals->display, eglSurface);
    return nullptr;
  }
  auto device =
      EGLDevice::Wrap(eglGlobals->display, eglSurface, eglContext, eglShareContext, false);
  if (device == nullptr) {
    eglDestroyContext(eglGlobals->display, eglContext);
    eglDestroySurface(eglGlobals->display, eglSurface);
  }
  return device;
}

std::shared_ptr<EGLDevice> EGLDevice::MakeAdopted(EGLDisplay eglDisplay, EGLSurface eglSurface,
                                                  EGLContext eglContext) {
  return EGLDevice::Wrap(eglDisplay, eglSurface, eglContext, nullptr, true);
}

std::shared_ptr<EGLDevice> EGLDevice::MakeFrom(EGLNativeWindowType nativeWindow,
                                               EGLContext sharedContext) {
  static auto eglGlobals = EGLGlobals::Get();
  auto eglSurface =
      eglCreateWindowSurface(eglGlobals->display, eglGlobals->windowConfig, nativeWindow, nullptr);
  if (eglSurface == nullptr) {
    LOGE("EGLDevice::MakeFrom() eglCreateWindowSurface error=%d", eglGetError());
    return nullptr;
  }
  auto eglContext = CreateContext(sharedContext, eglGlobals->windowConfig);
  if (eglContext == EGL_NO_CONTEXT) {
    eglDestroySurface(eglGlobals->display, eglSurface);
    return nullptr;
  }
  auto device = EGLDevice::Wrap(eglGlobals->display, eglSurface, eglContext, sharedContext, false);
  if (device == nullptr) {
    eglDestroyContext(eglGlobals->display, eglContext);
    eglDestroySurface(eglGlobals->display, eglSurface);
  }
  return device;
}

std::shared_ptr<EGLDevice> EGLDevice::Wrap(EGLDisplay eglDisplay, EGLSurface eglSurface,
                                           EGLContext eglContext, EGLContext shareContext,
                                           bool isAdopted) {
  auto glContext = GLDevice::Get(eglContext);
  if (glContext) {
    return std::static_pointer_cast<EGLDevice>(glContext);
  }
  if (eglDisplay == nullptr || eglContext == nullptr || eglSurface == nullptr) {
    return nullptr;
  }
  EGLContext oldEglContext = eglGetCurrentContext();
  EGLDisplay oldEglDisplay = eglGetCurrentDisplay();
  EGLSurface oldEglReadSurface = eglGetCurrentSurface(EGL_READ);
  EGLSurface oldEglDrawSurface = eglGetCurrentSurface(EGL_DRAW);
  if (oldEglContext != eglContext) {
    auto result = eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);
    if (!result) {
      eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      if (oldEglDisplay) {
        eglMakeCurrent(oldEglDisplay, oldEglDrawSurface, oldEglReadSurface, oldEglContext);
      }
      return nullptr;
    }
  }
  auto device = std::shared_ptr<EGLDevice>(new EGLDevice(eglContext));
  device->isAdopted = isAdopted;
  device->eglDisplay = eglDisplay;
  device->eglSurface = eglSurface;
  device->eglContext = eglContext;
  device->shareContext = shareContext;
  device->weakThis = device;
  if (oldEglContext != eglContext) {
    eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (oldEglDisplay) {
      eglMakeCurrent(oldEglDisplay, oldEglDrawSurface, oldEglReadSurface, oldEglContext);
    }
  }
  return device;
}

EGLDevice::EGLDevice(void* nativeHandle) : GLDevice(nativeHandle) {
}

EGLDevice::~EGLDevice() {
  releaseAll();
  if (isAdopted) {
    return;
  }
  eglDestroyContext(eglDisplay, eglContext);
  eglDestroySurface(eglDisplay, eglSurface);
}

bool EGLDevice::sharableWith(void* nativeContext) const {
  return nativeHandle == nativeContext || shareContext == nativeContext;
}

bool EGLDevice::onMakeCurrent() {
  oldEglContext = eglGetCurrentContext();
  oldEglDisplay = eglGetCurrentDisplay();
  oldEglReadSurface = eglGetCurrentSurface(EGL_READ);
  oldEglDrawSurface = eglGetCurrentSurface(EGL_DRAW);
  if (oldEglContext == eglContext) {
    // 如果外部已经设定好了当前的 Context，以外部设定的为准，不再切换。外部用于 read/draw 的 Surface
    // 有可能不一样。
    return true;
  }
  auto result = eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);
  if (!result) {
    LOGE("EGLDevice::onMakeCurrent() failure result = %d error= %d", result, eglGetError());
    return false;
  }
  return true;
}

void EGLDevice::onClearCurrent() {
  if (oldEglContext == eglContext) {
    return;
  }
  eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  if (oldEglDisplay) {
    // 可能失败。
    eglMakeCurrent(oldEglDisplay, oldEglDrawSurface, oldEglReadSurface, oldEglContext);
  }
}

void EGLDevice::swapBuffers(int64_t timestamp) {
  if (timestamp != INT64_MIN) {
    static auto eglPresentationTimeANDROID = reinterpret_cast<PFNEGLPRESENTATIONTIMEANDROIDPROC>(
        eglGetProcAddress("eglPresentationTimeANDROID"));
    if (eglPresentationTimeANDROID) {
      // egl uses nano seconds
      eglPresentationTimeANDROID(eglDisplay, eglSurface, timestamp * 1000);
    }
  }
  eglSwapBuffers(eglDisplay, eglSurface);
}
}  // namespace tgfx