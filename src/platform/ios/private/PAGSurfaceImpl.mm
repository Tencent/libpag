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

#import "PAGSurfaceImpl.h"
#include "GPUDrawable.h"
#import "PAGLayer+Internal.h"
#import "PAGLayerImpl+Internal.h"
#import "PAGSurface+Internal.h"
#include "PixelBufferUtils.h"
#include "base/utils/Log.h"
#include "types.h"

@interface PAGSurfaceImpl ()

@property(nonatomic) std::shared_ptr<pag::PAGSurface> pagSurface;
@property(nonatomic) pag::GPUDrawable* gpuDrawable;

@end

@implementation PAGSurfaceImpl

- (std::shared_ptr<pag::PAGSurface>)pagSurface {
  return _pagSurface;
}

+ (PAGSurfaceImpl*)FromLayer:(CAEAGLLayer*)layer {
  auto drawable = pag::GPUDrawable::FromLayer(layer);
  if (drawable == nullptr) {
    return nil;
  }
  auto surface = pag::PAGSurface::MakeFrom(drawable);
  PAGSurfaceImpl* pagSurface = [[PAGSurfaceImpl alloc] initWithSurface:surface
                                                              drawable:drawable.get()];
  return [pagSurface autorelease];
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

+ (PAGSurfaceImpl*)MakeOffscreen:(CGSize)size {
  LOGE("The simulator does not support [PAGSurface MakeOffscreen:].");
  return nil;
}

#else

+ (PAGSurfaceImpl*)FromCVPixelBuffer:(CVPixelBufferRef)pixelBuffer {
  return [PAGSurfaceImpl FromCVPixelBuffer:pixelBuffer context:nil];
}

+ (PAGSurfaceImpl*)FromCVPixelBuffer:(CVPixelBufferRef)pixelBuffer
                             context:(EAGLContext*)eaglContext {
  auto drawable = pag::GPUDrawable::FromCVPixelBuffer(pixelBuffer, eaglContext);
  if (drawable == nullptr) {
    return nil;
  }
  auto surface = pag::PAGSurface::MakeFrom(drawable);
  PAGSurfaceImpl* pagSurface = [[PAGSurfaceImpl alloc] initWithSurface:surface
                                                              drawable:drawable.get()];
  return [pagSurface autorelease];
}

+ (PAGSurfaceImpl*)MakeOffscreen:(CGSize)size {
  // 这里如果添加autoreleasePool会导致PAGSurfaceImpl也被释放，因此不加。
  // 使用时当PAGSurfaceImpl autorelease时，pixelBuffer也会析构
  auto pixelBuffer = pag::PixelBufferUtils::Make(static_cast<int>(roundf(size.width)),
                                                 static_cast<int>(roundf(size.height)));
  return [PAGSurfaceImpl FromCVPixelBuffer:pixelBuffer];
}

#endif

- (instancetype)initWithSurface:(std::shared_ptr<pag::PAGSurface>)value
                       drawable:(pag::GPUDrawable*)target {
  if (self = [super init]) {
    _pagSurface = value;
    _gpuDrawable = target;
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
  _pagSurface->freeCache();
}

- (CVPixelBufferRef)getCVPixelBuffer {
  return _gpuDrawable->getCVPixelBuffer();
}

- (CVPixelBufferRef)makeSnapshot {
  CVPixelBufferRef pixelBuffer =
      pag::PixelBufferUtils::Make(_pagSurface->width(), _pagSurface->height());
  if (pixelBuffer == nil) {
    LOGE("CVPixelBufferRef create failed!");
    return nil;
  }
  CVPixelBufferLockBaseAddress(pixelBuffer, 0);
  size_t bytesPerRow = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0);
  uint8_t* pixelBufferData = (uint8_t*)CVPixelBufferGetBaseAddress(pixelBuffer);
  BOOL status = _pagSurface->readPixels(pag::ColorType::BGRA_8888, pag::AlphaType::Premultiplied,
                                        pixelBufferData, bytesPerRow);
  CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
  if (!status) {
    LOGE("ReadPixels failed!");
    return nil;
  }
  return pixelBuffer;
}

- (BOOL)copyPixelsTo:(CVPixelBufferRef)pixelBuffer {
  if (!pixelBuffer) {
    return NO;
  }

  CVPixelBufferLockBaseAddress(pixelBuffer, 0);
  int width = (int)CVPixelBufferGetWidth(pixelBuffer);
  int height = (int)CVPixelBufferGetHeight(pixelBuffer);
  if (width != [self width] || height != [self height]) {
    CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
    return NO;
  }
  size_t bytesPerRow = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0);
  uint8_t* pixelBufferData = (uint8_t*)CVPixelBufferGetBaseAddress(pixelBuffer);
  BOOL status = _pagSurface->readPixels(pag::ColorType::BGRA_8888, pag::AlphaType::Premultiplied,
                                        pixelBufferData, bytesPerRow);
  CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
  if (!status) {
    LOGE("ReadPixels failed!");
    return NO;
  }
  return YES;
}

@end
