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

#include "CGLWindow.h"
#include <thread>
#include "CGLHardwareTexture.h"
#include "gpu/opengl/GLSurface.h"

namespace pag {
static std::mutex threadCacheLocker = {};
static std::unordered_map<std::thread::id, std::weak_ptr<CGLDevice>> threadCacheMap = {};

static std::shared_ptr<CGLDevice> MakeDeviceFromThreadPool() {
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
  auto device = CGLDevice::Make();
  if (device == nullptr) {
    return nullptr;
  }
  threadCacheMap[threadID] = device;
  return device;
}

std::shared_ptr<CGLWindow> CGLWindow::MakeFrom(NSView* view, CGLContextObj sharedContext) {
  if (view == nil) {
    return nullptr;
  }
  auto device = CGLDevice::Make(sharedContext);
  if (device == nullptr) {
    return nullptr;
  }
  auto window = std::shared_ptr<CGLWindow>(new CGLWindow(device));
  // do not retain view here, otherwise it can cause circular reference.
  window->view = view;
  return window;
}

std::shared_ptr<CGLWindow> CGLWindow::MakeFrom(CVPixelBufferRef pixelBuffer,
                                               std::shared_ptr<CGLDevice> device) {
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
  auto window = std::shared_ptr<CGLWindow>(new CGLWindow(device));
  window->pixelBuffer = pixelBuffer;
  return window;
}

CGLWindow::CGLWindow(std::shared_ptr<Device> device) : Window(std::move(device)) {
}

CGLWindow::~CGLWindow() {
  if (pixelBuffer) {
    CFRelease(pixelBuffer);
    pixelBuffer = nil;
  }
  if (view) {
    auto glContext = static_cast<CGLDevice*>(device.get())->glContext;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    [glContext setView:nil];
#pragma clang diagnostic pop
    view = nil;
  }
}

std::shared_ptr<Surface> CGLWindow::onCreateSurface(Context* context) {
  auto glContext = static_cast<CGLDevice*>(device.get())->glContext;
  [glContext update];
  if (view != nil) {
    CGSize size = [view convertSizeToBacking:view.bounds.size];
    if (size.width <= 0 || size.height <= 0) {
      return nullptr;
    }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    [glContext setView:view];
#pragma clang diagnostic pop
    GLFrameBufferInfo glInfo = {};
    glInfo.id = 0;
    glInfo.format = GL::RGBA8;
    BackendRenderTarget renderTarget(glInfo, size.width, size.height);
    return Surface::MakeFrom(context, renderTarget, ImageOrigin::BottomLeft);
  }
  auto texture = CGLHardwareTexture::MakeFrom(context, pixelBuffer, true);
  return GLSurface::MakeFrom(context, texture);
}

void CGLWindow::onPresent(Context* context, int64_t) {
  auto glContext = static_cast<CGLDevice*>(device.get())->glContext;
  if (view) {
    [glContext flushBuffer];
  } else {
    auto gl = GLContext::Unwrap(context);
    gl->flush();
  }
}
}  // namespace pag