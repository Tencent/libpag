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
#import "PAGScaleMode.h"

NS_ASSUME_NONNULL_BEGIN

PAG_API @interface PAGDiskCacheConfig : NSObject

/**
 * Set the maximum size of the disk cache, in bytes.
 * Defaults to 1 GB.
 */
+ (void)SetMaxDiskSize:(NSUInteger)size;

/**
 * Get the maximum size of the disk cache, in bytes.
 */
+ (NSUInteger)MaxDiskSize;

/**
 * Get the total size of data in the disk cache, in bytes.
 */
+ (NSUInteger)totalSize;

/**
 * Clear all disk cache.
 */
+ (void)removeAllFiles;

/**
 * Clear the disk cache to the input size, in bytes, using  LRU.
 */
+ (void)removeFilesToSize:(NSUInteger)size;

@end

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
 * Returns the current PAGComposition for PAGImageView to render as content.
 */
- (PAGComposition* __nullable)getComposition;

/**
 * Sets a new PAGComposition for PAGImageView to render as content.
 * Note: If the composition is already added to another PAGImageView, it will be removed from the
 * previous PAGImageView.
 */
- (void)setComposition:(PAGComposition*)newComposition;

/**
 * The path string of a pag file set by setPath.
 */
- (NSString*)getPath;

/**
 * Load a pag file from the specified path, returns false if the file does not exist or the data is
 * not a pag file. Note: All PAGFiles loaded by the same path share the same internal cache. The
 * internal cache is alive until all PAGFiles are released.
 */
- (BOOL)setPath:(NSString*)filePath;

/**
 * Pre-load all animated image frame into memory or disk. Then later frame image request can
 * directly return the frame for index without decoding. This method may be called on background
 * thread.
 */
- (void)preloadAllFrames;

/**
 * Unload all animated image frame from memory  or disk if are already pre-loaded. Then later frame
 * image request need decoding. You can use this to free up the memory or disk usage if need.
 */
- (void)unloadAllFrames;

/**
 *  Set a switch for the memory cache, the default value is no.
 */
- (void)enableMemoryCache:(BOOL)enable;

/**
 * The maximum frame rate for rendering. If set to a value less than the actual frame rate from
 * PAGFile, it drops frames but increases performance. Otherwise, it has no effect. The default
 * value is 30.
 */
- (float)maxFrameRate;

/**
 * Set the maximum frame rate for rendering.
 */
- (void)setMaxFrameRate:(float)value;

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
 * Set the value of renderScale property.
 */
- (void)setRenderScale:(float)scale;

/**
 * Get the value of renderScale property.
 */
- (float)renderScale;

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
 * Starts animating the images in the receiver.
 */
- (void)startAnimating;

/**
 * Stops animating the images in the receiver.
 */
- (void)stopAnimating;

/**
 * The image displayed in the image view.
 */
- (UIImage*)currentImage;

/**
 * Returns a bool value indicating whether the animation is running.
 */
- (BOOL)animating;

@end

NS_ASSUME_NONNULL_END
