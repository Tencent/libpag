/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

@class PAGImageView;

@protocol PAGImageViewListener <NSObject>

@optional
/**
 * Notifies the beginning of the animation. It can be called from either the UI thread or the thread
 * that calls the play method.
 */
- (void)onAnimationStart:(PAGImageView* _Nonnull)pagView;

/**
 * Notifies the end of the animation. It can only be called from the UI thread.
 */
- (void)onAnimationEnd:(PAGImageView* _Nonnull)pagView;

/**
 * Notifies the cancellation of the animation. It can be called from either the UI thread or the
 * thread that calls the stop method.
 */
- (void)onAnimationCancel:(PAGImageView* _Nonnull)pagView;

/**
 * Notifies the repetition of the animation. It can only be called from the UI thread.
 */
- (void)onAnimationRepeat:(PAGImageView* _Nonnull)pagView;

/**
 * Notifies another frame of the animation has occurred. It may be called from an arbitrary
 * thread if the animation is running asynchronously.
 */
- (void)onAnimationUpdate:(PAGImageView* _Nonnull)pagView;

@end

PAG_API @interface PAGImageView : UIImageView

/**
 * [Deprecated]
 * Returns the size limit of the disk cache in bytes.
 */
+ (NSUInteger)MaxDiskSize DEPRECATED_MSG_ATTRIBUTE("Please use [PAGDiskCache MaxDiskSize] instead");

/**
 * [Deprecated]
 * Sets the size limit of the disk cache in bytes. The default disk cache limit is 1 GB.
 */
+ (void)SetMaxDiskSize:(NSUInteger)size
    DEPRECATED_MSG_ATTRIBUTE("Please use [PAGDiskCache SetMaxDiskSize] instead");

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
- (void)setComposition:(nullable PAGComposition*)newComposition;

/**
 * Sets a new PAGComposition and the maxFrameRate limit to the PAGImageView. Note: If the
 * composition is already added to another PAGImageView, it will be removed from the previous
 * PAGImageView.
 */
- (void)setComposition:(nullable PAGComposition*)newComposition maxFrameRate:(float)maxFrameRate;

/**
 * Returns the file path set by the setPath() method.
 */
- (nullable NSString*)getPath;

/**
 * Loads a pag file from the specified path, returns false if the file does not exist, or it is not
 * a valid pag file.
 */
- (BOOL)setPath:(nullable NSString*)path;
/**
 * Loads a pag file from the specified path with the maxFrameRate limit, returns false if the file
 * does not exist, or it is not a valid pag file.
 */
- (BOOL)setPath:(nullable NSString*)filePath maxFrameRate:(float)maxFrameRate;

/**
 * Asynchronously load a PAG file from the specific path, a block with PAGFile will be called
 * when loading is complete. If loading fails, PAGFile will be set to nil.
 */
- (void)setPathAsync:(nullable NSString*)filePath
     completionBlock:(void (^_Nullable)(PAGFile* __nullable))callback;

/**
 * Asynchronously load a PAG file from the specific path with the maxFrameRate limit, a block
 * with PAGFile will be called when loading is complete. If loading fails, PAGFile will be set to
 * nil.
 */
- (void)setPathAsync:(nullable NSString*)filePath
        maxFrameRate:(float)maxFrameRate
     completionBlock:(void (^_Nullable)(PAGFile* __nullable))callback;

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
 * Adds a listener to the set of listeners that are sent events through the life of an animation,
 * such as start, repeat, and end. PAGImageView only holds a weak reference to the listener.
 */
- (void)addListener:(nullable id<PAGImageViewListener>)listener;

/**
 * Removes the specified listener from the set of listeners
 */
- (void)removeListener:(nullable id<PAGImageViewListener>)listener;

/**
 * Returns the current frame index the PAGImageView is rendering.
 */
- (NSUInteger)currentFrame;

/**
 * Sets the frame index for the PAGImageView to render.
 */
- (void)setCurrentFrame:(NSUInteger)currentFrame;

/**
 * Returns the number of frames in the PAGImageView in one loop. Note that the value may change
 * if the associated PAGComposition was modified.
 */
- (NSUInteger)numFrames;

/**
 * Returns a UIImage capturing the contents of the current PAGImageView.
 */
- (nullable UIImage*)currentImage;

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
 * Indicates whether this PAGImageView is playing.
 */
- (BOOL)isPlaying;

/**
 * Call this method to render current position immediately. Note that all the changes previously
 * made to the PAGImageView will only take effect after this method is called. If the play() method
 * is already called, there is no need to call it manually since it will be automatically called
 * every frame. Returns true if the content has changed.
 */
- (BOOL)flush;

@end
