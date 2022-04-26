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

#include "GPUDrawable.h"
#include "tgfx/gpu/Surface.h"
#include "tgfx/gpu/opengl/cgl/CGLWindow.h"

namespace pag {
std::shared_ptr<GPUDrawable> GPUDrawable::FromView(NSView* view) {
  if (view == nil) {
    return nullptr;
  }
  return std::shared_ptr<GPUDrawable>(new GPUDrawable(view));
}

std::shared_ptr<GPUDrawable> GPUDrawable::FromCVPixelBuffer(CVPixelBufferRef pixelBuffer) {
  if (pixelBuffer == nil ||
      CVPixelBufferGetPixelFormatType(pixelBuffer) != kCVPixelFormatType_32BGRA) {
    return nullptr;
  }
  return std::shared_ptr<GPUDrawable>(new GPUDrawable(pixelBuffer));
}

GPUDrawable::GPUDrawable(NSView* view) : view(view) {
  // do not retain view here, otherwise it can cause circular reference.
  updateSize();
}

GPUDrawable::GPUDrawable(CVPixelBufferRef pixelBuffer) : pixelBuffer(pixelBuffer) {
  CFRetain(pixelBuffer);
  updateSize();
}

GPUDrawable::~GPUDrawable() {
  if (pixelBuffer) {
    CFRelease(pixelBuffer);
    pixelBuffer = nil;
  }
}

void GPUDrawable::updateSize() {
  CGFloat width;
  CGFloat height;
  if (pixelBuffer != nil) {
    width = CVPixelBufferGetWidth(pixelBuffer);
    height = CVPixelBufferGetHeight(pixelBuffer);
  } else {
    CGSize size = [view convertSizeToBacking:view.bounds.size];
    width = size.width;
    height = size.height;
  }
  _width = static_cast<int>(floor(width));
  _height = static_cast<int>(floor(height));
}

std::shared_ptr<tgfx::Device> GPUDrawable::getDevice() {
  if (_width <= 0 || _height <= 0) {
    return nullptr;
  }
  if (window == nullptr) {
    if (pixelBuffer) {
      auto device = std::static_pointer_cast<tgfx::CGLDevice>(tgfx::GLDevice::Make());
      window = tgfx::CGLWindow::MakeFrom(pixelBuffer, device);
    } else {
      window = tgfx::CGLWindow::MakeFrom(view);
    }
  }
  return window ? window->getDevice() : nullptr;
}

std::shared_ptr<tgfx::Surface> GPUDrawable::createSurface(tgfx::Context* context) {
  if (window == nullptr) {
    return nullptr;
  }
  return window->createSurface(context);
}

void GPUDrawable::present(tgfx::Context* context) {
  if (window == nullptr) {
    return;
  }
  return window->present(context);
}
}
