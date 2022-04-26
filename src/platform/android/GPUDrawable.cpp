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

#include "GPUDrawable.h"
#include "base/utils/Log.h"
#include "tgfx/gpu/Surface.h"

namespace pag {
std::shared_ptr<GPUDrawable> GPUDrawable::FromWindow(ANativeWindow* nativeWindow,
                                                     EGLContext sharedContext) {
  if (nativeWindow == nullptr) {
    LOGE("GPUDrawable.FromWindow() The nativeWindow is invalid.");
    return nullptr;
  }
  return std::shared_ptr<GPUDrawable>(new GPUDrawable(nativeWindow, sharedContext));
}

GPUDrawable::GPUDrawable(ANativeWindow* nativeWindow, EGLContext eglContext)
    : nativeWindow(nativeWindow), sharedContext(eglContext) {
  updateSize();
}

GPUDrawable::~GPUDrawable() {
  ANativeWindow_release(nativeWindow);
}

void GPUDrawable::updateSize() {
  if (nativeWindow != nullptr) {
    _width = ANativeWindow_getWidth(nativeWindow);
    _height = ANativeWindow_getHeight(nativeWindow);
  }
}

std::shared_ptr<tgfx::Device> GPUDrawable::getDevice() {
  if (_width <= 0 || _height <= 0) {
    return nullptr;
  }
  if (window == nullptr) {
    window = tgfx::EGLWindow::MakeFrom(nativeWindow, sharedContext);
  }
  return window ? window->getDevice() : nullptr;
}

std::shared_ptr<tgfx::Surface> GPUDrawable::createSurface(tgfx::Context* context) {
  return window ? window->createSurface(context) : nullptr;
}

void GPUDrawable::present(tgfx::Context* context) {
  if (window == nullptr) {
    return;
  }
  return window->present(context, currentTimeStamp);
}

void GPUDrawable::setTimeStamp(int64_t timeStamp) {
  currentTimeStamp = timeStamp;
}

}  // namespace pag
