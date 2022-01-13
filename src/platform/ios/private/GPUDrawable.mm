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

namespace pag {

NSString* const kGPURenderTargetBufferPreparedNotification =
    @"PAG.io.GPURenderTargetBufferPrepared";
NSString* const kPreparedAsync = @"async";

std::shared_ptr<GPUDrawable> GPUDrawable::FromLayer(CAEAGLLayer* layer) {
  if (layer == nil) {
    return nullptr;
  }
  auto drawable = std::shared_ptr<GPUDrawable>(new GPUDrawable(layer));
  drawable->weakThis = drawable;
  return drawable;
}

std::shared_ptr<GPUDrawable> GPUDrawable::FromCVPixelBuffer(CVPixelBufferRef pixelBuffer,
                                                            EAGLContext* eaglContext) {
  if (pixelBuffer == nil ||
      CVPixelBufferGetPixelFormatType(pixelBuffer) != kCVPixelFormatType_32BGRA) {
    return nullptr;
  }
  auto drawable = std::shared_ptr<GPUDrawable>(new GPUDrawable(pixelBuffer));
  [eaglContext retain];
  drawable->eaglContext = eaglContext;
  drawable->weakThis = drawable;
  return drawable;
}

GPUDrawable::GPUDrawable(CAEAGLLayer* layer) : layer(layer), bufferPreparing(false) {
  // do not retain layer here, otherwise it can cause circular reference.
  updateSize();
}

GPUDrawable::GPUDrawable(CVPixelBufferRef pixelBuffer)
    : pixelBuffer(pixelBuffer), bufferPreparing(false) {
  CFRetain(pixelBuffer);
  updateSize();
}

GPUDrawable::~GPUDrawable() {
  if (pixelBuffer) {
    CFRelease(pixelBuffer);
    pixelBuffer = nil;
  }
  [eaglContext release];
}

int GPUDrawable::width() const {
  return _width;
}

int GPUDrawable::height() const {
  return _height;
}

void GPUDrawable::updateSize() {
  CGFloat width;
  CGFloat height;
  if (pixelBuffer != nil) {
    width = CVPixelBufferGetWidth(pixelBuffer);
    height = CVPixelBufferGetHeight(pixelBuffer);
  } else {
    width = layer.bounds.size.width * layer.contentsScale;
    height = layer.bounds.size.height * layer.contentsScale;
  }
  _width = static_cast<int>(floor(width));
  _height = static_cast<int>(floor(height));
  surface = nullptr;
}

std::shared_ptr<Device> GPUDrawable::getDevice() {
  if (_width <= 0 || _height <= 0) {
    return nullptr;
  }
  if (window == nullptr) {
    auto device = EAGLDevice::MakeAdopted(eaglContext);
    if (pixelBuffer) {
      window = EAGLWindow::MakeFrom(pixelBuffer, device);
    } else {
      window = EAGLWindow::MakeFrom(layer, device);
    }
  }
  return window ? window->getDevice() : nullptr;
}

std::shared_ptr<Surface> GPUDrawable::createSurface(Context* context) {
  if (window == nullptr || bufferPreparing) {
    return nullptr;
  }
  if (surface) {
    return surface;
  }
  if (layer == nil || IsInMainThread()) {
    surface = window->createSurface(context);
    if (layer != nil && surface != nullptr) {
      [[NSNotificationCenter defaultCenter]
          postNotificationName:kGPURenderTargetBufferPreparedNotification
                        object:layer
                      userInfo:@{
                        kPreparedAsync : @(false)
                      }];
    }
    return surface;
  } else {
    bufferPreparing = true;
    auto strongThis = weakThis.lock();
    [layer retain];
    dispatch_async(dispatch_get_main_queue(), ^{
      auto glContext = strongThis->window->getDevice()->lockContext();
      if (glContext == nullptr) {
        strongThis->bufferPreparing = false;
        [strongThis->layer release];
        return;
      }
      if (strongThis->bufferPreparing) {
        strongThis->surface = strongThis->window->createSurface(glContext);
        if (strongThis->surface) {
          [[NSNotificationCenter defaultCenter]
              postNotificationName:kGPURenderTargetBufferPreparedNotification
                            object:strongThis->layer
                          userInfo:@{
                            kPreparedAsync : @(true)
                          }];
        }
      }
      strongThis->bufferPreparing = false;
      strongThis->window->getDevice()->unlock();
      [strongThis->layer release];
    });
    return nullptr;
  }
}

void GPUDrawable::present(Context* context) {
  if (window == nullptr) {
    return;
  }

  if (IsInMainThread()) {
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
    return;
  }
}

bool GPUDrawable::IsInMainThread() {
  /// 由于current queue有可能是来自main queue sync，这种情况下也没有异步的必要，应该直接上屏
  /// 因此这里使用NSThread来判断是否主线程。
  return NSThread.isMainThread;
}

CVPixelBufferRef GPUDrawable::getCVPixelBuffer() {
  return pixelBuffer;
}

}
