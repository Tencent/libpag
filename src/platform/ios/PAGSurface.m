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

#import "PAGSurface.h"
#import "platform/ios/private/PAGSurfaceImpl.h"

@interface PAGSurface ()

@property(nonatomic, strong) PAGSurfaceImpl* surface;

@end

@implementation PAGSurface {
}

- (id)impl {
  return _surface;
}

+ (PAGSurface*)FromLayer:(CAEAGLLayer*)layer {
  PAGSurfaceImpl* surface = [PAGSurfaceImpl FromLayer:layer];
  if (surface == nil) {
    return nil;
  }
  PAGSurface* pagSurface = [[[PAGSurface alloc] init] autorelease];
  pagSurface.surface = surface;
  return pagSurface;
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

+ (PAGSurface*)FromCVPixelBuffer:(CVPixelBufferRef)pixelBuffer context:(EAGLContext*)eaglContext {
  PAGSurfaceImpl* surface = [PAGSurfaceImpl FromCVPixelBuffer:pixelBuffer context:eaglContext];
  if (surface == nil) {
    return nil;
  }
  PAGSurface* pagSurface = [[[PAGSurface alloc] init] autorelease];
  pagSurface.surface = surface;
  return pagSurface;
}

+ (PAGSurface*)MakeFromGPU:(CGSize)size {
  PAGSurfaceImpl* surface = [PAGSurfaceImpl MakeFromGPU:size];
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

@end
