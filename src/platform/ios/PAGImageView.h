/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#import <UIKit/UIKit.h>
#import "PAGComposition.h"

NS_ASSUME_NONNULL_BEGIN

@class PAGImageView;

@protocol PAGImageViewListener <NSObject>

@optional
/**
 * Notifies the start of the animation.
 */
- (void)onAnimationStart:(PAGImageView*)pagView;

/**
 * Notifies the end of the animation.
 */
- (void)onAnimationEnd:(PAGImageView*)pagView;

/**
 * Notifies the cancellation of the animation.
 */
- (void)onAnimationCancel:(PAGImageView*)pagView;

/**
 * Notifies the repetition of the animation.
 */
- (void)onAnimationRepeat:(PAGImageView*)pagView;

/**
 * Notifies the occurrence of another frame of the animation.
 */
- (void)onAnimationUpdate:(PAGImageView*)pagView;

@end

PAG_API @interface PAGImageView : UIImageView

/**
 * Get the maximum size of the disk cache, in bytes.
 */
+ (NSUInteger)MaxDiskSize;

/**
 * Set the maximum size of the disk cache, in bytes.
 * Defaults to 1 GB.
 */
+ (void)SetMaxDiskSize:(NSUInteger)size;

/**
 * Returns the current PAGComposition for PAGImageView to render as content.
 */
- (PAGComposition* __nullable)getComposition;

/**
 * Sets a new PAGComposition for PAGImageView to render as content. The maximum frame rate
 * for rendering is 30.
 * Note: If the composition is already added to another PAGImageView, it will be removed from the
 * previous PAGImageView.
 */
- (void)setComposition:(PAGComposition*)newComposition;

/**
 * Sets a new PAGComposition with maximum frame rate for PAGImageView to render as content.
 * Note: If the composition is already added to another PAGImageView, it will be removed from the
 * previous PAGImageView.
 */
- (void)setComposition:(PAGComposition*)newComposition maxFrameRate:(float)maxFrameRate;

/**
 * The path string of a pag file set by setPath.
 */
- (NSString*)getPath;

/**
 * Load a pag file from the specified path, returns false if the file does not exist or the data is
 * not a pag file. The maximum frame rate for rendering is 30.
 * Note: All PAGFiles loaded by the same path share the same internal cache. The internal
 * cache is alive until all PAGFiles are released.
 */
- (BOOL)setPath:(NSString*)filePath;
/**
 * Load a pag file from the specified path with maximum frame rate, returns false if the file
 * does not exist or the data is not a pag file.
 * Note: All PAGFiles loaded by the same path share the same internal cache. The internal
 * cache is alive until all PAGFiles are released.
 */
- (BOOL)setPath:(NSString*)filePath maxFrameRate:(float)maxFrameRate;

/**
 * Return memory cache status.
 */
- (BOOL)memoryCacheEnabled;

/**
 *  Set a switch for the memory cache, the default value is no.
 */
- (void)setMemoryCacheEnabled:(BOOL)enable;

/**
 * Returns a copy of current matrix.
 */
- (CGAffineTransform)matrix;

/**
 * Set the transformation which will be applied to the composition. The method is valid only
 * when the scaleMode is PAGScaleMode.None.
 */
- (void)setMatrix:(CGAffineTransform)value;

/**
 * Get the value of renderScale property.
 */
- (float)renderScale;

/**
 * Set the value of renderScale property.
 */
- (void)setRenderScale:(float)scale;

/**
 * Set the number of times the animation will repeat. The default is 1, which means the animation
 * will play only once. 0 means the animation will play infinity times.
 */
- (void)setRepeatCount:(int)repeatCount;

/**
 * Adds a listener to the set of listeners that are sent events through the life of an animation,
 * such as start, repeat, and end.
 */
- (void)addListener:(id<PAGImageViewListener>)listener;

/**
 * Removes a listener from the set listening to this animation.
 */
- (void)removeListener:(id<PAGImageViewListener>)listener;

/**
 *  Returns the current rendering frame.
 */
- (NSUInteger)currentFrame;

/**
 * Sets the frame index to render.
 */
- (void)setCurrentFrame:(NSUInteger)currentFrame;

/**
 * Start the animation.
 */
- (void)play;

/**
 * Pause the animation.
 */
- (void)pause;

/**
 * The image displayed in the image view.
 */
- (UIImage*)currentImage;

/**
 * Call this method to render current position immediately.
 * Returns true if rendering is successful, otherwise false
 */
- (BOOL)flush;

/**
 * Indicates whether or not this PAGImageView is playing.
 */
- (BOOL)isPlaying;

@end

NS_ASSUME_NONNULL_END
