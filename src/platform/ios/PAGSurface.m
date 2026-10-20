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

#ifndef GLES_SILENCE_DEPRECATION
#define GLES_SILENCE_DEPRECATION 1
#endif
#import "PAGSurface.h"
#import "platform/ios/private/PAGSurface+Internal.h"
#import "platform/ios/private/PAGSurfaceImpl.h"

#if defined(TGFX_USE_OPENGL)
#import "PAGSurface+OpenGL.h"
#import "platform/ios/private/opengl/PAGSurfaceImpl+OpenGL.h"
#elif defined(TGFX_USE_METAL)
#import "PAGSurface+Metal.h"
#import "platform/ios/private/metal/PAGSurfaceImpl+Metal.h"
#endif

@implementation PAGSurface {
}

- (id)impl {
  return _surface;
}

+ (PAGSurface*)FromCVPixelBuffer:(CVPixelBufferRef)pixelBuffer {
  PAGSurfaceImpl* surface = [PAGSurfaceImpl FromCVPixelBuffer:pixelBuffer];
  if (surface == nil) {
    return nil;
  }
  PAGSurface* pagSurface = [[[PAGSurface alloc] init] autorelease];
  pagSurface.surface = surface;
  return pagSurface;
}

+ (PAGSurface*)MakeFromGPU:(CGSize)size {
  return [PAGSurface MakeOffscreen:size];
}

+ (PAGSurface*)MakeOffscreen:(CGSize)size {
  PAGSurfaceImpl* surface = [PAGSurfaceImpl MakeOffscreen:size];
  if (surface == nil) {
    return nil;
  }
  PAGSurface* pagSurface = [[[PAGSurface alloc] init] autorelease];
  pagSurface.surface = surface;
  return pagSurface;
}

- (void)dealloc {
  [_surface release];
  [super dealloc];
}

- (void)updateSize {
  [_surface updateSize];
}

- (int)width {
  return [_surface width];
}

- (int)height {
  return [_surface height];
}

- (BOOL)clearAll {
  return [_surface clearAll];
}

- (void)freeCache {
  [_surface freeCache];
}

- (CVPixelBufferRef)getCVPixelBuffer {
  return [_surface getCVPixelBuffer];
}

- (CVPixelBufferRef)makeSnapshot {
  return [_surface makeSnapshot];
}

- (BOOL)copyPixelsTo:(void*)pixels rowBytes:(size_t)rowBytes {
  return [_surface copyPixelsTo:pixels rowBytes:rowBytes];
}

@end

// Keep category implementations with the class so static linking loads their methods without -ObjC.
#if defined(TGFX_USE_OPENGL)
@implementation PAGSurface (OpenGL)

+ (PAGSurface*)FromLayer:(CAEAGLLayer*)layer {
  PAGSurfaceImpl* surface = [PAGSurfaceImpl FromLayer:layer];
  if (surface == nil) {
    return nil;
  }
  PAGSurface* pagSurface = [[[PAGSurface alloc] init] autorelease];
  pagSurface.surface = surface;
  return pagSurface;
}

+ (PAGSurface*)FromCVPixelBuffer:(CVPixelBufferRef)pixelBuffer context:(EAGLContext*)eaglContext {
  PAGSurfaceImpl* surface = [PAGSurfaceImpl FromCVPixelBuffer:pixelBuffer context:eaglContext];
  if (surface == nil) {
    return nil;
  }
  PAGSurface* pagSurface = [[[PAGSurface alloc] init] autorelease];
  pagSurface.surface = surface;
  return pagSurface;
}

@end
#elif defined(TGFX_USE_METAL)
@implementation PAGSurface (Metal)

+ (PAGSurface*)FromLayer:(CAMetalLayer*)metalLayer {
  PAGSurfaceImpl* surface = [PAGSurfaceImpl FromLayer:metalLayer];
  if (surface == nil) {
    return nil;
  }
  PAGSurface* pagSurface = [[[PAGSurface alloc] init] autorelease];
  pagSurface.surface = surface;
  return pagSurface;
}

@end
#endif
