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

#pragma once

#import <OpenGLES/EAGL.h>
#import <QuartzCore/QuartzCore.h>
#import "PAGSurface.h"

// The CAEAGLLayer / EAGLContext APIs below are deprecated since iOS 12 but remain part of the
// published GL-backend API surface. Silence the deprecation warnings locally: framework
// consumers compile this header without libpag's GLES_SILENCE_DEPRECATION build define, and
// defining that macro here would leak into their whole translation unit.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

PAG_API @interface PAGSurface(OpenGL)

/**
 * Creates a new PAGSurface from specified CAEAGLLayer. The GPU context will be created internally
 * by PAGSurface.
 */
+ (PAGSurface*)FromLayer:(CAEAGLLayer*)layer;

/**
 * Creates a new PAGSurface from specified CVPixelBuffer and EAGLContext. Multiple PAGSurfaces with
 * the same context share the same GPU caches. The caches are not destroyed when resetting a
 * PAGPlayer's surface to another PAGSurface with the same context. Returns nil when the app runs
 * in the simulator (the EAGL backend has no hardware buffers there).
 */
+ (PAGSurface*)FromCVPixelBuffer:(CVPixelBufferRef)pixelBuffer context:(EAGLContext*)eaglContext;

@end

#pragma clang diagnostic pop
