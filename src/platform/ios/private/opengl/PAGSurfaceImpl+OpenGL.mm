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

#import "platform/ios/private/opengl/PAGSurfaceImpl+OpenGL.h"

#if defined(TGFX_USE_OPENGL)

#include "base/utils/Log.h"
#include "platform/cocoa/private/PixelBufferUtil.h"
#import "platform/ios/private/PAGSurfaceImpl+Internal.h"
#import "platform/ios/private/opengl/EAGLDrawable.h"
#include "rendering/drawables/HardwareBufferDrawable.h"
#include "tgfx/gpu/opengl/eagl/EAGLDevice.h"

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
  // The EAGL GPU disables hardware buffers in the simulator (tgfx EAGLGPU), so pixel-buffer
  // surfaces cannot be created there. This is a GL-only restriction: Metal's
  // HardwareBufferAvailable() is true on the simulator, its pixel-buffer entry point in the
  // Metal category is not gated on TARGET_IPHONE_SIMULATOR.
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

#endif  // TGFX_USE_OPENGL
