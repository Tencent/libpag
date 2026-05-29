/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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
#include <emscripten/html5.h>

namespace pagx {

std::shared_ptr<GPUDrawable> GPUDrawable::FromCanvasID(const std::string& canvasID) {
  if (canvasID.empty()) {
    return nullptr;
  }
  return std::shared_ptr<GPUDrawable>(new GPUDrawable(canvasID));
}

GPUDrawable::GPUDrawable(std::string canvasID) : canvasID(std::move(canvasID)) {
  GPUDrawable::updateSize();
}

void GPUDrawable::updateSize() {
  if (canvasID.empty()) {
    return;
  }
  emscripten_get_canvas_element_size(canvasID.c_str(), &_width, &_height);
  // The cached surface is sized for the previous width/height; drop it so the next getSurface()
  // call reallocates against the new canvas size.
  freeSurface();
}

std::shared_ptr<tgfx::Device> GPUDrawable::getDevice() {
  if (_width <= 0 || _height <= 0) {
    return nullptr;
  }
  if (window == nullptr) {
    window = tgfx::WebGLWindow::MakeFrom(canvasID);
  }
  return window != nullptr ? window->getDevice() : nullptr;
}

std::shared_ptr<tgfx::Surface> GPUDrawable::getSurface(tgfx::Context* context) {
  if (surface != nullptr) {
    return surface;
  }
  if (window == nullptr || context == nullptr) {
    return nullptr;
  }
  surface = tgfx::Surface::MakeFrom(context, window);
  return surface;
}

void GPUDrawable::present(tgfx::Context* context) {
  if (context != nullptr) {
    context->flushAndSubmit();
  }
}

}  // namespace pagx
