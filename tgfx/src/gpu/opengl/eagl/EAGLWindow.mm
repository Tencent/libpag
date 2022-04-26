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

#include "tgfx/gpu/opengl/eagl/EAGLWindow.h"
#include <thread>
#include "EAGLHardwareTexture.h"
#include "gpu/opengl/GLContext.h"
#include "gpu/opengl/GLSurface.h"

namespace tgfx {
static std::mutex threadCacheLocker = {};
static std::unordered_map<std::thread::id, std::weak_ptr<GLDevice>> threadCacheMap = {};

static std::shared_ptr<GLDevice> MakeDeviceFromThreadPool() {
  std::lock_guard<std::mutex> autoLock(threadCacheLocker);
  auto threadID = std::this_thread::get_id();
  auto result = threadCacheMap.find(threadID);
  if (result != threadCacheMap.end()) {
    auto& weak = result->second;
    auto context = weak.lock();
    if (context) {
      return context;
    }
    threadCacheMap.erase(result);
  }
  auto device = GLDevice::Make();
  if (device == nullptr) {
    return nullptr;
  }
  threadCacheMap[threadID] = device;
  return device;
}

std::shared_ptr<EAGLWindow> EAGLWindow::MakeFrom(CAEAGLLayer* layer,
                                                 std::shared_ptr<GLDevice> device) {
  if (layer == nil) {
    return nullptr;
  }
  if (device == nullptr) {
    device = MakeDeviceFromThreadPool();
  }
  if (device == nullptr) {
    return nullptr;
  }
  auto window = std::shared_ptr<EAGLWindow>(new EAGLWindow(device));
  // do not retain layer here, otherwise it can cause circular reference.
  window->layer = layer;
  return window;
}

std::shared_ptr<EAGLWindow> EAGLWindow::MakeFrom(CVPixelBufferRef pixelBuffer,
                                                 std::shared_ptr<GLDevice> device) {
  if (pixelBuffer == nil) {
    return nullptr;
  }
  if (device == nullptr) {
    device = MakeDeviceFromThreadPool();
  }
  if (device == nullptr) {
    return nullptr;
  }
  CFRetain(pixelBuffer);
  auto window = std::shared_ptr<EAGLWindow>(new EAGLWindow(device));
  window->pixelBuffer = pixelBuffer;
  return window;
}

EAGLWindow::EAGLWindow(std::shared_ptr<Device> device) : Window(std::move(device)) {
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
  if (pixelBuffer) {
    CFRelease(pixelBuffer);
    pixelBuffer = nil;
  }
}

std::shared_ptr<Surface> EAGLWindow::onCreateSurface(Context* context) {
  if (pixelBuffer != nil) {
    auto texture = EAGLHardwareTexture::MakeFrom(context, pixelBuffer);
    return GLSurface::MakeFrom(texture);
  }
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
  GLFrameBuffer glInfo = {};
  glInfo.id = frameBufferID;
  glInfo.format = PixelFormat::RGBA_8888;
  auto renderTarget = GLRenderTarget::MakeFrom(context, glInfo, static_cast<int>(width),
                                               static_cast<int>(height), ImageOrigin::BottomLeft);
  return Surface::MakeFrom(renderTarget);
}

void EAGLWindow::onPresent(Context* context, int64_t) {
  auto gl = GLFunctions::Get(context);
  if (layer) {
    gl->bindRenderbuffer(GL_RENDERBUFFER, colorBuffer);
    auto eaglContext = static_cast<EAGLDevice*>(context->device())->eaglContext();
    [eaglContext presentRenderbuffer:GL_RENDERBUFFER];
    gl->bindRenderbuffer(GL_RENDERBUFFER, 0);
  } else {
    gl->flush();
  }
}
}  // namespace tgfx
