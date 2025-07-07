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

+ (PAGSurface*)FromView:(NSView*)view;

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
 * PAGSurface is created by [PAGSurface FromLayer].
 */
- (CVPixelBufferRef)getCVPixelBuffer;

/**
 * Returns a CVPixelBuffer object capturing the contents of the PAGSurface. Subsequent rendering of
 * the PAGSurface will not be captured.
 */
- (CVPixelBufferRef)makeSnapshot;
@end
