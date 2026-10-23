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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

// Uses CAEAGLLayer, deprecated since iOS 12. libpag's own build defines
// GLES_SILENCE_DEPRECATION (see CMakeLists); define it locally too so IDE indexing without the
// build flags stays clean.
#ifndef GLES_SILENCE_DEPRECATION
#define GLES_SILENCE_DEPRECATION 1
#endif
#include "platform/ios/private/opengl/EAGLDrawable.h"

namespace pag {
NSString* const AsyncSurfacePreparedNotification = @"io.pag.AsyncSurfacePrepared";

std::shared_ptr<EAGLDrawable> EAGLDrawable::FromLayer(CAEAGLLayer* layer) {
  if (layer == nil) {
    return nullptr;
  }
  auto drawable = std::shared_ptr<EAGLDrawable>(new EAGLDrawable(layer));
  drawable->weakThis = drawable;
  return drawable;
}

EAGLDrawable::EAGLDrawable(CAEAGLLayer* layer) : layer(layer) {
  // do not retain layer here, otherwise it can cause circular reference.
  updateSize();
  tryCreateSurface();
}

void EAGLDrawable::tryCreateSurface() {
  // try to create the surface if the layer is ready, because some devices may require the surface
  // to be created on the main thread.
  if (!NSThread.isMainThread || _width <= 0 || _height <= 0) {
    return;
  }
  window = tgfx::EAGLWindow::MakeFrom(layer);
  if (window != nullptr) {
    auto device = window->getDevice();
    auto context = device->lockContext();
    surface = tgfx::Surface::MakeFrom(context, window);
    device->unlock();
  }
}

int EAGLDrawable::width() const {
  return _width;
}

int EAGLDrawable::height() const {
  return _height;
}

void EAGLDrawable::updateSize() {
  auto width = layer.bounds.size.width * layer.contentsScale;
  auto height = layer.bounds.size.height * layer.contentsScale;
  _width = static_cast<int>(roundf(width));
  _height = static_cast<int>(roundf(height));
}

std::shared_ptr<tgfx::Device> EAGLDrawable::getDevice() {
  if (_width <= 0 || _height <= 0) {
    return nullptr;
  }
  if (window == nullptr) {
    window = tgfx::EAGLWindow::MakeFrom(layer);
  }
  return window ? window->getDevice() : nullptr;
}

std::shared_ptr<tgfx::Surface> EAGLDrawable::onCreateSurface(tgfx::Context* context) {
  if (window == nullptr || bufferPreparing) {
    return nullptr;
  }
  if (surface) {
    return surface;
  }
  // https://github.com/Tencent/libpag/issues/1870
  // Creating a surface in a non-main thread may lead to a crash.
  if (NSThread.isMainThread) {
    surface = tgfx::Surface::MakeFrom(context, window);
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
      strongThis->surface = tgfx::Surface::MakeFrom(context, strongThis->window);
      strongThis->bufferPreparing = false;
      strongThis->window->getDevice()->unlock();
      if (strongThis->surface) {
        [[NSNotificationCenter defaultCenter] postNotificationName:AsyncSurfacePreparedNotification
                                                            object:strongThis->layer
                                                          userInfo:nil];
      }
      [strongThis->layer release];
    });
    return nullptr;
  }
}

void EAGLDrawable::onFreeSurface() {
}

void EAGLDrawable::present(tgfx::Context*) {
  if (window == nullptr) {
    return;
  }
  // In the new tgfx architecture, Window::onPresent() is called automatically by
  // DrawingBuffer::presentWindows() after command submission.
}
}  // namespace pag
