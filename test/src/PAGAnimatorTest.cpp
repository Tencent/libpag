/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
#include "base/PAGTest.h"
#include "rendering/PAGAnimator.h"

namespace pag {
class AnimatorListener : public PAGAnimator::Listener {
 public:
  bool waitForUpdateCount(size_t count, uint64_t timeout) {
    std::unique_lock<std::mutex> lock(locker);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
    while (updateCount < count) {
      if (condition.wait_until(lock, deadline) == std::cv_status::timeout) {
        return updateCount >= count;
      }
    }
    return true;
  }

  void releaseUpdate() {
    std::lock_guard<std::mutex> autoLock(locker);
    updateReleased = true;
    condition.notify_all();
  }

  std::thread::id willUpdateThread() {
    std::lock_guard<std::mutex> autoLock(locker);
    return willThread;
  }

  std::thread::id updateThread() {
    std::lock_guard<std::mutex> autoLock(locker);
    return currentUpdateThread;
  }

 protected:
  void onAnimationWillUpdate(PAGAnimator*) override {
    std::lock_guard<std::mutex> autoLock(locker);
    willThread = std::this_thread::get_id();
  }

  void onAnimationUpdate(PAGAnimator*) override {
    std::unique_lock<std::mutex> lock(locker);
    currentUpdateThread = std::this_thread::get_id();
    updateCount++;
    condition.notify_all();
    while (!updateReleased) {
      condition.wait(lock);
    }
  }

 private:
  std::mutex locker = {};
  std::condition_variable condition = {};
  std::thread::id willThread = {};
  std::thread::id currentUpdateThread = {};
  size_t updateCount = 0;
  bool updateReleased = false;
};

class CancelOnUpdateListener : public PAGAnimator::Listener {
 public:
  bool waitForUpdateStart(uint64_t timeout) {
    std::unique_lock<std::mutex> lock(locker);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
    while (!updateStarted) {
      if (condition.wait_until(lock, deadline) == std::cv_status::timeout) {
        return updateStarted;
      }
    }
    return true;
  }

  bool waitForCancelReturn(uint64_t timeout) {
    std::unique_lock<std::mutex> lock(locker);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
    while (!cancelReturned) {
      if (condition.wait_until(lock, deadline) == std::cv_status::timeout) {
        return cancelReturned;
      }
    }
    return true;
  }

 protected:
  void onAnimationUpdate(PAGAnimator* animator) override {
    {
      std::lock_guard<std::mutex> autoLock(locker);
      updateStarted = true;
      condition.notify_all();
    }
    animator->cancel();
    std::lock_guard<std::mutex> autoLock(locker);
    cancelReturned = true;
    condition.notify_all();
  }

 private:
  std::mutex locker = {};
  std::condition_variable condition = {};
  bool updateStarted = false;
  bool cancelReturned = false;
};

enum class AnimationEvent { Update, End };

class EndUpdateListener : public PAGAnimator::Listener {
 public:
  explicit EndUpdateListener(bool cancelDeferredUpdate)
      : cancelDeferredUpdate(cancelDeferredUpdate) {
  }

  bool waitForEventCount(size_t count, uint64_t timeout) {
    std::unique_lock<std::mutex> lock(locker);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
    while (events.size() < count) {
      if (condition.wait_until(lock, deadline) == std::cv_status::timeout) {
        return events.size() >= count;
      }
    }
    return true;
  }

  std::vector<AnimationEvent> getEvents() {
    std::lock_guard<std::mutex> autoLock(locker);
    return events;
  }

 protected:
  void onAnimationUpdate(PAGAnimator* animator) override {
    bool requestUpdate = false;
    {
      std::lock_guard<std::mutex> autoLock(locker);
      events.push_back(AnimationEvent::Update);
      if (!deferredUpdateRequested) {
        deferredUpdateRequested = true;
        requestUpdate = true;
      }
      condition.notify_all();
    }
    if (requestUpdate) {
      animator->updateAsync();
      if (cancelDeferredUpdate) {
        animator->cancel();
      }
    }
  }

  void onAnimationEnd(PAGAnimator*) override {
    std::lock_guard<std::mutex> autoLock(locker);
    events.push_back(AnimationEvent::End);
    condition.notify_all();
  }

 private:
  std::mutex locker = {};
  std::condition_variable condition = {};
  std::vector<AnimationEvent> events = {};
  bool cancelDeferredUpdate = false;
  bool deferredUpdateRequested = false;
};

class AnimatorCall {
 public:
  AnimatorCall(std::shared_ptr<PAGAnimator> animator, bool update)
      : animator(std::move(animator)), update(update) {
  }

  void run() {
    {
      std::lock_guard<std::mutex> autoLock(locker);
      threadID = std::this_thread::get_id();
      callStarted = true;
      condition.notify_all();
    }
    if (update) {
      animator->updateAsync();
    } else {
      animator->cancel();
    }
    std::lock_guard<std::mutex> autoLock(locker);
    returned = true;
    condition.notify_all();
  }

  bool waitForCallStart(uint64_t timeout) {
    std::unique_lock<std::mutex> lock(locker);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
    while (!callStarted) {
      if (condition.wait_until(lock, deadline) == std::cv_status::timeout) {
        return callStarted;
      }
    }
    return true;
  }

  bool waitForReturn(uint64_t timeout) {
    std::unique_lock<std::mutex> lock(locker);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
    while (!returned) {
      if (condition.wait_until(lock, deadline) == std::cv_status::timeout) {
        return returned;
      }
    }
    return true;
  }

