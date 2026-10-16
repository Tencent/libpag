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

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#import "PAGImageLayer.h"

// Backend-specific factory categories (PAGSurface+OpenGL.h / PAGSurface+Metal.h) are selected by
// the libpag build and exported through the framework's umbrella header; only the
// backend-agnostic surface entry points stay in this class interface.
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
 * Returns a CVPixelBuffer object capturing the contents of the PAGSurface. Subsequent rendering of
 * the PAGSurface will not be captured.
 * Limitation: on a PAGSurface created from a CAMetalLayer this method is unreliable — the
 * underlying tgfx window presents and releases its CAMetalDrawable as part of every flush, so a
 * later readback receives whichever drawable comes off the layer's rotation queue (often blank or
 * stale). Use an offscreen PAGSurface ([PAGSurface MakeOffscreen:]) if you need reliable
 * readback with Metal. The OpenGL backend is not affected.
 */
- (CVPixelBufferRef)makeSnapshot;
@end
