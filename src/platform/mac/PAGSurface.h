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

#import <AppKit/AppKit.h>
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
 * [Deprecated](Please use [PAGSurface MakeOffscreen] instead.)
 * Creates an offscreen PAGSurface of the specified size for pixel reading.
 */
+ (PAGSurface*)MakeFromGPU:(CGSize)size
    DEPRECATED_MSG_ATTRIBUTE("Please use [PAGSurface MakeOffscreen] instead.");

/**
 * Creates an offscreen PAGSurface of the specified size for pixel reading.
 */
+ (PAGSurface*)MakeOffscreen:(CGSize)size;

/**
 * The width of the surface.
 */
- (int)width;

/**
 * The height of the surface.
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
 * PAGSurface is created from a view or a layer — [PAGSurface FromView:] on the OpenGL backend
 * or [PAGSurface FromLayer:] with a CAMetalLayer on the Metal backend.
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
@end

#import "PAGSurfaceBackend.h"
