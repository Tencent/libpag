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

#include "tgfx/opengl/eagl/EAGLWindow.h"
#include "tgfx/opengl/GLFunctions.h"
#include "utils/Log.h"

namespace tgfx {
std::shared_ptr<EAGLWindow> EAGLWindow::MakeFrom(CAEAGLLayer* layer,
                                                 std::shared_ptr<GLDevice> device) {
  if (layer == nil) {
    return nullptr;
  }
  if (device == nullptr) {
    device = tgfx::GLDevice::MakeFromThreadPool();
  }
  if (device == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<EAGLWindow>(new EAGLWindow(device, layer));
}

EAGLWindow::EAGLWindow(std::shared_ptr<Device> device, CAEAGLLayer* layer)
    : Window(std::move(device)), layer(layer) {
  // do not retain layer here, otherwise it can cause circular reference.
}

EAGLWindow::~EAGLWindow() {
  auto context = device->lockContext();
  if (context) {
    auto gl = GLFunctions::Get(context);
    if (frameBufferID > 0) {
      gl->deleteFramebuffers(1, &frameBufferID);
      frameBufferID = 0;
    }
    if (colorBuffer) {
      gl->deleteRenderbuffers(1, &colorBuffer);
      colorBuffer = 0;
    }
    device->unlock();
  }
}

std::shared_ptr<Surface> EAGLWindow::onCreateSurface(Context* context) {
  auto gl = GLFunctions::Get(context);
  if (frameBufferID > 0) {
    gl->deleteFramebuffers(1, &frameBufferID);
    frameBufferID = 0;
  }
  if (colorBuffer) {
    gl->deleteRenderbuffers(1, &colorBuffer);
    colorBuffer = 0;
  }
  auto width = layer.bounds.size.width * layer.contentsScale;
  auto height = layer.bounds.size.height * layer.contentsScale;
  if (width <= 0 || height <= 0) {
    return nullptr;
  }
  gl->genFramebuffers(1, &frameBufferID);
  gl->bindFramebuffer(GL_FRAMEBUFFER, frameBufferID);
  gl->genRenderbuffers(1, &colorBuffer);
  gl->bindRenderbuffer(GL_RENDERBUFFER, colorBuffer);
  gl->framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorBuffer);
  auto eaglContext = static_cast<EAGLDevice*>(context->device())->eaglContext();
  [eaglContext renderbufferStorage:GL_RENDERBUFFER fromDrawable:layer];
  auto frameBufferStatus = gl->checkFramebufferStatus(GL_FRAMEBUFFER);
  gl->bindFramebuffer(GL_FRAMEBUFFER, 0);
  gl->bindRenderbuffer(GL_RENDERBUFFER, 0);
  if (frameBufferStatus != GL_FRAMEBUFFER_COMPLETE) {
    LOGE("EAGLWindow::onCreateSurface() Framebuffer is not complete!");
    return nullptr;
  }
  GLFrameBufferInfo glInfo = {};
  glInfo.id = frameBufferID;
  glInfo.format = GL_RGBA8;
  BackendRenderTarget renderTarget = {glInfo, static_cast<int>(width), static_cast<int>(height)};
  return Surface::MakeFrom(context, renderTarget, ImageOrigin::BottomLeft);
}

void EAGLWindow::onPresent(Context* context, int64_t) {
  auto gl = GLFunctions::Get(context);
  gl->bindRenderbuffer(GL_RENDERBUFFER, colorBuffer);
  auto eaglContext = static_cast<EAGLDevice*>(context->device())->eaglContext();
  [eaglContext presentRenderbuffer:GL_RENDERBUFFER];
  gl->bindRenderbuffer(GL_RENDERBUFFER, 0);
}
}  // namespace tgfx
