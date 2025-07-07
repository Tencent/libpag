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

#import "PAGFile.h"
#import "PAGImage.h"
#import "PAGLayer.h"

@class PAGView;
@protocol PAGViewListener <NSObject>

@optional
/**
 * Notifies the beginning of the animation. It can be called from either the UI thread or the thread
 * that calls the play method.
 */
- (void)onAnimationStart:(PAGView*)pagView;

/**
 * Notifies the end of the animation. It can only be called from the UI thread.
 */
- (void)onAnimationEnd:(PAGView*)pagView;

/**
 * Notifies the cancellation of the animation. It can be called from either the UI thread or the
 * thread that calls the stop method.
 */
- (void)onAnimationCancel:(PAGView*)pagView;

/**
 * Notifies the repetition of the animation. It can only be called from the UI thread.
 */
- (void)onAnimationRepeat:(PAGView*)pagView;

/**
 * Notifies another frame of the animation has occurred. It may be called from an arbitrary
 * thread if the animation is running asynchronously.
 */
- (void)onAnimationUpdate:(PAGView*)pagView;

@end

PAG_API @interface PAGView : NSView

/**
 * Adds a listener to the set of listeners that are sent events through the life of an animation,
 * such as start, repeat, and end. PAGView only holds a weak reference to the listener.
 */
- (void)addListener:(id<PAGViewListener>)listener;

/**
 * Removes a listener from the set listening to this animation.
 */
- (void)removeListener:(id<PAGViewListener>)listener;

/**
 * The total number of times the animation is set to play. The default is 1, which means the
 * animation will play only once. If the repeat count is set to 0 or a negative value, the
 * animation will play infinity times.
 */
- (int)repeatCount;

/**
 * Set the number of times the animation to play.
 */
- (void)setRepeatCount:(int)repeatCount;

/**
 * Indicates whether this pag view is playing.
 */
- (BOOL)isPlaying;

/**
 * Starts to play the animation from the current position. Calling the play() method when the
 * animation is already playing has no effect. The play() method does not alter the animation's
 * current position. However, if the animation previously reached its end, it will restart from
 * the beginning.
 */
- (void)play;

/**
 * Cancels the animation at the current position. Calling the play() method can resume the animation
 * from the last paused position.
 */
- (void)pause;

/**
 * Cancels the animation at the current position. Currently, it has the same effect as pause().
 */
- (void)stop;

/**
 * The path string of a pag file set by setPath.
 */
- (NSString*)getPath;

/**
 * Load a pag file from the specified path, returns false if the file does not exist or the data is
 * not a pag file. Note: All PAGFiles loaded by the same path share the same internal cache. The
 * internal cache is alive until all PAGFiles are released. Use '[PAGFile Load:size:]' instead if
 * you don't want to load a PAGFile from the intenal caches.
 */
- (BOOL)setPath:(NSString*)filePath;

/**
 * Returns the current PAGComposition for PAGView to render as content.
 */
- (PAGComposition*)getComposition;

/**
 * Sets a new PAGComposition for PAGView to render as content.
 * Note: If the composition is already added to another PAGView, it will be removed from the
 * previous PAGView.
 */
- (void)setComposition:(PAGComposition*)newComposition;

/**
 * Return the value of videoEnabled property.
 */
- (BOOL)videoEnabled;

/**
 * If set false, will skip video layer drawing.
 */
- (void)setVideoEnabled:(BOOL)enable;

/**
 * If set to true, PAG renderer caches an internal bitmap representation of the static content for
 * each layer. This caching can increase performance for layers that contain complex vector content.
 * The execution speed can be significantly faster depending on the complexity of the content, but
 * it requires extra graphics memory. The default value is true.
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
 * The maximum frame rate for rendering. If set to a value less than the actual frame rate from
 * PAGFile, it drops frames but increases performance. Otherwise, it has no effect. The default
 * value is 60.
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
 * Returns a copy of the current matrix.
 */
- (CGAffineTransform)matrix;

/**
 * Set the transformation which will be applied to the composition. The method is valid only
 * when the scaleMode is PAGScaleMode.None.
 */
- (void)setMatrix:(CGAffineTransform)value;

/**
 * The duration of current composition in microseconds.
 */
- (int64_t)duration;

/**
 * Returns the current progress of play position, the value is from 0.0 to 1.0. It is applied only
 * when the composition is not null.
 */
- (double)getProgress;

/**
 * Set the progress of play position, the value is from 0.0 to 1.0.
 */
- (void)setProgress:(double)value;

/**
 * Call this method to render current position immediately. If the play() method is already
 * called, there is no need to call it. Returns true if the content has changed.
 */
- (BOOL)flush;

/**
 * Returns an array of layers that lie under the specified point.
 * The point is in pixels, convert dp to pixel by multiply [UIScreen mainScreen].scale, eg: point.x
 * * scale,point.y* scale.
 */
- (NSArray<PAGLayer*>*)getLayersUnderPoint:(CGPoint)point;

/**
 * Free the cache created by the pag view immediately. Can be called to reduce memory pressure.
 */
- (void)freeCache;

/**
 * Returns a CVPixelBuffer object capturing the contents of the PAGView. Subsequent rendering of
 * the PAGView will not be captured. Returns nil if the PAGView hasn't been presented yet.
 */
- (CVPixelBufferRef)makeSnapshot;

/**
 * Returns a rectangle in pixels that defines the displaying area of the specified layer, which is
 * in the coordinate of the PAGView.
 */
- (CGRect)getBounds:(PAGLayer*)pagLayer;
@end
