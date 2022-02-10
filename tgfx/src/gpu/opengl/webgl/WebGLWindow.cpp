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

#include "gpu/opengl/webgl/WebGLWindow.h"
#include "gpu/opengl/GLDefines.h"

namespace pag {
std::shared_ptr<WebGLWindow> WebGLWindow::MakeFrom(const std::string& canvasID) {
  if (canvasID.empty()) {
    return nullptr;
  }
  auto device = WebGLDevice::MakeFrom(canvasID);
  if (device == nullptr) {
    return nullptr;
  }
  auto window = std::shared_ptr<WebGLWindow>(new WebGLWindow(device));
  window->canvasID = canvasID;
  return window;
}

WebGLWindow::WebGLWindow(std::shared_ptr<Device> device) : Window(std::move(device)) {
}

std::shared_ptr<Surface> WebGLWindow::onCreateSurface(Context* context) {
  int width = 0;
  int height = 0;
  emscripten_get_canvas_element_size(canvasID.c_str(), &width, &height);
  if (width <= 0 || height <= 0) {
    LOGE("WebGLWindow::onCreateSurface() Can not create a Surface with zero size.");
    return nullptr;
  }
  GLFrameBufferInfo glInfo = {};
  glInfo.id = 0;
  glInfo.format = GL::RGBA8;
  BackendRenderTarget renderTarget(glInfo, width, height);
  return Surface::MakeFrom(context, renderTarget, ImageOrigin::BottomLeft);
}
}  // namespace pag
