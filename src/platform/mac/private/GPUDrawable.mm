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
std::shared_ptr<GPUDrawable> GPUDrawable::FromView(NSView* view) {
  if (view == nil) {
    return nullptr;
  }
  return std::shared_ptr<GPUDrawable>(new GPUDrawable(view));
}

GPUDrawable::GPUDrawable(NSView* view) : view(view) {
  // do not retain view here, otherwise it can cause circular reference.
  updateSize();
}

void GPUDrawable::updateSize() {
  CGSize size = [view convertSizeToBacking:view.bounds.size];
  _width = static_cast<int>(roundf(size.width));
  _height = static_cast<int>(roundf(size.height));
  if (window) {
    window->invalidSize();
  }
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
  if (window == nullptr) {
    return nullptr;
  }
  return window->getSurface(context);
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
  return window->present(context);
}
}  // namespace pag