  std::thread::id getThreadID() {
    std::lock_guard<std::mutex> autoLock(locker);
    return threadID;
  }

 private:
  std::shared_ptr<PAGAnimator> animator = nullptr;
  std::mutex locker = {};
  std::condition_variable condition = {};
  std::thread::id threadID = {};
  bool update = false;
  bool callStarted = false;
  bool returned = false;
};

static void RunAnimatorCall(AnimatorCall* call) {
  call->run();
}

static std::shared_ptr<PAGAnimator> MakeAnimator(
    const std::shared_ptr<PAGAnimator::Listener>& listener) {
  auto animator = std::shared_ptr<PAGAnimator>(new PAGAnimator(listener));
  animator->weakThis = animator;
  return animator;
}

PAG_TEST(PAGAnimatorTest, StoppedAsyncUpdateIsTrackedUntilCanceled) {
  auto listener = std::make_shared<AnimatorListener>();
  auto animator = MakeAnimator(listener);
  AnimatorCall updateCall(animator, true);
  std::thread updateThread(RunAnimatorCall, &updateCall);

  auto updateReturned = updateCall.waitForReturn(1000);
  if (!updateReturned) {
    listener->releaseUpdate();
  }
  updateThread.join();
  ASSERT_TRUE(updateReturned);
  auto updateStarted = listener->waitForUpdateCount(1, 1000);
  if (!updateStarted) {
    listener->releaseUpdate();
    animator->cancel();
  }
  ASSERT_TRUE(updateStarted);
  EXPECT_NE(listener->updateThread(), updateCall.getThreadID());
  EXPECT_EQ(listener->willUpdateThread(), listener->updateThread());

  AnimatorCall cancelCall(animator, false);
  std::thread cancelThread(RunAnimatorCall, &cancelCall);
  auto cancelStarted = cancelCall.waitForCallStart(1000);
  if (!cancelStarted) {
    listener->releaseUpdate();
    cancelThread.join();
  }
  ASSERT_TRUE(cancelStarted);
  EXPECT_FALSE(cancelCall.waitForReturn(20));
  listener->releaseUpdate();
  EXPECT_TRUE(cancelCall.waitForReturn(1000));
  cancelThread.join();
  EXPECT_EQ(animator->task, nullptr);
}

PAG_TEST(PAGAnimatorTest, StoppedAsyncUpdateProcessesLatestRequest) {
  auto listener = std::make_shared<AnimatorListener>();
  auto animator = MakeAnimator(listener);

  animator->updateAsync();
  auto updateStarted = listener->waitForUpdateCount(1, 1000);
  if (!updateStarted) {
    listener->releaseUpdate();
    animator->cancel();
  }
  ASSERT_TRUE(updateStarted);
  animator->updateAsync();
  listener->releaseUpdate();
  EXPECT_TRUE(listener->waitForUpdateCount(2, 1000));
  animator->cancel();
  EXPECT_EQ(animator->task, nullptr);
}

PAG_TEST(PAGAnimatorTest, AsyncUpdateAllowsReentrantCancel) {
  auto listener = std::make_shared<CancelOnUpdateListener>();
  auto animator = MakeAnimator(listener);

  animator->updateAsync();
  auto updateStarted = listener->waitForUpdateStart(1000);
  if (!updateStarted) {
    animator->cancel();
  }
  ASSERT_TRUE(updateStarted);
  auto cancelReturned = listener->waitForCancelReturn(100);
  if (!cancelReturned) {
    EXPECT_TRUE(listener->waitForCancelReturn(1000));
  }
  EXPECT_TRUE(cancelReturned);
  animator->cancel();
  EXPECT_EQ(animator->task, nullptr);
}

PAG_TEST(PAGAnimatorTest, FinalUpdatePrecedesDeferredUpdate) {
  auto listener = std::make_shared<EndUpdateListener>(false);
  auto animator = MakeAnimator(listener);
  animator->_duration = 1;
  animator->_progress = 1.0;
  animator->_isRunning = true;

  animator->advance();

  auto deferredUpdateCompleted = listener->waitForEventCount(3, 1000);
  if (!deferredUpdateCompleted) {
    animator->cancel();
  }
  ASSERT_TRUE(deferredUpdateCompleted);
  animator->cancel();
  auto events = listener->getEvents();
  ASSERT_EQ(events.size(), 3u);
  EXPECT_EQ(events[0], AnimationEvent::Update);
  EXPECT_EQ(events[1], AnimationEvent::End);
  EXPECT_EQ(events[2], AnimationEvent::Update);
  EXPECT_FALSE(animator->isEnding);
}

PAG_TEST(PAGAnimatorTest, CancelDropsUpdateDeferredDuringEnd) {
  auto listener = std::make_shared<EndUpdateListener>(true);
  auto animator = MakeAnimator(listener);
  animator->_duration = 1;
  animator->_progress = 1.0;
  animator->_isRunning = true;

  animator->advance();

  auto events = listener->getEvents();
  ASSERT_EQ(events.size(), 2u);
  EXPECT_EQ(events[0], AnimationEvent::Update);
  EXPECT_EQ(events[1], AnimationEvent::End);
  EXPECT_FALSE(animator->isEnding);
  EXPECT_FALSE(animator->asyncUpdateRequested);
  EXPECT_EQ(animator->task, nullptr);
}

}  // namespace pag
