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

#import "PAGSurface.h"
#import "platform/mac/private/PAGSurfaceImpl.h"

@interface PAGSurface ()

@property(nonatomic, strong) PAGSurfaceImpl* surface;

@end

@implementation PAGSurface {
}

+ (PAGSurface*)FromView:(NSView*)view {
  PAGSurfaceImpl* surface = [PAGSurfaceImpl FromView:view];
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

@end
