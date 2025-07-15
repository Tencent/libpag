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

#pragma once

#include "pag/pag.h"
#include "tgfx/core/Task.h"

namespace pag {
/**
 * PAGAnimator provides a simple timing engine for running animations.
 */
class PAGAnimator {
 public:
  /**
   * This interface is used to receive notifications when the status of the associated PAGAnimator
   * changes, and to flush the animation frame.
   */
  class Listener {
   public:
    virtual ~Listener() = default;

   protected:
    /**
     * Notifies the beginning of the animation. This may be called from any thread that invokes the
     * start() method.
     */
    virtual void onAnimationStart(PAGAnimator*) {
    }

    /**
     * Notifies the end of the animation. This will only be called from the UI thread.
     */
    virtual void onAnimationEnd(PAGAnimator*) {
    }

    /**
     * Notifies the cancellation of the animation. This may be called from any thread that invokes
     * the cancel() method.
     */
    virtual void onAnimationCancel(PAGAnimator*) {
    }

    /**
     * Notifies the repetition of the animation. This will only be called from the UI thread.
     */
    virtual void onAnimationRepeat(PAGAnimator*) {
    }

    /**
     * Notifies another frame of the animation will occur. This will only be called from the UI
     * thread. Note: onAnimationWillUpdate and onAnimationUpdate will always appear in pairs.
     */
    virtual void onAnimationWillUpdate(PAGAnimator*) {
    }

    /**
     * Notifies another frame of the animation has occurred. This may be called from an arbitrary
     * thread if the animation is running asynchronously.
     */
    virtual void onAnimationUpdate(PAGAnimator* animator) = 0;

    friend class PAGAnimator;
  };

  /**
   * Creates a new PAGAnimator with the specified listener.
   */
  static std::shared_ptr<PAGAnimator> MakeFrom(std::weak_ptr<Listener> listener);

  /**
   * Indicates whether the animation is allowed to run in the UI thread. The default value is false.
   */
  bool isSync();

  /**
   * Set whether the animation is allowed to run in the UI thread.
   */
  void setSync(bool value);

  /**
   * Returns the length of the animation in microseconds.
   */
  int64_t duration();

  /**
   * Sets the length of the animation in microseconds.
   */
  void setDuration(int64_t duration);

  /**
   * The total number of times the animation is set to play. The default is 1, which means the
   * animation will play only once. If the repeat count is set to 0 or a negative value, the
   * animation will play infinity times.
   */
  int repeatCount();

  /**
   * Set the number of times the animation to play.
   */
  void setRepeatCount(int repeatCount);

  /**
   * Returns the current position of the animation, which is a number between 0.0 and 1.0.
   */
  double progress();

  /**
   * Set the current progress of the animation.
   */
  void setProgress(double value);

  /**
   * Indicates whether the animation is running.
   */
  bool isRunning();

  /**
   * Starts the animation from the current position. Calling the start() method when the animation
   * is already started has no effect. The start() method does not alter the animation's current
   * position. However, if the animation previously reached its end, it will restart from the
   * beginning.
   */
  void start();

  /**
   * Cancels the animation at the current position. Calling the start() method can resume the
   * animation from the last canceled position.
   */
  void cancel();

  /**
   * Manually update the animation to the current progress without altering its playing status. If
   * isSync is set to false, the calling thread won't be blocked. Please note that if the animation
   * already has an ongoing asynchronous flushing task, this action won't have any effect.
   */
  void update();

 private:
  std::mutex locker = {};
  std::weak_ptr<PAGAnimator> weakThis;
  std::weak_ptr<Listener> weakListener;
  std::shared_ptr<tgfx::Task> task = nullptr;
  int64_t _startTime = INT64_MIN;
  int64_t _duration = 0;
  int _repeatCount = 1;
  double _progress = 0;
  bool _isSync = false;
  bool _isRunning = false;
  bool isAnimating = false;
  bool isEnded = false;
  int playedCount = 0;

  explicit PAGAnimator(std::weak_ptr<Listener> listener);
  void advance();
  std::vector<int> doAdvance();
  void doUpdate(bool setStartTime);
  void onFlush(bool setStartTime);
  void startAnimation();
  void cancelAnimation();
  void resetStartTime();

  friend class AnimationTicker;
};
}  // namespace pag
