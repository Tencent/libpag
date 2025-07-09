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

#import "PAGAnimator.h"
#import <Foundation/Foundation.h>
#include "rendering/PAGAnimator.h"

namespace pag {
class AnimatorListener : public pag::PAGAnimator::Listener {
 public:
  explicit AnimatorListener(id<PAGAnimatorUpdater> updater) {
    updaterTable = [[NSHashTable weakObjectsHashTable] retain];
    listeners = [[NSHashTable weakObjectsHashTable] retain];
    [updaterTable addObject:updater];
  }

  ~AnimatorListener() override {
    [updaterTable release];
    [listeners release];
  }

  void addListener(id<PAGAnimatorListener> listener) {
    std::lock_guard<std::mutex> lock(locker);
    if (listener == nil) {
      return;
    }
    [listeners addObject:listener];
  }

  void removeListener(id<PAGAnimatorListener> listener) {
    std::lock_guard<std::mutex> lock(locker);
    if (listener == nil) {
      return;
    }
    [listeners removeObject:listener];
  }

 protected:
  void onAnimationStart(pag::PAGAnimator*) override {
    auto updater = (id<PAGAnimatorUpdater>)[updaterTable.anyObject retain];
    if (updater == nil) {
      return;
    }
    auto copiedListeners = getListeners();
    for (id listener in copiedListeners) {
      if ([listener respondsToSelector:@selector(onAnimationStart:)]) {
        [listener onAnimationStart:updater];
      }
    }
    [copiedListeners release];
    [updater release];
  }

  void onAnimationEnd(pag::PAGAnimator*) override {
    auto updater = (id<PAGAnimatorUpdater>)[updaterTable.anyObject retain];
    if (updater == nil) {
      return;
    }
    auto copiedListeners = getListeners();
    for (id listener in copiedListeners) {
      if ([listener respondsToSelector:@selector(onAnimationEnd:)]) {
        [listener onAnimationEnd:updater];
      }
    }
    [copiedListeners release];
    [updater release];
  }

  void onAnimationCancel(pag::PAGAnimator*) override {
    auto updater = (id<PAGAnimatorUpdater>)[updaterTable.anyObject retain];
    if (updater == nil) {
      return;
    }
    auto copiedListeners = getListeners();
    for (id listener in copiedListeners) {
      if ([listener respondsToSelector:@selector(onAnimationCancel:)]) {
        [listener onAnimationCancel:updater];
      }
    }
    [copiedListeners release];
    [updater release];
  }

  void onAnimationRepeat(pag::PAGAnimator*) override {
    auto updater = (id<PAGAnimatorUpdater>)[updaterTable.anyObject retain];
    if (updater == nil) {
      return;
    }
    auto copiedListeners = getListeners();
    for (id listener in copiedListeners) {
      if ([listener respondsToSelector:@selector(onAnimationRepeat:)]) {
        [listener onAnimationRepeat:updater];
      }
    }
    [copiedListeners release];
    [updater release];
  }

  void onAnimationWillUpdate(PAGAnimator*) override {
    // [updaterTable.anyObject retain] is non-atomic and may lead to a wild pointer issue
    // if the object is released after obtaining anyObject.
    // Therefore, it is necessary to increment the reference count in the main thread before using.
    [updaterTable.anyObject retain];
  };

  void onAnimationUpdate(pag::PAGAnimator* animator) override {
    @autoreleasepool {
      auto updater = (id<PAGAnimatorUpdater>)updaterTable.anyObject;
      if (updater == nil) {
        return;
      }
      auto progress = animator->progress();
      [updater onAnimationFlush:progress];
      auto copiedListeners = getListeners();
      for (id listener in copiedListeners) {
        if ([listener respondsToSelector:@selector(onAnimationUpdate:)]) {
          [listener onAnimationUpdate:updater];
        }
      }
      [copiedListeners release];
      dispatch_async(dispatch_get_main_queue(), ^{
        [updaterTable.anyObject release];
      });
    }
  };

 private:
  std::mutex locker = {};
  NSHashTable* updaterTable = nil;
  NSHashTable* listeners = nil;

  NSHashTable* getListeners() {
    std::lock_guard<std::mutex> lock(locker);
    return listeners.copy;
  }
};
}  // namespace pag

@implementation PAGAnimator {
  std::shared_ptr<pag::PAGAnimator> animator;
  std::shared_ptr<pag::AnimatorListener> animatorListener;
}

- (instancetype)initWithUpdater:(id<PAGAnimatorUpdater>)updater {
  self = [super init];
  if (self) {
    self->animatorListener = updater ? std::make_shared<pag::AnimatorListener>(updater) : nullptr;
    self->animator = pag::PAGAnimator::MakeFrom(animatorListener);
    if (self->animator == nullptr) {
      [self release];
      return nil;
    }
  }
  return self;
}

- (void)addListener:(id<PAGAnimatorListener>)listener {
  animatorListener->addListener(listener);
}

- (void)removeListener:(id<PAGAnimatorListener>)listener {
  animatorListener->removeListener(listener);
}

- (BOOL)isSync {
  return animator->isSync();
}

- (void)setSync:(BOOL)value {
  animator->setSync(value);
}

- (int64_t)duration {
  return animator->duration();
}

- (void)setDuration:(int64_t)duration {
  animator->setDuration(duration);
}

- (int)repeatCount {
  return animator->repeatCount();
}

- (void)setRepeatCount:(int)repeatCount {
  animator->setRepeatCount(repeatCount);
}

- (double)progress {
  return animator->progress();
}

- (void)setProgress:(double)value {
  animator->setProgress(value);
}

- (bool)isRunning {
  return animator->isRunning();
}

- (void)start {
  animator->start();
}

- (void)cancel {
  animator->cancel();
}

- (void)update {
  animator->update();
}

@end
