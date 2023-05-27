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
static constexpr int AnimationTypeEnd = 1;
static constexpr int AnimationTypeRepeat = 2;
static constexpr int AnimationTypeUpdate = 3;

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
      if (animator.unique()) {
        animator->cancelAnimation();
        continue;
      }
      animator->advance();
    }
  }
};

static auto& animationTicker = *new AnimationTicker();

std::shared_ptr<PAGAnimator> PAGAnimator::MakeFrom(Listener* listener) {
  if (listener == nullptr || !animationTicker.available()) {
    return nullptr;
  }
  auto animator = std::shared_ptr<PAGAnimator>(new PAGAnimator(listener));
  animator->weakThis = animator;
  return animator;
}

PAGAnimator::PAGAnimator(Listener* listener) : listener(listener) {
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
  if (_isSync) {
    resetTask();
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
  resetStartTime();
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
  listener->onAnimationStart(this);
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
    resetTask();
    if (!_isPlaying) {
      return;
    }
    _isPlaying = false;
    playedCount = 0;
    cancelAnimation();
  }
  listener->onAnimationCancel(this);
}

void PAGAnimator::flush() {
  doFlush(false);
}

void PAGAnimator::advance() {
  auto events = doAdvance();
  if (events.empty()) {
    return;
  }
  doFlush(true);
  for (auto& type : events) {
    switch (type) {
      case AnimationTypeEnd:
        listener->onAnimationEnd(this);
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

std::vector<int> PAGAnimator::doAdvance() {
  std::lock_guard<std::mutex> autoLock(locker);
  if (!_isPlaying || _duration <= 0) {
    return {};
  }
  int64_t playTime;
  if (_startTime == INT64_MIN) {
    playTime =
        static_cast<int64_t>(_progress * static_cast<double>(_duration)) + playedCount * _duration;
  } else {
    playTime = tgfx::Clock::Now() - _startTime;
    auto fraction = static_cast<double>(playTime) / static_cast<double>(_duration);
    if (fraction < 0) {
      fraction = 0;
    }
    if (_repeatCount > 0 && fraction > _repeatCount) {
      fraction = _repeatCount;
    }
    _progress = ClampProgress(fraction);
  }
  std::vector<int> events = {};
  auto count = static_cast<int>(playTime / _duration);
  if (_repeatCount > 0) {
    if (count >= _repeatCount) {
      // Set the playedCount to 0 to allow the animation to be played again.
      playedCount = 0;
      isEnded = true;
      _isPlaying = false;
      cancelAnimation();
      events.push_back(AnimationTypeUpdate);
      events.push_back(AnimationTypeEnd);
    } else {
      if (count > playedCount) {
        playedCount = count;
        events.push_back(AnimationTypeRepeat);
      }
      events.push_back(AnimationTypeUpdate);
    }
  } else {
    events.push_back(AnimationTypeUpdate);
  }
  return events;
}

void PAGAnimator::doFlush(bool updateStartTime) {
  locker.lock();
  if (task != nullptr && task->executing()) {
    locker.unlock();
    return;
  }
  auto isSync = _isSync;
  locker.unlock();
  if (isSync) {
    onFlush(updateStartTime);
  } else {
    task = tgfx::Task::Run(
        [animator = weakThis.lock(), updateStartTime]() { animator->onFlush(updateStartTime); });
  }
}

void PAGAnimator::onFlush(bool updateStartTime) {
  listener->onAnimationFlush(this);
  std::lock_guard<std::mutex> autoLock(locker);
  if (updateStartTime && _startTime == INT64_MIN) {
    _startTime = tgfx::Clock::Now() -
                 static_cast<int64_t>(_progress * static_cast<double>(_duration)) -
                 playedCount * _duration;
  }
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
  resetStartTime();
  animationTicker.removeAnimator(weakThis.lock());
}

void PAGAnimator::resetStartTime() {
  _startTime = INT64_MIN;
}

void PAGAnimator::resetTask() {
  if (task != nullptr) {
    task->cancel();
    task->wait();
    task = nullptr;
  }
}
}  // namespace pag
