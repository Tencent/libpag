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

#import <CoreFoundation/CoreFoundation.h>

/**
 * This interface is used to update the animation.
 */
@protocol PAGAnimatorUpdater <NSObject>

/**
 * Notifies a new frame of the animation needs to be updated. It may be called from an arbitrary
 * thread if the animation is running asynchronously.
 */
- (void)onUpdate:(double)progress;

@end

/**
 * Interface for listening to the animation life cycle events.  These methods are optional, you can
 * implement only the events you care about.
 */
@protocol PAGAnimatorListener <NSObject>

@optional
/**
 * Notifies the beginning of the animation.
 */
- (void)onAnimationStart:(_Nullable id)view;

/**
 * Notifies the end of the animation.
 */
- (void)onAnimationEnd:(_Nullable id)view;

/**
 * Notifies the cancellation of the animation.
 */
- (void)onAnimationCancel:(_Nullable id)view;

/**
 * Notifies the repetition of the animation.
 */
- (void)onAnimationRepeat:(_Nullable id)view;

/**
 * Notifies the frame updating of the animation.
 */
- (void)onAnimationUpdate:(_Nullable id)view;

@end

@interface PAGAnimator : NSObject

/**
 * Initializes a new PAGAnimator object with the specified updater.
 */
- (_Nullable instancetype)initWithUpdater:(_Nullable id<PAGAnimatorUpdater>)updater;

/**
 * Adds a listener to the set of listeners that are sent events through the life of an animation,
 * such as start, repeat, and end. PAGAnimator only holds a weak reference to the listener.
 */
- (void)addListener:(_Nullable id<PAGAnimatorListener>)listener;

/**
 * Removes a listener from the set listening to this animation.
 */
- (void)removeListener:(_Nullable id<PAGAnimatorListener>)listener;

/**
 * Indicates whether the animation is allowed to run in the UI thread. The default value is NO.
 * Regardless of whether the animation runs asynchronously, all listener callbacks will be called
 * on the UI thread.
 */
- (BOOL)isSync;

/**
 * Set whether the animation is allowed to run in the UI thread.
 */
- (void)setSync:(BOOL)value;

/**
 * Returns the length of the animation in microseconds.
 */
- (int64_t)duration;

/**
 * Sets the length of the animation in microseconds.
 */
- (void)setDuration:(int64_t)duration;

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
 * Returns the current position of the animation, which is a number between 0.0 and 1.0.
 */
- (double)progress;

/**
 * Set the current progress of the animation.
 */
- (void)setProgress:(double)value;

/**
 * Indicates whether the animation is playing.
 */
- (bool)isPlaying;

/**
 * Starts to play the animation from the current position. Calling the play() method when the
 * animation is already playing has no effect. The play() method does not alter the animation's
 * current position. However, if the animation previously reached its end, it will restart from
 * the beginning.
 */
- (void)play;

/**
 * Pauses the animation at the current position. Calling the play() method can resume the
 * animation from the last paused playing position.
 */
- (void)pause;

/**
 * Stops the animation at the current position. Unlike pause(), stop() not only pauses the
 * animation but also resets the number of times the animation has played to 0. This method may
 * block the calling thread if there is an asynchronous task running.
 */
- (void)stop;

/**
 * Manually update the animation to the current progress without altering its playing status. If
 * the animation is currently running an asynchronous updating task, this action will not have any
 * effect.
 */
- (void)update;

@end
