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

#pragma once

#import <CoreGraphics/CoreGraphics.h>
#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#import "PAGImageLayer.h"

// Backend-specific factories are declared in separate category headers. The generated
// PAGSurfaceBackend.h imports the selected category after this class declaration, so importing
// PAGSurface.h alone exposes all factories supported by the framework.
PAG_API @interface PAGSurface : NSObject

/**
 * Creates a new PAGSurface from specified CVPixelBuffer. The GPU context will be created internally
 * by PAGSurface. On the simulator this returns nil when the GL build's EAGL backend has no
 * hardware buffers; Metal builds are not affected.
 */
+ (PAGSurface*)FromCVPixelBuffer:(CVPixelBufferRef)pixelBuffer;

/**
 * [Deprecated](Please use [PAGSurface MakeOffscreen] instead)
 * Creates an offscreen PAGSurface of the specified size for pixel reading.
 */
+ (PAGSurface*)MakeFromGPU:(CGSize)size
    DEPRECATED_MSG_ATTRIBUTE("Please use [PAGSurface MakeOffscreen] instead.");

/**
 * Creates an offscreen PAGSurface of the specified size for pixel reading.
 */
+ (PAGSurface*)MakeOffscreen:(CGSize)size;

/**
 * The width of surface in pixels.
 */
- (int)width;

/**
 * The height of the surface in pixels.
 */
- (int)height;

/**
 * Update the size of the surface, and reset the internal surface.
 */
- (void)updateSize;

/**
 * Erases all pixels of this surface with transparent color. Returns true if the content has
 * changed.
 */
- (BOOL)clearAll;

/**
 * Free the cache created by the surface immediately. Can be called to reduce memory pressure.
 */
- (void)freeCache;

/**
 * Returns the internal CVPixelBuffer object associated with this PAGSurface. Returns nil if the
 * PAGSurface is created from a layer — [PAGSurface FromLayer:] with either a CAEAGLLayer
 * (OpenGL backend) or a CAMetalLayer (Metal backend) — or if the app is running in the simulator
 * on the OpenGL backend.
 */
- (CVPixelBufferRef)getCVPixelBuffer;

/**
 * Returns a CVPixelBuffer object capturing the contents of the PAGSurface, or nil if readback
 * fails. Subsequent rendering of the PAGSurface will not be captured.
 * On a PAGSurface created from a CAMetalLayer, readback is rejected if the layer is
 * framebuffer-only or the frame's drawing batch has already been flushed. A presented window frame
 * is not retained for later snapshots. Use an offscreen PAGSurface ([PAGSurface MakeOffscreen:]) if
 * you need reliable readback with Metal.
 */
- (CVPixelBufferRef)makeSnapshot;

/**
 * Copies the pixels of the PAGSurface to the specified memory address. The format of the copied
 * pixels is in the BGRA color type with the premultiplied alpha type. Returns NO if copying fails,
 * including when CAMetalLayer readback is rejected as described by makeSnapshot.
 */
- (BOOL)copyPixelsTo:(void*)pixels rowBytes:(size_t)rowBytes;

@end

#import "PAGSurfaceBackend.h"
