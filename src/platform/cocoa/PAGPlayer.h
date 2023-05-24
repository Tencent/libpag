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

#import <Foundation/Foundation.h>
#import "PAGComposition.h"
#import "PAGFile.h"
#import "PAGImage.h"
#import "PAGScaleMode.h"
#import "PAGSurface.h"

PAG_API @interface PAGPlayer : NSObject
/**
 * Returns the current PAGComposition for PAGPlayer to render as content.
 */
- (PAGComposition*)getComposition;

/**
 * Sets a new PAGComposition for PAGPlayer to render as content.
 * Note: If the composition is already added to another PAGPlayer, it will be removed from the
 * previous PAGPlayer.
 */
- (void)setComposition:(PAGComposition*)newComposition;

/**
 * Returns the PAGSurface object for PAGPlayer to render onto.
 */
- (PAGSurface*)getSurface;

/**
 * Set the PAGSurface object for PAGPlayer to render onto.
 */
- (void)setSurface:(PAGSurface*)surface;

/**
 * If set to false, the player skips rendering for video composition.
 */
- (BOOL)videoEnabled;

/**
 * Set the value of videoEnabled property.
 */
- (void)setVideoEnabled:(BOOL)value;

/**
 * If set to true, PAGPlayer caches an internal bitmap representation of the static content for each
 * layer. This caching can increase performance for layers that contain complex vector content. The
 * execution speed can be significantly faster depending on the complexity of the content, but it
 * requires extra graphics memory. The default value is true.
 */
- (BOOL)cacheEnabled;

/**
 * Set the value of cacheEnabled property.
 */
- (void)setCacheEnabled:(BOOL)value;

/**
 * This value defines the scale factor for internal graphics caches, ranges from 0.0 to 1.0. The
 * scale factors less than 1.0 may result in blurred output, but it can reduce the usage of graphics
 * memory which leads to better performance. The default value is 1.0.
 */
- (float)cacheScale;

/**
 * Set the value of cacheScale property.
 */
- (void)setCacheScale:(float)value;

/**
 * The maximum frame rate for rendering, ranges from 1 to 60. If set to a value less than the actual
 * frame rate from composition, it drops frames but increases performance. Otherwise, it has no
 * effect. The default value is 60.
 */
- (float)maxFrameRate;

/**
 * Set the maximum frame rate for rendering.
 */
- (void)setMaxFrameRate:(float)value;

/**
 * Returns the current scale mode.
 */
- (PAGScaleMode)scaleMode;

/**
 * Specifies the rule of how to scale the pag content to fit the surface size. The matrix
 * changes when this method is called.
 */
- (void)setScaleMode:(PAGScaleMode)value;

/**
 * Returns a copy of current matrix.
 */
- (CGAffineTransform)matrix;

/**
 * Set the transformation which will be applied to the composition. The scaleMode property
 * will be set to PAGScaleMode::None when this method is called.
 */
- (void)setMatrix:(CGAffineTransform)value;

/**
 * The duration of current composition in microseconds.
 */
- (int64_t)duration;

/**
 * Returns the current progress of play position, the value is from 0.0 to 1.0.
 */
- (double)getProgress;

/**
 * Set the progress of play position, the value is from 0.0 to 1.0.
 */
- (void)setProgress:(double)value;

/**
 * Returns the current frame.
 */
- (int64_t)currentFrame;

/**
 * Prepares the player for the next flush() call. It collects all CPU tasks from the current
 * progress of the composition and runs them asynchronously in parallel. It is usually used for
 * speeding up the first frame rendering.
 */
- (void)prepare;

/**
 * Apply all pending changes to the target surface immediately. Returns true if the content has
 * changed.
 */
- (BOOL)flush;

/**
 * Returns a rectangle that defines the displaying area of the specified layer, which is in the
 * coordinate of the PAGSurface.
 */
- (CGRect)getBounds:(PAGLayer*)pagLayer;

/**
 * Returns an array of layers that lie under the specified point. The point is in pixels and from
 * the surface's coordinates.
 */
- (NSArray<PAGLayer*>*)getLayersUnderPoint:(CGPoint)point;

/**
 * Evaluates the PAGLayer to see if it overlaps or intersects with the specified point. The point is
 * in the coordinate space of the PAGSurface, not the PAGComposition that contains the PAGLayer. It
 * always returns false if the PAGLayer or its parent (or parent's parent...) has not been added to
 * this PAGPlayer. This method only check against the bounding box not the acual pixels.
 */
- (BOOL)hitTestPoint:(PAGLayer*)layer point:(CGPoint)point;

/**
 * Evaluates the PAGLayer to see if it overlaps or intersects with the specified point. The point is
 * in the coordinate space of the PAGSurface, not the PAGComposition that contains the PAGLayer. It
 * always returns false if the PAGLayer or its parent (or parent's parent...) has not been added to
 * this PAGPlayer. The pixelHitTest parameter indicates whether or not to check against the actual
 * pixels of the object (true) or the bounding box (false). Returns true if the PAGLayer overlaps or
 * intersects with the specified point.
 */
- (BOOL)hitTestPoint:(PAGLayer*)layer point:(CGPoint)point pixelHitTest:(BOOL)pixelHitTest;

@end
