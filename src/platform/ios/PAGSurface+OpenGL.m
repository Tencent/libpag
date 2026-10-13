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

#ifndef GLES_SILENCE_DEPRECATION
#define GLES_SILENCE_DEPRECATION 1
#endif
#import "PAGSurface+OpenGL.h"
#import "platform/ios/private/PAGSurface+Internal.h"
#import "platform/ios/private/opengl/PAGSurfaceImpl+OpenGL.h"

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
