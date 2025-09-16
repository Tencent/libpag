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

#include "PAGAnimator.h"
#include "base/utils/TimeUtil.h"
#include "platform/Platform.h"
#include "rendering/utils/DisplayLinkWrapper.h"
#include "tgfx/core/Clock.h"
#include "tgfx/core/Task.h"

namespace pag {
static constexpr int AnimationTypeEnd = 1;
static constexpr int AnimationTypeRepeat = 2;
static constexpr int AnimationTypeUpdate = 3;

class AnimationTicker {
 public:
  static AnimationTicker* GetInstance() {
    static auto& instance = *new AnimationTicker();
    return &instance;
  }

  AnimationTicker() {
    auto callback = [this] { onFrameAvailable(); };
    displayLink = DisplayLinkWrapper::Make(callback);
    if (displayLink == nullptr) {
      displayLink = Platform::Current()->createDisplayLink(callback);
    }
  }

  bool available() {
    return displayLink != nullptr;
  }

  void addAnimator(std::shared_ptr<PAGAnimator> animator) {
    locker.lock();
    auto needStart = animators.empty();
    animators.push_back(std::move(animator));
    locker.unlock();
    if (needStart) {
      displayLink->start();
    }
  }

  void removeAnimator(std::shared_ptr<PAGAnimator> animator) {
    locker.lock();
    auto index = std::find(animators.begin(), animators.end(), animator);
    if (index != animators.end()) {
      animators.erase(index);
    }
    auto needStop = animators.empty();
    locker.unlock();
    if (needStop) {
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
      if (animator.use_count() == 1) {
        animator->cancelAnimation();
        continue;
      }
      animator->advance();
    }
  }
};

std::shared_ptr<PAGAnimator> PAGAnimator::MakeFrom(std::weak_ptr<Listener> listener) {
  if (listener.expired() || !AnimationTicker::GetInstance()->available()) {
    return nullptr;
  }
  auto animator = std::shared_ptr<PAGAnimator>(new PAGAnimator(std::move(listener)));
  animator->weakThis = animator;
  return animator;
}

PAGAnimator::PAGAnimator(std::weak_ptr<Listener> listener) : weakListener(std::move(listener)) {
}

bool PAGAnimator::isSync() {
  std::lock_guard<std::mutex> autoLock(locker);
  return _isSync;
}

void PAGAnimator::setSync(bool value) {
  locker.lock();
  if (_isSync == value) {
    locker.unlock();
    return;
  }
  _isSync = value;
  auto tempTask = task;
  task = nullptr;
  locker.unlock();
  if (tempTask) {
    tempTask->wait();
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
  if (_isRunning) {
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
  playedCount = 0;
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

bool PAGAnimator::isRunning() {
  std::lock_guard<std::mutex> autoLock(locker);
  return _isRunning;
}

void PAGAnimator::start() {
  {
    std::lock_guard<std::mutex> autoLock(locker);
    if (_isRunning) {
      return;
    }
    _isRunning = true;
    if (isEnded) {
      isEnded = false;
      _progress = 0.0;
    }
    startAnimation();
  }
  auto listener = weakListener.lock();
  if (listener) {
    listener->onAnimationStart(this);
  }
  advance();
}

void PAGAnimator::cancel() {
  {
    std::lock_guard<std::mutex> autoLock(locker);
    if (!_isRunning) {
      return;
    }
    _isRunning = false;
    cancelAnimation();
  }
  auto listener = weakListener.lock();
  if (listener) {
    listener->onAnimationCancel(this);
  }
}

void PAGAnimator::update() {
  doUpdate(false);
}

void PAGAnimator::advance() {
  auto events = doAdvance();
  auto listener = weakListener.lock();
  if (listener == nullptr) {
    return;
  }
  for (auto& type : events) {
    switch (type) {
      case AnimationTypeEnd:
        listener->onAnimationEnd(this);
        break;
      case AnimationTypeRepeat:
        listener->onAnimationRepeat(this);
        break;
      case AnimationTypeUpdate:
        doUpdate(true);
      default:
        break;
    }
  }
}

std::vector<int> PAGAnimator::doAdvance() {
  std::lock_guard<std::mutex> autoLock(locker);
  if (!_isRunning || _duration <= 0) {
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
  if (_repeatCount > 0 && count >= _repeatCount) {
    // Set the playedCount to 0 to allow the animation to be played again.
    playedCount = 0;
    isEnded = true;
    _isRunning = false;
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
  return events;
}

void PAGAnimator::doUpdate(bool setStartTime) {
  locker.lock();
  if (task != nullptr && task->status() == tgfx::TaskStatus::Executing) {
    locker.unlock();
    return;
  }
  auto isSync = _isSync;
  locker.unlock();
  auto listener = weakListener.lock();
  if (listener) {
    listener->onAnimationWillUpdate(this);
  }
  if (isSync) {
    onFlush(setStartTime);
  } else {
    task = tgfx::Task::Run([weakThis = weakThis, setStartTime]() {
      auto animator = weakThis.lock();
      if (animator) {
        animator->onFlush(setStartTime);
      }
    });
  }
}

void PAGAnimator::onFlush(bool setStartTime) {
  auto listener = weakListener.lock();
  if (listener) {
    listener->onAnimationUpdate(this);
  }
  std::lock_guard<std::mutex> autoLock(locker);
  if (setStartTime && _startTime == INT64_MIN) {
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
  resetStartTime();
  AnimationTicker::GetInstance()->addAnimator(weakThis.lock());
}

void PAGAnimator::cancelAnimation() {
  if (!isAnimating) {
    return;
  }
  isAnimating = false;
  AnimationTicker::GetInstance()->removeAnimator(weakThis.lock());
}

void PAGAnimator::resetStartTime() {
  _startTime = INT64_MIN;
}

}  // namespace pag
