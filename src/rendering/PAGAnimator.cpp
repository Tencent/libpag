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

#include "PAGAnimator.h"
#include "base/utils/TimeUtil.h"
#include "platform/Platform.h"
#include "tgfx/utils/Clock.h"
#include "tgfx/utils/Task.h"

namespace pag {
static constexpr int AnimationTypeStart = 1;
static constexpr int AnimationTypeEnd = 2;
static constexpr int AnimationTypeCancel = 3;
static constexpr int AnimationTypeRepeat = 4;
static constexpr int AnimationTypeUpdate = 5;

class AnimationUpdater {
 public:
  explicit AnimationUpdater(std::shared_ptr<PAGAnimationUpdater> updater)
      : updater(std::move(updater)) {
  }

  int64_t startTime() {
    return _startTime != INT64_MIN;
  }

  void resetStartTime() {
    _startTime = INT64_MIN;
  }

  void update(double progress, int64_t playTime = INT64_MIN) {
    updater->onUpdate(progress);
    if (_startTime == INT64_MIN && playTime != INT64_MIN) {
      _startTime = tgfx::Clock::Now() - playTime;
    }
  }

 private:
  std::atomic_int64_t _startTime = INT64_MIN;
  std::shared_ptr<PAGAnimationUpdater> updater = nullptr;
};

class AnimationTicker {
 public:
  AnimationTicker() {
    displayLink = Platform::Current()->createDisplayLink([this] { onFrameAvailable(); });
  }

  bool available() {
    return displayLink != nullptr;
  }

  void addAnimator(std::shared_ptr<PAGAnimator> animator) {
    std::lock_guard<std::mutex> autoLock(locker);
    auto needStart = animators.empty();
    animators.push_back(std::move(animator));
    if (needStart) {
      displayLink->start();
    }
  }

  void removeAnimator(std::shared_ptr<PAGAnimator> animator) {
    std::lock_guard<std::mutex> autoLock(locker);
    auto index = std::find(animators.begin(), animators.end(), animator);
    if (index != animators.end()) {
      animators.erase(index);
    }
    if (animators.empty()) {
      displayLink->stop();
    }
  }

 private:
  std::mutex locker = {};
  std::shared_ptr<DisplayLink> displayLink = nullptr;
  std::vector<std::shared_ptr<PAGAnimator>> animators = {};

