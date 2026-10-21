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
#import "PAGLayerImpl+Internal.h"
#import "PAGSurfaceImpl+Internal.h"
#include "base/utils/Log.h"
#include "platform/cocoa/private/PixelBufferUtil.h"

#if defined(TGFX_USE_OPENGL)
#import "platform/mac/private/opengl/CGLDrawable.h"
#import "platform/mac/private/opengl/PAGSurfaceImpl+OpenGL.h"
#elif defined(TGFX_USE_METAL)
#import "platform/cocoa/private/metal/MetalLayerDrawable.h"
#import "platform/mac/private/metal/PAGSurfaceImpl+Metal.h"
#endif

@interface PAGSurfaceImpl ()

@property(nonatomic) std::shared_ptr<pag::PAGSurface> pagSurface;

@end

@implementation PAGSurfaceImpl {
  CVPixelBufferRef pixelBuffer;
}

+ (PAGSurfaceImpl*)MakeOffscreen:(CGSize)size {
  auto surface = pag::PAGSurface::MakeOffscreen(static_cast<int>(roundf(size.width)),
                                                static_cast<int>(roundf(size.height)));
  if (surface == nullptr) {
    return nil;
  }
  PAGSurfaceImpl* pagSurface = [[[PAGSurfaceImpl alloc] initWithSurface:surface] autorelease];
  return pagSurface;
}

- (instancetype)initWithSurface:(std::shared_ptr<pag::PAGSurface>)value {
  if (self = [super init]) {
    _pagSurface = value;
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
    // The buffer was never written, so handing it back would look like a successful snapshot of
    // a blank frame. Return nil as the iOS implementation does; PixelBufferUtil::Make()
    // autoreleases the buffer, so dropping the reference is enough.
    LOGE("ReadPixels failed!");
    return nil;
  }
  return hardwareBuffer;
}

@end

// Keep the backend factory with its class so it is retained by ordinary static linking.
#if defined(TGFX_USE_OPENGL)
@implementation PAGSurfaceImpl (OpenGL)

+ (PAGSurfaceImpl*)FromView:(NSView*)view {
  auto drawable = pag::CGLDrawable::FromView(view);
  auto surface = pag::PAGSurface::MakeFrom(drawable);
  if (surface == nullptr) {
    return nil;
  }
  return [[[PAGSurfaceImpl alloc] initWithSurface:surface] autorelease];
}

@end
#elif defined(TGFX_USE_METAL)
@implementation PAGSurfaceImpl (Metal)

+ (PAGSurfaceImpl*)FromLayer:(CAMetalLayer*)layer {
  auto drawable = pag::MetalLayerDrawable::FromLayer(layer);
  auto surface = pag::PAGSurface::MakeFrom(drawable);
  if (surface == nullptr) {
    return nil;
  }
  return [[[PAGSurfaceImpl alloc] initWithSurface:surface] autorelease];
}

@end
#endif
