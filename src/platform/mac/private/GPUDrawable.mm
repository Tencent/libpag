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
#include "tgfx/core/Surface.h"
#include "tgfx/gpu/opengl/cgl/CGLWindow.h"

namespace pag {
NSString* const kAsyncSurfacePreparedNotification = @"io.pag.AsyncSurfacePrepared";

std::shared_ptr<GPUDrawable> GPUDrawable::FromView(NSView* view) {
  if (view == nil) {
    return nullptr;
  }
  auto drawable = std::shared_ptr<GPUDrawable>(new GPUDrawable(view));
  drawable->weakThis = drawable;
  return drawable;
}

GPUDrawable::GPUDrawable(NSView* view) : view(view) {
  // do not retain view here, otherwise it can cause circular reference.
  updateSize();
  tryCreateSurface();
}

void GPUDrawable::tryCreateSurface() {
  // try to create the surface if the view is ready, because CGLWindow needs to access the NSView
  // geometry on the main thread.
  if (!NSThread.isMainThread || _width <= 0 || _height <= 0) {
    return;
  }
  if (window == nullptr) {
    window = tgfx::CGLWindow::MakeFrom(view);
  }
  if (window != nullptr) {
    auto device = window->getDevice();
    auto context = device->lockContext();
    if (context != nullptr) {
      surface = tgfx::Surface::MakeFrom(context, window);
      device->unlock();
    }
  }
}

void GPUDrawable::updateSize() {
  CGSize size = [view convertSizeToBacking:view.bounds.size];
  _width = static_cast<int>(roundf(size.width));
  _height = static_cast<int>(roundf(size.height));
}

std::shared_ptr<tgfx::Device> GPUDrawable::getDevice() {
  if (_width <= 0 || _height <= 0) {
    return nullptr;
  }
  if (window == nullptr) {
    window = tgfx::CGLWindow::MakeFrom(view);
  }
  return window ? window->getDevice() : nullptr;
}

std::shared_ptr<tgfx::Surface> GPUDrawable::onCreateSurface(tgfx::Context* context) {
  if (window == nullptr || bufferPreparing) {
    return nullptr;
  }
  // https://github.com/Tencent/libpag/issues/1870
  // Creating a surface in a non-main thread may lead to a crash because CGLWindow needs to access
  // the NSView geometry on the main thread.
  if (NSThread.isMainThread) {
    return tgfx::Surface::MakeFrom(context, window);
  }
  bufferPreparing = true;
  auto strongThis = weakThis.lock();
  [view retain];
  dispatch_async(dispatch_get_main_queue(), ^{
    auto context = strongThis->window->getDevice()->lockContext();
    if (context == nullptr) {
      strongThis->bufferPreparing = false;
      [strongThis->view release];
      return;
    }
    strongThis->surface = tgfx::Surface::MakeFrom(context, strongThis->window);
    strongThis->bufferPreparing = false;
    strongThis->window->getDevice()->unlock();
    if (strongThis->surface) {
      [[NSNotificationCenter defaultCenter] postNotificationName:kAsyncSurfacePreparedNotification
                                                          object:strongThis->view
                                                        userInfo:nil];
    }
    [strongThis->view release];
  });
  return nullptr;
}

void GPUDrawable::onFreeSurface() {
}

void GPUDrawable::present(tgfx::Context*) {
  // In the new tgfx architecture, Window::onPresent() is called automatically by
  // DrawingBuffer::presentWindows() after command submission. No explicit present() needed.
}
}  // namespace pag
