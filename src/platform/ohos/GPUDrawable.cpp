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
                                                     EGLContext sharedContext, bool ownsWindow) {
  if (!nativeWindow) {
    LOGE("GPUDrawable.FromWindow() The nativeWindow is invalid.");
    return nullptr;
  }
  return std::shared_ptr<GPUDrawable>(new GPUDrawable(nativeWindow, sharedContext, ownsWindow));
}

GPUDrawable::GPUDrawable(NativeWindow* nativeWindow, EGLContext eglContext, bool ownsWindow)
    : nativeWindow(nativeWindow), sharedContext(eglContext), ownsWindow(ownsWindow) {
  updateSize();
}

GPUDrawable::~GPUDrawable() {
  if (ownsWindow && nativeWindow) {
    OH_NativeWindow_DestroyNativeWindow(nativeWindow);
  }
}

void GPUDrawable::updateSize() {
  OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, GET_BUFFER_GEOMETRY, &_height, &_width);
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
  if (window == nullptr) {
    return nullptr;
  }
  return tgfx::Surface::MakeFrom(context, window);
}

void GPUDrawable::onFreeSurface() {
}

void GPUDrawable::present(tgfx::Context*) {
  if (window == nullptr) {
    return;
  }
  window->setPresentationTime(currentTimeStamp);
  // In the new tgfx architecture, Window::onPresent() is called automatically by
  // DrawingBuffer::presentWindows() after command submission.
}

void GPUDrawable::setTimeStamp(int64_t timeStamp) {
  currentTimeStamp = timeStamp;
}

}  // namespace pag
