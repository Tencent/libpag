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
class AnimationUpdater : public pag::PAGAnimator::Updater {
 public:
  explicit AnimationUpdater(id<PAGAnimatorUpdater> updater) : updater(updater) {
  }

  void onUpdate(double progress) override {
    [updater onUpdate:progress];
  }

 private:
  id<PAGAnimatorUpdater> updater;
};

class ListenerManager : public pag::PAGAnimator::Listener {
 public:
  explicit ListenerManager(id view) : view(view) {
    listeners = [[NSHashTable weakObjectsHashTable] retain];
  }

  ~ListenerManager() override {
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
  void onAnimationStart(PAGAnimator*) override {
    auto copiedListeners = getListeners();
    for (id listener in copiedListeners) {
      if ([listener respondsToSelector:@selector(onAnimationStart:)]) {
        [listener onAnimationStart:view];
      }
    }
    [copiedListeners release];
  }

  void onAnimationEnd(PAGAnimator*) override {
    auto copiedListeners = getListeners();
    for (id listener in copiedListeners) {
      if ([listener respondsToSelector:@selector(onAnimationEnd:)]) {
        [listener onAnimationEnd:view];
      }
    }
    [copiedListeners release];
  }

  void onAnimationCancel(PAGAnimator*) override {
    auto copiedListeners = getListeners();
    for (id listener in copiedListeners) {
      if ([listener respondsToSelector:@selector(onAnimationCancel:)]) {
        [listener onAnimationCancel:view];
      }
    }
    [copiedListeners release];
  }

  void onAnimationRepeat(PAGAnimator*) override {
    auto copiedListeners = getListeners();
    for (id listener in copiedListeners) {
      if ([listener respondsToSelector:@selector(onAnimationRepeat:)]) {
        [listener onAnimationRepeat:view];
      }
    }
    [copiedListeners release];
  }

  void onAnimationUpdate(PAGAnimator*) override {
    auto copiedListeners = getListeners();
    for (id listener in copiedListeners) {
      if ([listener respondsToSelector:@selector(onAnimationUpdate:)]) {
        [listener onAnimationUpdate:view];
      }
    }
    [copiedListeners release];
  };

 private:
  std::mutex locker = {};
  id view = nil;
  NSHashTable* listeners = nil;

  NSHashTable* getListeners() {
    std::lock_guard<std::mutex> lock(locker);
    return listeners.copy;
  }
};
}  // namespace pag

@implementation PAGAnimator {
  std::shared_ptr<pag::PAGAnimator> animator;
  std::unique_ptr<pag::PAGAnimator::Updater> animationUpdater;
  std::shared_ptr<pag::ListenerManager> listenerManager;
}

- (instancetype)initWithUpdater:(id<PAGAnimatorUpdater>)updater {
  self = [super init];
  if (self) {
    self->animationUpdater = updater ? std::make_unique<pag::AnimationUpdater>(updater) : nullptr;
    self->animator = pag::PAGAnimator::MakeFrom(animationUpdater.get());
    if (self->animator == nullptr) {
      [self release];
      return nil;
    }
    self->listenerManager = std::make_shared<pag::ListenerManager>(updater);
    self->animator->addListener(self->listenerManager);
  }
  return self;
}

- (void)addListener:(id<PAGAnimatorListener>)listener {
  listenerManager->addListener(listener);
}

- (void)removeListener:(id<PAGAnimatorListener>)listener {
  listenerManager->removeListener(listener);
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

- (void)update {
  animator->update();
}

@end
