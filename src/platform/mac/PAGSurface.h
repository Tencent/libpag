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

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#import "PAGImageLayer.h"

__attribute__((visibility("default"))) @interface PAGSurface : NSObject

+ (PAGSurface*)FromView:(NSView*)view;

+ (PAGSurface*)MakeFromGPU:(CGSize)size;

/**
 * The width of surface in pixels.
 */
- (int)width;

/**
 * The height of surface in pixels.
 */
- (int)height;

/**
 * Update the size of surface, and reset the internal surface.
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
 * Returns the CVPixelBuffer object created by MakeFromGPU.
 */
- (CVPixelBufferRef)getCVPixelBuffer;

/**
 * Returns a CVPixelBuffer object capturing the contents of the PAGSurface.
 * Subsequent rendering of the PAGSurface will not be captured.
 * @return
 */
- (CVPixelBufferRef)makeSnapshot;
@end