  void onFrameAvailable() {
    locker.lock();
    auto listCopy = animators;
    locker.unlock();
    for (auto& animator : listCopy) {
      animator->advance();
    }
  }
};

static auto& animationTicker = *new AnimationTicker();

std::shared_ptr<PAGAnimator> PAGAnimator::MakeFrom(std::shared_ptr<PAGAnimationUpdater> updater) {
  if (updater == nullptr || !animationTicker.available()) {
    return nullptr;
  }
  auto animationUpdater = std::make_shared<AnimationUpdater>(std::move(updater));
  auto animator = std::shared_ptr<PAGAnimator>(new PAGAnimator(std::move(animationUpdater)));
  animator->weakThis = animator;
  return animator;
}

PAGAnimator::PAGAnimator(std::shared_ptr<AnimationUpdater> updater) : updater(std::move(updater)) {
}

void PAGAnimator::addListener(std::shared_ptr<PAGAnimatorListener> listener) {
  std::lock_guard<std::mutex> autoLock(locker);
  if (findListener(listener) != -1) {
    return;
  }
  listeners.push_back(std::move(listener));
}

void PAGAnimator::removeListener(std::shared_ptr<PAGAnimatorListener> listener) {
  std::lock_guard<std::mutex> autoLock(locker);
  auto index = findListener(std::move(listener));
  if (index == -1) {
    return;
  }
  listeners.erase(listeners.begin() + index);
}

bool PAGAnimator::isSync() {
  std::lock_guard<std::mutex> autoLock(locker);
  return _isSync;
}

void PAGAnimator::setSync(bool value) {
  std::lock_guard<std::mutex> autoLock(locker);
  if (_isSync == value) {
    return;
  }
  _isSync = value;
  if (_isSync && task != nullptr) {
    task->cancel();
    task->wait();
    task = nullptr;
  }
}

int64_t PAGAnimator::duration() {
  std::lock_guard<std::mutex> autoLock(locker);
  return _duration;
}

void PAGAnimator::setDuration(int64_t duration) {
  std::lock_guard<std::mutex> autoLock(locker);
  if (_duration == duration) {
    return;
  }
  _duration = duration;
  if (_isPlaying) {
    if (_duration > 0) {
      startAnimation();
    } else {
      cancelAnimation();
    }
  }
}

int PAGAnimator::repeatCount() {
  std::lock_guard<std::mutex> autoLock(locker);
  return _repeatCount;
}

void PAGAnimator::setRepeatCount(int repeatCount) {
  std::lock_guard<std::mutex> autoLock(locker);
  if (repeatCount < 0) {
    repeatCount = 0;
  }
  _repeatCount = repeatCount;
}

double PAGAnimator::progress() {
  std::lock_guard<std::mutex> autoLock(locker);
  return _progress;
}

void PAGAnimator::setProgress(double value) {
  std::lock_guard<std::mutex> autoLock(locker);
  _progress = ClampProgress(value);
  isEnded = false;
  updater->resetStartTime();
}

bool PAGAnimator::isPlaying() {
  std::lock_guard<std::mutex> autoLock(locker);
  return _isPlaying;
}

void PAGAnimator::play() {
  {
    std::lock_guard<std::mutex> autoLock(locker);
    if (_isPlaying) {
      return;
    }
    _isPlaying = true;
    if (isEnded) {
      isEnded = false;
      _progress = 0.0;
    }
    startAnimation();
  }
  notifyListeners({AnimationTypeStart});
  advance();
}

void PAGAnimator::pause() {
  std::lock_guard<std::mutex> autoLock(locker);
  if (!_isPlaying) {
    return;
  }
  _isPlaying = false;
  cancelAnimation();
}

void PAGAnimator::stop() {
  {
    std::lock_guard<std::mutex> autoLock(locker);
    if (!_isPlaying) {
      return;
    }
    _isPlaying = false;
    playedCount = 0;
    cancelAnimation();
  }
  notifyListeners({AnimationTypeCancel});
}

void PAGAnimator::update() {
  locker.lock();
  if (task != nullptr && task->executing()) {
    locker.unlock();
    return;
  }
  if (_isSync) {
    updater->update(_progress);
  } else {
    task =
        tgfx::Task::Run([updater = updater, progress = _progress]() { updater->update(progress); });
  }
  locker.unlock();
  notifyListeners({AnimationTypeUpdate});
}

void PAGAnimator::advance() {
  auto types = doAdvance();
  notifyListeners(types);
}

std::vector<int> PAGAnimator::doAdvance() {
  std::lock_guard<std::mutex> autoLock(locker);
  if (!_isPlaying || _duration <= 0 || (task != nullptr && task->executing())) {
    return {};
  }
  int64_t playTime;
  if (updater->startTime() == INT64_MIN) {
    playTime =
        static_cast<int64_t>(_progress * static_cast<double>(_duration)) + playedCount * _duration;
  } else {
    playTime = tgfx::Clock::Now() - updater->startTime();
    updateProgress(playTime);
  }
  if (_isSync) {
    updater->update(_progress, playTime);
  } else {
    task = tgfx::Task::Run([updater = updater, progress = _progress, playTime]() {
      updater->update(progress, playTime);
    });
  }
  std::vector<int> types = {};
  auto count = static_cast<int>(playTime / _duration);
  if (_repeatCount > 0) {
    if (count >= _repeatCount) {
      // Set the playedCount to 0 to allow the animation to be played again.
      playedCount = 0;
      isEnded = true;
      _isPlaying = false;
      cancelAnimation();
      types.push_back(AnimationTypeUpdate);
      types.push_back(AnimationTypeEnd);
    } else {
      if (count > playedCount) {
        playedCount = count;
        types.push_back(AnimationTypeRepeat);
      }
      types.push_back(AnimationTypeUpdate);
    }
  } else {
    types.push_back(AnimationTypeUpdate);
  }
  return types;
}

void PAGAnimator::updateProgress(int64_t playTime) {
  auto fraction = static_cast<double>(playTime) / static_cast<double>(_duration);
  if (fraction < 0) {
    fraction = 0;
  }
  if (_repeatCount > 0 && fraction > _repeatCount) {
    fraction = _repeatCount;
  }
  _progress = ClampProgress(fraction);
}

void PAGAnimator::startAnimation() {
  if (isAnimating || _duration <= 0) {
    return;
  }
  isAnimating = true;
  animationTicker.addAnimator(weakThis.lock());
}

void PAGAnimator::cancelAnimation() {
  if (!isAnimating) {
    return;
  }
  isAnimating = false;
  updater->resetStartTime();
  animationTicker.removeAnimator(weakThis.lock());
}

int PAGAnimator::findListener(std::shared_ptr<PAGAnimatorListener> listener) {
  auto count = static_cast<int>(listeners.size());
  for (auto i = 0; i < count; i++) {
    if (listeners[i] == listener) {
      return i;
    }
  }
  return -1;
}

void PAGAnimator::notifyListeners(std::vector<int> types) {
  if (types.empty()) {
    return;
  }
  locker.lock();
  auto listenersCopy = listeners;
  locker.unlock();
  for (auto& type : types) {
    for (auto& listener : listenersCopy) {
      switch (type) {
        case AnimationTypeStart:
          listener->onAnimationStart(this);
          break;
        case AnimationTypeEnd:
          listener->onAnimationEnd(this);
          break;
        case AnimationTypeCancel:
          listener->onAnimationCancel(this);
          break;
        case AnimationTypeRepeat:
          listener->onAnimationRepeat(this);
          break;
        case AnimationTypeUpdate:
          listener->onAnimationUpdate(this);
          break;
        default:
          break;
      }
    }
  }
}
}  // namespace pag
