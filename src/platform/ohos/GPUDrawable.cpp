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

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#include "base/utils/Log.h"
#include "tgfx/gpu/Surface.h"

namespace pag {
std::shared_ptr<GPUDrawable> GPUDrawable::FromXComponent(OH_NativeXComponent* xComponent,
                                                         void* nativeWindow) {
  if (nativeWindow == nullptr || xComponent == nullptr) {
    LOGE("GPUDrawable.FromXComponent() The nativeWindow or xComponent is invalid.");
    return nullptr;
  }
  return std::shared_ptr<GPUDrawable>(new GPUDrawable(xComponent, nativeWindow));
}

GPUDrawable::GPUDrawable(OH_NativeXComponent* xComponent, void* nativeWindow)
    : xComponent(xComponent), nativeWindow(nativeWindow) {
  updateSize();
}

GPUDrawable::~GPUDrawable() {
}

void GPUDrawable::updateSize() {
  uint64_t width = 0;
  uint64_t height = 0;
  OH_NativeXComponent_GetXComponentSize(xComponent, nativeWindow, &width, &height);
  _width = width;
  _height = height;
  if (window) {
    window->invalidSize();
  }
}

std::shared_ptr<tgfx::Device> GPUDrawable::getDevice() {
  if (_width <= 0 || _height <= 0) {
    return nullptr;
  }
  if (!window) {
    window = tgfx::EGLWindow::MakeFrom(reinterpret_cast<EGLNativeWindowType>(nativeWindow));
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
  return window->present(context, currentTimeStamp);
}

void GPUDrawable::setTimeStamp(int64_t timeStamp) {
  currentTimeStamp = timeStamp;
}

}  // namespace pag
