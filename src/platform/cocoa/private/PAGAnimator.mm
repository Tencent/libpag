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

#import "PAGAnimator.h"
#include "rendering/PAGAnimator.h"

namespace pag {
class AnimatorListener : public pag::PAGAnimator::Listener {
 public:
  explicit AnimatorListener(id<PAGAnimatorUpdater> updater) : updater(updater) {
    listeners = [[NSHashTable weakObjectsHashTable] retain];
  }

  ~AnimatorListener() override {
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
    auto copiedListeners = getListeners();
    for (id listener in copiedListeners) {
      if ([listener respondsToSelector:@selector(onAnimationStart:)]) {
        [listener onAnimationStart:updater];
      }
    }
    [copiedListeners release];
  }

  void onAnimationEnd(pag::PAGAnimator*) override {
    auto copiedListeners = getListeners();
    for (id listener in copiedListeners) {
      if ([listener respondsToSelector:@selector(onAnimationEnd:)]) {
        [listener onAnimationEnd:updater];
      }
    }
    [copiedListeners release];
  }

  void onAnimationCancel(pag::PAGAnimator*) override {
    auto copiedListeners = getListeners();
    for (id listener in copiedListeners) {
      if ([listener respondsToSelector:@selector(onAnimationCancel:)]) {
        [listener onAnimationCancel:updater];
      }
    }
    [copiedListeners release];
  }

  void onAnimationRepeat(pag::PAGAnimator*) override {
    auto copiedListeners = getListeners();
    for (id listener in copiedListeners) {
      if ([listener respondsToSelector:@selector(onAnimationRepeat:)]) {
        [listener onAnimationRepeat:updater];
      }
    }
    [copiedListeners release];
  }

  void onAnimationUpdate(pag::PAGAnimator*) override {
    auto copiedListeners = getListeners();
    for (id listener in copiedListeners) {
      if ([listener respondsToSelector:@selector(onAnimationUpdate:)]) {
        [listener onAnimationUpdate:updater];
      }
    }
    [copiedListeners release];
  };

  void onAnimationFlush(pag::PAGAnimator* animator) override {
    auto progress = animator->progress();
    [updater onAnimationFlush:progress];
  }

 private:
  std::mutex locker = {};
  id<PAGAnimatorUpdater> updater = nil;
  NSHashTable* listeners = nil;

  NSHashTable* getListeners() {
    std::lock_guard<std::mutex> lock(locker);
    return listeners.copy;
  }
};
}  // namespace pag

@implementation PAGAnimator {
  std::shared_ptr<pag::PAGAnimator> animator;
  std::unique_ptr<pag::AnimatorListener> animatorListener;
}

- (instancetype)initWithUpdater:(id<PAGAnimatorUpdater>)updater {
  self = [super init];
  if (self) {
    self->animatorListener = updater ? std::make_unique<pag::AnimatorListener>(updater) : nullptr;
    self->animator = pag::PAGAnimator::MakeFrom(animatorListener.get());
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

- (bool)isPlaying {
  return animator->isPlaying();
}

- (void)play {
  animator->play();
}

- (void)pause {
  animator->pause();
}

- (void)stop {
  animator->stop();
}

- (void)flush {
  animator->flush();
}

@end
