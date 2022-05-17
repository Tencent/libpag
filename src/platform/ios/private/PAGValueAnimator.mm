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

#import "PAGValueAnimator.h"
#include <chrono>
#include <mutex>

@implementation PAGValueAnimator

static CADisplayLink* caDisplayLink = NULL;
static NSMutableArray* animators = [[NSMutableArray array] retain];
static std::mutex valueAnimatorLocker = {};
static int64_t AnimatorIdCount = 0;

static int64_t GetCurrentTimeUS() {
  static auto START_TIME = std::chrono::high_resolution_clock::now();
  auto now = std::chrono::high_resolution_clock::now();
  auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - START_TIME);
  return static_cast<int64_t>(ns.count() * 1e-3);
}

+ (void)StartDisplayLink {
  caDisplayLink = [CADisplayLink displayLinkWithTarget:[PAGValueAnimator class]
                                              selector:@selector(HandleDisplayLink:)];
  //这里本来是默认的mode，当ui处于drag模式下时,无法进行渲染， 所以改成commonmodes...
  [caDisplayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSRunLoopCommonModes];
}

+ (void)StopDisplayLink {
  [caDisplayLink invalidate];
  caDisplayLink = NULL;
}

+ (void)HandleDisplayLink:(CADisplayLink*)displayLink {
  auto timestamp = GetCurrentTimeUS();
  NSArray* copyanimators = animators.copy;
  //遍历copy数组
  for (id animator in copyanimators) {
    PAGValueAnimator* valueAnimator = (PAGValueAnimator*)animator;
    [valueAnimator onAnimationFrame:timestamp];
  }
  [copyanimators release];
}

+ (int64_t)AddAnimator:(PAGValueAnimator*)animator {
  std::lock_guard<std::mutex> autoLock(valueAnimatorLocker);
  [animators addObject:animator];
  if (animators.count == 1) {
    [PAGValueAnimator StartDisplayLink];
  }
  return ++AnimatorIdCount;
}

+ (void)RemoveAnimator:(int64_t)animatorId {
  std::lock_guard<std::mutex> autoLock(valueAnimatorLocker);
  NSUInteger realAnimatorIndex = 0;
  for (PAGValueAnimator* animator in animators) {
    if (animator.animatorId == animatorId) {
      realAnimatorIndex = [animators indexOfObject:animator];
      break;
    }
  }
  [animators removeObjectAtIndex:realAnimatorIndex];

  if (animators.count == 0) {
    [PAGValueAnimator StopDisplayLink];
  }
}

- (id)init {
  self = [super init];
  duration = 0;
  startTime = 0;
  playTime = 0;
  animatorId = NSIntegerMax;
  animatorListener = nil;
  repeatCount = 0;
  repeatedTimes = 0;
  return self;
}

- (void)onAnimationFrame:(int64_t)timestamp {
  auto count = (timestamp - startTime) / duration;
  if (repeatCount >= 0 && count > repeatCount) {
    playTime = duration;
    [self stop:false];
    animatedFraction = 1.0;
    [animatorListener onAnimationUpdate];
    [animatorListener onAnimationEnd];
  } else {
    if (repeatedTimes < count) {
      [animatorListener onAnimationRepeat];
    }
    playTime = (timestamp - startTime) % duration;
    animatedFraction = static_cast<double>(playTime) / duration;
    [animatorListener onAnimationUpdate];
  }
  repeatedTimes = (int)count;
}

- (void)setListener:(id)listener {
  animatorListener = listener;
}

- (int64_t)duration {
  return duration;
}

- (void)setDuration:(int64_t)value {
  duration = value;
}

- (double)getAnimatedFraction {
  return animatedFraction;
}

- (void)setCurrentPlayTime:(int64_t)time {
  if (duration <= 0) {
    return;
  }
  int64_t gapTime = playTime - time;
  playTime = time;
  startTime += gapTime % duration;
  animatedFraction = static_cast<double>(playTime) / duration;
  [animatorListener onAnimationUpdate];
}

- (BOOL)isPlaying {
  return animatorId != NSIntegerMax;
}

- (int)repeatedTimes {
  return repeatedTimes;
}

- (void)setRepeatedTimes:(int)value {
  repeatedTimes = value;
}

/**
 * Set the number of times the animation will repeat. The default is 0, which means the animation
 * will play only once. -1 means the animation will play infinity times.
 */
- (void)setRepeatCount:(int)value {
  repeatCount = value;
}

- (void)start {
  if (duration <= 0 || animatorId != NSIntegerMax || animatorListener == nil) {
    return;
  }
  animatorId = [PAGValueAnimator AddAnimator:self];
  startTime = GetCurrentTimeUS() - playTime % duration - repeatedTimes * duration;
  animatedFraction = static_cast<double>(playTime) / duration;
  [animatorListener onAnimationUpdate];
  if (animatedFraction == 0) {
    [animatorListener onAnimationStart];
  }
}

- (void)stop {
  [self stop:true];
}

- (void)stop:(bool)notification {
  if (animatorId == NSIntegerMax) {
    return;
  }
  [PAGValueAnimator RemoveAnimator:animatorId];
  animatorId = NSIntegerMax;
  if (notification) {
    [animatorListener onAnimationCancel];
  }
}

- (int64_t)animatorId {
  return animatorId;
}

@end
