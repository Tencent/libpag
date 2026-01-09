/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 Tencent. All rights reserved.
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
#include <native_window/external_window.h>
#include "base/utils/Log.h"
#include "tgfx/core/Surface.h"

namespace pag {
std::shared_ptr<GPUDrawable> GPUDrawable::FromWindow(NativeWindow* nativeWindow,
                                                     EGLContext sharedContext) {
  if (!nativeWindow) {
    LOGE("GPUDrawable.FromWindow() The nativeWindow is invalid.");
    return nullptr;
  }
  return std::shared_ptr<GPUDrawable>(new GPUDrawable(nativeWindow, sharedContext));
}

GPUDrawable::GPUDrawable(NativeWindow* nativeWindow, EGLContext eglContext)
    : nativeWindow(nativeWindow), sharedContext(eglContext) {
  updateSize();
}

GPUDrawable::~GPUDrawable() {
  OH_NativeWindow_DestroyNativeWindow(nativeWindow);
}

void GPUDrawable::updateSize() {
  OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, GET_BUFFER_GEOMETRY, &_height, &_width);
  if (window) {
    window->invalidSize();
  }
}

std::shared_ptr<tgfx::Device> GPUDrawable::getDevice() {
  if (_width <= 0 || _height <= 0) {
    return nullptr;
  }
  if (!window) {
    window = tgfx::EGLWindow::MakeFrom(reinterpret_cast<EGLNativeWindowType>(nativeWindow),
                                       sharedContext);
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
  if (window == nullptr) {
    return;
  }
  window->present(context);
}

void GPUDrawable::setTimeStamp(int64_t timeStamp) {
  currentTimeStamp = timeStamp;
}

}  // namespace pag
