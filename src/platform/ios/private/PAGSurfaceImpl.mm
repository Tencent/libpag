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

#import "PAGSurfaceImpl.h"
#include "GPUDrawable.h"
#import "PAGLayer+Internal.h"
#import "PAGLayerImpl+Internal.h"
#import "PAGSurface+Internal.h"
#include "base/utils/Log.h"
#include "pag/types.h"
#include "platform/cocoa/private/PixelBufferUtil.h"
#include "rendering/drawables/HardwareBufferDrawable.h"
#include "tgfx/gpu/opengl/eagl/EAGLDevice.h"

@interface PAGSurfaceImpl ()

@property(nonatomic) std::shared_ptr<pag::PAGSurface> pagSurface;

@end

@implementation PAGSurfaceImpl {
  CVPixelBufferRef pixelBuffer;
}

- (std::shared_ptr<pag::PAGSurface>)pagSurface {
  return _pagSurface;
}

+ (PAGSurfaceImpl*)FromLayer:(CAEAGLLayer*)layer {
  auto drawable = pag::GPUDrawable::FromLayer(layer);
  auto surface = pag::PAGSurface::MakeFrom(drawable);
  if (surface == nullptr) {
    return nil;
  }
  return [[[PAGSurfaceImpl alloc] initWithSurface:surface] autorelease];
}

#if TARGET_IPHONE_SIMULATOR

+ (PAGSurfaceImpl*)FromCVPixelBuffer:(CVPixelBufferRef)pixelBuffer {
  LOGE("The simulator does not support [PAGSurface FromCVPixelBuffer:].");
  return nil;
}

+ (PAGSurfaceImpl*)FromCVPixelBuffer:(CVPixelBufferRef)pixelBuffer
                             context:(EAGLContext*)eaglContext {
  LOGE("The simulator does not support [PAGSurface FromCVPixelBuffer:context:].");
  return nil;
}

#else

+ (PAGSurfaceImpl*)FromCVPixelBuffer:(CVPixelBufferRef)pixelBuffer {
  return [PAGSurfaceImpl FromCVPixelBuffer:pixelBuffer context:nil];
}

+ (PAGSurfaceImpl*)FromCVPixelBuffer:(CVPixelBufferRef)pixelBuffer
                             context:(EAGLContext*)eaglContext {
  auto device = tgfx::EAGLDevice::MakeFrom(eaglContext);
  auto drawable = pag::HardwareBufferDrawable::MakeFrom(pixelBuffer, device);
  auto surface = pag::PAGSurface::MakeFrom(drawable);
  if (surface == nullptr) {
    return nil;
  }
  return [[[PAGSurfaceImpl alloc] initWithSurface:surface pixelBuffer:pixelBuffer] autorelease];
}

#endif

+ (PAGSurfaceImpl*)MakeOffscreen:(CGSize)size {
  auto surface = pag::PAGSurface::MakeOffscreen(static_cast<int>(roundf(size.width)),
                                                static_cast<int>(roundf(size.height)));
  if (surface == nullptr) {
    return nil;
  }
  return [[[PAGSurfaceImpl alloc] initWithSurface:surface] autorelease];
}

- (instancetype)initWithSurface:(std::shared_ptr<pag::PAGSurface>)value {
  if (self = [super init]) {
    _pagSurface = value;
    pixelBuffer = nil;
  }
  return self;
}

- (instancetype)initWithSurface:(std::shared_ptr<pag::PAGSurface>)value
                    pixelBuffer:(CVPixelBufferRef)buffer {
  if (self = [super init]) {
    _pagSurface = value;
    pixelBuffer = buffer;
  }
  return self;
}

- (void)updateSize {
  _pagSurface->updateSize();
}

- (int)width {
  return _pagSurface->width();
}

- (int)height {
  return _pagSurface->height();
}

- (BOOL)clearAll {
  return _pagSurface->clearAll();
}

- (void)freeCache {
  pixelBuffer = nil;
  _pagSurface->freeCache();
}

- (CVPixelBufferRef)getCVPixelBuffer {
  if (pixelBuffer == nil) {
    pixelBuffer = _pagSurface->getHardwareBuffer();
  }
  return pixelBuffer;
}

- (CVPixelBufferRef)makeSnapshot {
  auto hardwareBuffer = pag::PixelBufferUtil::Make(_pagSurface->width(), _pagSurface->height());
  if (hardwareBuffer == nil) {
    LOGE("CVPixelBufferRef create failed!");
    return nil;
  }
  CVPixelBufferLockBaseAddress(hardwareBuffer, 0);
  size_t bytesPerRow = CVPixelBufferGetBytesPerRowOfPlane(hardwareBuffer, 0);
  uint8_t* pixelBufferData = (uint8_t*)CVPixelBufferGetBaseAddress(hardwareBuffer);
  BOOL status = _pagSurface->readPixels(pag::ColorType::BGRA_8888, pag::AlphaType::Premultiplied,
                                        pixelBufferData, bytesPerRow);
  CVPixelBufferUnlockBaseAddress(hardwareBuffer, 0);
  if (!status) {
    LOGE("ReadPixels failed!");
    return nil;
  }
  return hardwareBuffer;
}

- (BOOL)copyPixelsTo:(void*)pixels rowBytes:(size_t)rowBytes {
  if (!pixels) {
    return NO;
  }
  return _pagSurface->readPixels(pag::ColorType::BGRA_8888, pag::AlphaType::Premultiplied, pixels,
                                 rowBytes);
}
@end
