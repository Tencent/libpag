/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "GPUDrawable.h"
#include <windows.h>
#include "base/utils/Log.h"
#ifdef PAG_USE_ANGLE
#include "tgfx/gpu/opengl/egl/EGLWindow.h"
#else
#include "tgfx/gpu/opengl/wgl/WGLWindow.h"
#endif

namespace pag {
std::shared_ptr<GPUDrawable> GPUDrawable::FromWindow(void* nativeWindow, void* sharedContext) {
  if (nativeWindow == nullptr) {
    LOGE("GPUDrawable.FromWindow() The nativeWindow is invalid.");
    return nullptr;
  }
  return std::shared_ptr<GPUDrawable>(new GPUDrawable(nativeWindow, sharedContext));
}

GPUDrawable::GPUDrawable(void* nativeWindow, void* sharedContext)
    : nativeWindow(nativeWindow), sharedContext(sharedContext) {
  GPUDrawable::updateSize();
}

void GPUDrawable::updateSize() {
  if (nativeWindow != nullptr) {
    RECT rect = {};
    if (GetWindowRect(static_cast<HWND>(nativeWindow), &rect)) {
      _width = rect.right - rect.left;
      _height = rect.bottom - rect.top;
    } else {
      _width = 0;
      _height = 0;
    }
    if (window) {
      window->invalidSize();
    }
  }
}

std::shared_ptr<tgfx::Device> GPUDrawable::getDevice() {
  if (_width <= 0 || _height <= 0) {
    return nullptr;
  }
  if (window == nullptr) {
#ifdef PAG_USE_ANGLE
    window = tgfx::EGLWindow::MakeFrom(reinterpret_cast<EGLNativeWindowType>(nativeWindow),
                                       sharedContext);
#else
    window = tgfx::WGLWindow::MakeFrom(reinterpret_cast<HWND>(nativeWindow),
                                       reinterpret_cast<HGLRC>(sharedContext));
#endif
  }
  return window ? window->getDevice() : nullptr;
}

std::shared_ptr<tgfx::Surface> GPUDrawable::onCreateSurface(tgfx::Context* context) {
  return window ? window->getSurface(context) : nullptr;
}

void GPUDrawable::onFreeSurface() {
  if (window) {
    window->freeSurface();
  }
}

void GPUDrawable::present(tgfx::Context* context) {
  window->present(context);
}
}  // namespace pag
