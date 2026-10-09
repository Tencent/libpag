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

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#import "PAGImageLayer.h"

PAG_API @interface PAGSurface : NSObject

/**
 * Creates a new PAGSurface from a NSView. Returns nil if the current libpag build does not use
 * the OpenGL backend.
 */
+ (PAGSurface*)FromView:(NSView*)view;

/**
 * Creates a new PAGSurface from a CAMetalLayer. The MTLDevice on the layer (or the system
 * default) is adopted internally. Multiple PAGSurfaces whose layers share the same MTLDevice
 * share the same GPU caches. Returns nil if the current libpag build does not use the Metal
 * backend. The caller must keep the CAMetalLayer alive for the lifetime of the PAGSurface —
 * libpag does not retain it (retaining would risk a reference cycle in typical view-layer
 * setups), so releasing the layer while the PAGSurface is still in use is undefined behavior.
 * The caller must also maintain layer.drawableSize (in pixels) — CAMetalLayer does not derive it
 * from bounds automatically, and if it stays at zero the surface reports a fallback size but
 * never actually renders. When the size changes, update layer.drawableSize and call updateSize
 * on the PAGSurface.
 */
+ (PAGSurface*)FromMetalLayer:(CAMetalLayer*)metalLayer;

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
 * Returns the internal CVPixelBuffer object associated with this PAGSurface, returns nil if this
 * PAGSurface is created by [PAGSurface FromView].
 */
- (CVPixelBufferRef)getCVPixelBuffer;

/**
 * Returns a CVPixelBuffer object capturing the contents of the PAGSurface. Subsequent rendering of
 * the PAGSurface will not be captured.
 */
- (CVPixelBufferRef)makeSnapshot;
@end
