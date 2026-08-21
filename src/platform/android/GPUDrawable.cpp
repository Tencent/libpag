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
#include "EGLResourceDisposer.h"
#include "base/utils/Log.h"
#include "tgfx/core/Surface.h"

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
  // Destroy the cached Surface and the EGLWindow on the disposer thread: the final window
  // release runs ~EGLDevice, whose eglDestroyContext() can block for a long time on some
  // drivers (e.g. PowerVR on MediaTek devices, which waits for the GPU to drain), and this
  // destructor runs on the main thread when a PAGView is detached from the window (issue #3685).
  // The ANativeWindow is safe to release here because the EGLSurface created from it holds its
  // own reference until eglDestroySurface() runs on the disposer thread.
  EGLResourceDisposer::DisposeAsync(std::move(surface), std::move(window));
}

void GPUDrawable::updateSize() {
  _width = ANativeWindow_getWidth(nativeWindow);
  _height = ANativeWindow_getHeight(nativeWindow);
}

void GPUDrawable::freeSurface() {
  // Drop the cached Surface reference on the disposer thread instead of destroying it inline.
  // Dropping the last reference can block the calling thread for tens of milliseconds on some
  // devices (measured 35~85ms on a PowerVR GE8320 phone), and this method runs on the main
  // thread when a PAGView is detached from the window or its surface is resized (issue #3685).
  // The window reference is kept here: the EGL context is only destroyed when the GPUDrawable
  // itself is destroyed. The disposer processes tasks in FIFO order, so a Surface enqueued
  // earlier is always destroyed before a window enqueued later, keeping its Context alive.
  EGLResourceDisposer::DisposeAsync(std::move(surface), nullptr);
}

std::shared_ptr<tgfx::Device> GPUDrawable::getDevice() {
  if (_width <= 0 || _height <= 0) {
    return nullptr;
  }
  if (!window) {
    window = tgfx::EGLWindow::MakeFrom(nativeWindow, sharedContext);
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
