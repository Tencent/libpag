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
class AnimationUpdaterWrapper : public pag::PAGAnimator::Updater {
 public:
  explicit AnimationUpdaterWrapper(id<PAGAnimationUpdater> updater) : updater(updater) {
    [updater retain];
  }

  ~AnimationUpdaterWrapper() override {
    [updater release];
  }

  void onUpdate(double progress) override {
    [updater onUpdate:progress];
  }

 private:
  id<PAGAnimationUpdater> updater;
};

class AnimatorListener : public pag::PAGAnimator::Listener {
 public:
  AnimatorListener(id<PAGAnimatorListener> listener, id view) : listener(listener), view(view) {
    [listener retain];
  }

  ~AnimatorListener() override {
    [listener release];
  }

  id<PAGAnimatorListener> getListener() {
    return listener;
  }

  void onAnimationStart(PAGAnimator*) override {
    if ([listener respondsToSelector:@selector(onAnimationStart:)]) {
      [listener onAnimationStart:view];
    }
  }

  void onAnimationEnd(PAGAnimator*) override {
    if ([listener respondsToSelector:@selector(onAnimationEnd:)]) {
      [listener onAnimationEnd:view];
    }
  }

  void onAnimationCancel(PAGAnimator*) override {
    if ([listener respondsToSelector:@selector(onAnimationCancel:)]) {
      [listener onAnimationCancel:view];
    }
  }

  void onAnimationRepeat(PAGAnimator*) override {
    if ([listener respondsToSelector:@selector(onAnimationRepeat:)]) {
      [listener onAnimationRepeat:view];
    }
  }

  void onAnimationUpdate(PAGAnimator*) override {
    if ([listener respondsToSelector:@selector(onAnimationUpdate:)]) {
      [listener onAnimationUpdate:view];
    }
  };

 private:
  id<PAGAnimatorListener> listener;
  id view;
};
}  // namespace pag

@implementation PAGAnimator {
  std::shared_ptr<pag::PAGAnimator> animator;
  std::vector<std::shared_ptr<pag::AnimatorListener>> listeners;
}

+ (nullable instancetype)MakeWithUpdater:(id<PAGAnimationUpdater>)updater {
  if (updater == nil) {
    return nil;
  }
  auto animationUpdater = std::make_shared<pag::AnimationUpdaterWrapper>(updater);
  auto animator = pag::PAGAnimator::MakeFrom(animationUpdater);
  if (animator == nullptr) {
    return nil;
  }
  return [[[PAGAnimator alloc] initWithAnimator:animator] autorelease];
}

- (instancetype)initWithAnimator:(std::shared_ptr<pag::PAGAnimator>)animator {
  self = [super init];
  if (self) {
    self->animator = animator;
  }
  return self;
}

- (void)addListener:(id<PAGAnimatorListener>)listener view:(id)view {
  if (listener == nil) {
    return;
  }
  auto result = std::find_if(listeners.begin(), listeners.end(),
                             [=](auto& item) { return item->getListener() == listener; });
  std::shared_ptr<pag::AnimatorListener> callback;
  if (result == listeners.end()) {
    callback = std::make_shared<pag::AnimatorListener>(listener, view);
    listeners.push_back(callback);
  } else {
    callback = *result;
  }
  animator->addListener(std::move(callback));
}

- (void)removeListener:(id<PAGAnimatorListener>)listener view:(id)view {
  auto result = std::find_if(listeners.begin(), listeners.end(),
                             [=](auto& item) { return item->getListener() == listener; });
  if (result == listeners.end()) {
    return;
  }
  auto callback = *result;
  listeners.erase(result);
  animator->removeListener(std::move(callback));
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
