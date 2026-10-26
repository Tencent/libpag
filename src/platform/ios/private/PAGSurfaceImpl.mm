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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#import "PAGSurfaceImpl.h"
#import "PAGLayer+Internal.h"
#import "PAGLayerImpl+Internal.h"
#import "PAGSurface+Internal.h"
#import "PAGSurfaceImpl+Internal.h"
#include "base/utils/Log.h"
#include "pag/types.h"
#include "platform/cocoa/private/PAGSurfaceFactory.h"
#include "platform/cocoa/private/PixelBufferUtil.h"

@interface PAGSurfaceImpl ()

@property(nonatomic) std::shared_ptr<pag::PAGSurface> pagSurface;

@end

@implementation PAGSurfaceImpl {
  CVPixelBufferRef pixelBuffer;
}

- (std::shared_ptr<pag::PAGSurface>)pagSurface {
  return _pagSurface;
}

+ (PAGSurfaceImpl*)FromHost:(id)host {
  auto surface = pag::cocoa::MakeSurfaceFromHost(host);
  if (surface == nullptr) {
    return nil;
  }
  return [[[PAGSurfaceImpl alloc] initWithSurface:surface] autorelease];
}

+ (PAGSurfaceImpl*)FromCVPixelBuffer:(CVPixelBufferRef)pixelBuffer {
  return [PAGSurfaceImpl FromCVPixelBuffer:pixelBuffer device:nil];
}

+ (PAGSurfaceImpl*)FromCVPixelBuffer:(CVPixelBufferRef)pixelBuffer device:(id)device {
  auto surface = pag::cocoa::MakeSurfaceFromPixelBuffer(pixelBuffer, device);
  if (surface == nullptr) {
    return nil;
  }
  return [[[PAGSurfaceImpl alloc] initWithSurface:surface pixelBuffer:pixelBuffer] autorelease];
}

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
