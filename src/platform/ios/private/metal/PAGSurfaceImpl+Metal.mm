/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#import "platform/ios/private/metal/PAGSurfaceImpl+Metal.h"

#import <Metal/Metal.h>
#include "platform/cocoa/private/PixelBufferUtil.h"
#import "platform/cocoa/private/metal/MetalLayerDrawable.h"
#import "platform/ios/private/PAGSurfaceImpl+Internal.h"
#include "rendering/drawables/HardwareBufferDrawable.h"
#include "tgfx/gpu/metal/MetalDevice.h"

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
  // No TARGET_IPHONE_SIMULATOR gate here: Metal's HardwareBufferAvailable() is true on the
  // simulator (CVMetalTextureCache path), so pixel-buffer surfaces work on arm64 simulators.
  // The simulator restriction exists only in the GL category.
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
