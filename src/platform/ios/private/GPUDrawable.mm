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

namespace pag {
NSString* const kAsyncSurfacePreparedNotification = @"io.pag.AsyncSurfacePrepared";

std::shared_ptr<GPUDrawable> GPUDrawable::FromLayer(CAEAGLLayer* layer) {
  if (layer == nil) {
    return nullptr;
  }
  auto drawable = std::shared_ptr<GPUDrawable>(new GPUDrawable(layer));
  drawable->weakThis = drawable;
  return drawable;
}

GPUDrawable::GPUDrawable(CAEAGLLayer* layer) : layer(layer) {
  // do not retain layer here, otherwise it can cause circular reference.
  updateSize();
  tryCreateSurface();
}

void GPUDrawable::tryCreateSurface() {
  // try to create the surface if the layer is ready, because some devices may require the surface
  // to be created on the main thread.
  if (!NSThread.isMainThread || _width <= 0 || _height <= 0) {
    return;
  }
  window = tgfx::EAGLWindow::MakeFrom(layer);
  if (window != nullptr) {
    auto device = window->getDevice();
    auto context = device->lockContext();
    surface = window->getSurface(context);
    device->unlock();
  }
}

int GPUDrawable::width() const {
  return _width;
}

int GPUDrawable::height() const {
  return _height;
}

void GPUDrawable::updateSize() {
  auto width = layer.bounds.size.width * layer.contentsScale;
  auto height = layer.bounds.size.height * layer.contentsScale;
  _width = static_cast<int>(roundf(width));
  _height = static_cast<int>(roundf(height));
  if (window) {
    window->invalidSize();
  }
}

std::shared_ptr<tgfx::Device> GPUDrawable::getDevice() {
  if (_width <= 0 || _height <= 0) {
    return nullptr;
  }
  if (window == nullptr) {
    window = tgfx::EAGLWindow::MakeFrom(layer);
  }
  return window ? window->getDevice() : nullptr;
}

std::shared_ptr<tgfx::Surface> GPUDrawable::onCreateSurface(tgfx::Context* context) {
  if (window == nullptr || bufferPreparing) {
    return nullptr;
  }
  if (surface) {
    return surface;
  }
  // https://github.com/Tencent/libpag/issues/1870
  // Creating a surface in a non-main thread may lead to a crash.
  if (NSThread.isMainThread) {
    surface = window->getSurface(context);
    return surface;
  } else {
    bufferPreparing = true;
    auto strongThis = weakThis.lock();
    [layer retain];
    dispatch_async(dispatch_get_main_queue(), ^{
      auto context = strongThis->window->getDevice()->lockContext();
      if (context == nullptr) {
        strongThis->bufferPreparing = false;
        [strongThis->layer release];
        return;
      }
      strongThis->surface = strongThis->window->getSurface(context);
      strongThis->bufferPreparing = false;
      strongThis->window->getDevice()->unlock();
      if (strongThis->surface) {
        [[NSNotificationCenter defaultCenter] postNotificationName:kAsyncSurfacePreparedNotification
                                                            object:strongThis->layer
                                                          userInfo:nil];
      }
      [strongThis->layer release];
    });
    return nullptr;
  }
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

  if (NSThread.isMainThread) {
    window->present(context);
  } else {
    auto strongThis = weakThis.lock();
    [layer retain];
    dispatch_async(dispatch_get_main_queue(), ^{
      auto glContext = strongThis->window->getDevice()->lockContext();
      if (glContext == nullptr) {
        [strongThis->layer release];
        return;
      }
      strongThis->window->present(glContext);
      strongThis->window->getDevice()->unlock();
      [strongThis->layer release];
    });
  }
}
}  // namespace pag
