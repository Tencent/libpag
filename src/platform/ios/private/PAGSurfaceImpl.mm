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
#include "platform/cocoa/private/PixelBufferUtil.h"
#include "rendering/drawables/HardwareBufferDrawable.h"

#if defined(TGFX_USE_OPENGL)
#import "platform/ios/private/opengl/EAGLDrawable.h"
#import "platform/ios/private/opengl/PAGSurfaceImpl+OpenGL.h"
#include "tgfx/gpu/opengl/eagl/EAGLDevice.h"
#elif defined(TGFX_USE_METAL)
#import "platform/cocoa/private/metal/MetalLayerDrawable.h"
#import "platform/ios/private/metal/PAGSurfaceImpl+Metal.h"
#include "tgfx/gpu/metal/MetalDevice.h"
#endif

@interface PAGSurfaceImpl ()

@property(nonatomic) std::shared_ptr<pag::PAGSurface> pagSurface;

@end

@implementation PAGSurfaceImpl {
  CVPixelBufferRef pixelBuffer;
}

- (std::shared_ptr<pag::PAGSurface>)pagSurface {
  return _pagSurface;
}

+ (PAGSurfaceImpl*)FromCVPixelBuffer:(CVPixelBufferRef)pixelBuffer {
#if defined(TGFX_USE_OPENGL)
  return [PAGSurfaceImpl FromCVPixelBuffer:pixelBuffer context:nil];
#else
  return [PAGSurfaceImpl FromCVPixelBuffer:pixelBuffer mtlDevice:nil];
#endif
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

// The default pixel-buffer factory dispatches to these methods. Static linking must load them
// whenever the PAGSurfaceImpl class is referenced.
#if defined(TGFX_USE_OPENGL)
@implementation PAGSurfaceImpl (OpenGL)

+ (PAGSurfaceImpl*)FromLayer:(CAEAGLLayer*)layer {
  auto drawable = pag::EAGLDrawable::FromLayer(layer);
  auto surface = pag::PAGSurface::MakeFrom(drawable);
  if (surface == nullptr) {
    return nil;
  }
  return [[[PAGSurfaceImpl alloc] initWithSurface:surface] autorelease];
}

+ (PAGSurfaceImpl*)FromCVPixelBuffer:(CVPixelBufferRef)pixelBuffer
                             context:(EAGLContext*)eaglContext {
#if TARGET_IPHONE_SIMULATOR
  // Only the EAGL backend disables hardware buffers on the simulator.
  LOGE("The simulator does not support [PAGSurface FromCVPixelBuffer:context:].");
  return nil;
#else
  auto device = tgfx::EAGLDevice::MakeFrom(eaglContext);
  auto drawable = pag::HardwareBufferDrawable::MakeFrom(pixelBuffer, device);
  auto surface = pag::PAGSurface::MakeFrom(drawable);
  if (surface == nullptr) {
    return nil;
  }
  return [[[PAGSurfaceImpl alloc] initWithSurface:surface pixelBuffer:pixelBuffer] autorelease];
#endif
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

+ (PAGSurfaceImpl*)FromCVPixelBuffer:(CVPixelBufferRef)pixelBuffer mtlDevice:(id<MTLDevice>)device {
  // Metal hardware buffers are supported on arm64 simulators through CVMetalTextureCache.
  std::shared_ptr<tgfx::MetalDevice> metalDevice = nullptr;
  if (device != nil) {
    metalDevice = tgfx::MetalDevice::MakeFrom(device);
  }
  auto drawable = pag::HardwareBufferDrawable::MakeFrom(pixelBuffer, metalDevice);
  auto surface = pag::PAGSurface::MakeFrom(drawable);
  if (surface == nullptr) {
    return nil;
  }
  return [[[PAGSurfaceImpl alloc] initWithSurface:surface pixelBuffer:pixelBuffer] autorelease];
}

@end
#endif
