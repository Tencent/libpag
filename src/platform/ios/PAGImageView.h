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
 * Notifies the beginning of the animation.
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
 * Notifies the frame updating of the animation.
 */
- (void)onAnimationUpdate:(PAGImageView*)pagView;

@end

PAG_API @interface PAGImageView : UIImageView

/**
 * Returns the size limit of the disk cache in bytes.
 */
+ (NSUInteger)MaxDiskSize;

/**
 * Sets the size limit of the disk cache in bytes. The default disk cache limit is 1 GB.
 */
+ (void)SetMaxDiskSize:(NSUInteger)size;

/**
 * Returns the current PAGComposition in the PAGImageView. Returns nil if the internal composition
 * was loaded from a pag file path.
 */
- (PAGComposition* __nullable)getComposition;

/**
 * Sets a new PAGComposition to the PAGImageView with the maxFrameRate set to 30 fps. Note: If the
 * composition is already added to another PAGImageView, it will be removed from the previous
 * PAGImageView.
 */
- (void)setComposition:(PAGComposition*)newComposition;

/**
 * Sets a new PAGComposition and the maxFrameRate limit to the PAGImageView. Note: If the
 * composition is already added to another PAGImageView, it will be removed from the previous
 * PAGImageView.
 */
- (void)setComposition:(PAGComposition*)newComposition maxFrameRate:(float)maxFrameRate;

/**
 * Returns the file path set by the setPath() method.
 */
- (nullable NSString*)getPath;

/**
 * Loads a pag file from the specified path, returns false if the file does not exist, or it is not
 * a valid pag file.
 */
- (BOOL)setPath:(NSString*)filePath;
/**
 * Loads a pag file from the specified path with the maxFrameRate limit, returns false if the file
 * does not exist, or it is not a valid pag file.
 */
- (BOOL)setPath:(NSString*)filePath maxFrameRate:(float)maxFrameRate;

/**
 * If set to true, the PAGImageView loads all image frames into the memory, which will significantly
 * increase the rendering performance but may cost lots of additional memory. Use it when you prefer
 * rendering speed over memory usage. If set to false, the PAGImageView loads only one image frame
 * at a time into the memory. The default value is false.
 */
- (BOOL)cacheAllFramesInMemory;

/**
 * Sets the value of the cacheAllFramesInMemory property.
 */
- (void)setCacheAllFramesInMemory:(BOOL)enable;

/**
 * This value defines the scale factor for the size of the cached image frames, which ranges from
 * 0.0 to 1.0. A scale factor less than 1.0 may result in blurred output, but it can reduce graphics
 * memory usage, increasing the rendering performance. The default value is 1.0.
 */
- (float)renderScale;

/**
 * Sets the value of the renderScale property.
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
 * Removes the specified listener from the set of listeners
 */
- (void)removeListener:(id<PAGImageViewListener>)listener;

/**
 * Returns the current frame index the PAGImageView is rendering.
 */
- (NSUInteger)currentFrame;

/**
 * Sets the frame index for the PAGImageView to render.
 */
- (void)setCurrentFrame:(NSUInteger)currentFrame;

/**
 * Returns a UIImage capturing the contents of the current PAGImageView.
 */
- (UIImage*)currentImage;

/**
 * Starts to play the animation.
 */
- (void)play;

/**
 * Pauses the animation at the current playing position. Calling the play method can resume the
 * animation from the last paused playing position.
 */
- (void)pause;

/**
 * Indicates whether this PAGImageView is playing.
 */
- (BOOL)isPlaying;

/**
 * Renders the current image frame immediately. Note that all the changes previously made to the
 * PAGImageView will only take effect after this method is called. If the play() method is already
 * called, there is no need to call it manually since it will be automatically called every frame.
 * Returns true if the content has changed.
 */
- (BOOL)flush;

@end

NS_ASSUME_NONNULL_END
