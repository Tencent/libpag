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
#include "PixelBufferUtils.h"
#include "base/utils/Log.h"

@interface PAGSurfaceImpl ()

@property(nonatomic) std::shared_ptr<pag::PAGSurface> pagSurface;
@property(nonatomic) CVPixelBufferRef cvPixelBuffer;

@end

@implementation PAGSurfaceImpl

+ (PAGSurfaceImpl*)FromView:(NSView*)view {
  auto drawable = pag::GPUDrawable::FromView(view);
  if (drawable == nullptr) {
    return nil;
  }
  auto surface = pag::PAGSurface::MakeFrom(drawable);
  PAGSurfaceImpl* pagSurface = [[[PAGSurfaceImpl alloc] initWithSurface:surface] autorelease];
  return pagSurface;
}

+ (PAGSurfaceImpl*)MakeOffscreen:(CGSize)size {
  auto pixelBuffer = pag::PixelBufferUtils::Make(static_cast<int>(roundf(size.width)),
                                                 static_cast<int>(roundf(size.height)));
  auto drawable = pag::GPUDrawable::FromCVPixelBuffer(pixelBuffer);
  if (drawable == nullptr) {
    return nil;
  }
  auto surface = pag::PAGSurface::MakeFrom(drawable);
  PAGSurfaceImpl* pagSurface = [[[PAGSurfaceImpl alloc] initWithSurface:surface] autorelease];
  pagSurface.cvPixelBuffer = pixelBuffer;
  return pagSurface;
}

- (instancetype)initWithSurface:(std::shared_ptr<pag::PAGSurface>)value {
  if (self = [super init]) {
    _pagSurface = value;
  }
  return self;
}

- (void)dealloc {
  if (_cvPixelBuffer != nil) {
    CFRelease(_cvPixelBuffer);
    _cvPixelBuffer = nil;
  }
  [super dealloc];
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
  return _cvPixelBuffer;
}

- (CVPixelBufferRef)cvPixelBufferCopyWithPixelBuffer:(CVPixelBufferRef)pixelBuffer {
  CVPixelBufferRef pixelBufferCopy =
      pag::PixelBufferUtils::Make(_pagSurface->width(), _pagSurface->height());
  if (pixelBufferCopy == nil) {
    LOGE("CVPixelBufferRef copy create failed!");
    return nil;
  }
  CVPixelBufferLockBaseAddress(pixelBuffer, 0);
  size_t bytesPerRow = CVPixelBufferGetBytesPerRow(pixelBuffer);
  void* baseAddress = CVPixelBufferGetBaseAddress(pixelBuffer);
  CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
  CVPixelBufferLockBaseAddress(pixelBufferCopy, 0);
  void* copyBaseAddress = CVPixelBufferGetBaseAddress(pixelBufferCopy);
  memcpy(copyBaseAddress, baseAddress, _pagSurface->height() * bytesPerRow);
  CVPixelBufferUnlockBaseAddress(pixelBufferCopy, 0);
  return pixelBufferCopy;
}

- (CVPixelBufferRef)makeSnapshot {
  if ([self getCVPixelBuffer] != nil) {
    return [self cvPixelBufferCopyWithPixelBuffer:[self getCVPixelBuffer]];
  }
  CVPixelBufferRef pixelBuffer =
      pag::PixelBufferUtils::Make(_pagSurface->width(), _pagSurface->height());
  if (pixelBuffer == nil) {
    LOGE("CVPixelBufferRef copy create failed!");
    return nil;
  }
  CVPixelBufferLockBaseAddress(pixelBuffer, 0);
  size_t bytesPerRow = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0);
  uint8_t* pixelBufferData = (uint8_t*)CVPixelBufferGetBaseAddress(pixelBuffer);
  BOOL status = _pagSurface->readPixels(pag::ColorType::BGRA_8888, pag::AlphaType::Premultiplied,
                                        pixelBufferData, bytesPerRow);
  if (!status) {
    LOGE("ReadPixels failed!");
  }
  CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
  return pixelBuffer;
}

@end
