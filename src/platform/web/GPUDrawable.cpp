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
#include <emscripten/html5.h>
#include "base/utils/Log.h"
#include "tgfx/core/Surface.h"

#if defined(TGFX_USE_WEBGPU)
#include "tgfx/gpu/webgpu/WebGPUWindow.h"
#else
#include "tgfx/gpu/opengl/webgl/WebGLWindow.h"
#endif

namespace pag {
std::shared_ptr<GPUDrawable> GPUDrawable::FromCanvasID(const std::string& canvasID) {
  if (canvasID.empty()) {
    LOGE("GPUDrawable.FromCanvasID() The canvasID is invalid.");
    return nullptr;
  }
  return std::shared_ptr<GPUDrawable>(new GPUDrawable(canvasID));
}

GPUDrawable::GPUDrawable(std::string canvasID) : canvasID(std::move(canvasID)) {
  GPUDrawable::updateSize();
}

#if defined(TGFX_USE_WEBGPU)
std::shared_ptr<GPUDrawable> GPUDrawable::FromCanvas(emscripten::val canvas) {
  if (canvas.isUndefined() || canvas.isNull()) {
    LOGE("GPUDrawable.FromCanvas() The canvas is invalid.");
    return nullptr;
  }
  return std::shared_ptr<GPUDrawable>(new GPUDrawable(std::move(canvas)));
}

GPUDrawable::GPUDrawable(emscripten::val canvas) : canvas(std::move(canvas)) {
  GPUDrawable::updateSize();
}
#endif

void GPUDrawable::updateSize() {
#if defined(TGFX_USE_WEBGPU)
  if (!canvas.isUndefined() && !canvas.isNull()) {
    _width = canvas["width"].as<int>();
    _height = canvas["height"].as<int>();
    return;
  }
#endif
  if (!canvasID.empty()) {
    emscripten_get_canvas_element_size(canvasID.c_str(), &_width, &_height);
  }
}

std::shared_ptr<tgfx::Device> GPUDrawable::getDevice() {
  if (_width <= 0 || _height <= 0) {
    return nullptr;
  }
  if (window == nullptr) {
#if defined(TGFX_USE_WEBGPU)
    if (!canvas.isUndefined() && !canvas.isNull()) {
      window = tgfx::WebGPUWindow::MakeFrom(canvas);
    } else {
      window = tgfx::WebGPUWindow::MakeFrom(canvasID);
    }
#else
    window = tgfx::WebGLWindow::MakeFrom(canvasID);
#endif
  }
  if (window == nullptr) {
    return nullptr;
  }
  return window->getDevice();
}

std::shared_ptr<tgfx::Surface> GPUDrawable::onCreateSurface(tgfx::Context* context) {
  if (window == nullptr) {
    return nullptr;
  }
  return tgfx::Surface::MakeFrom(context, window);
}

void GPUDrawable::onFreeSurface() {
}
}  // namespace pag
