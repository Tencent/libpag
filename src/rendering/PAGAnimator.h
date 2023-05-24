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

#pragma once

#include "pag/pag.h"
#include "tgfx/utils/Task.h"

namespace pag {
/**
 * PAGAnimator provides a simple timing engine for running animations.
 */
class PAGAnimator {
 public:
  /**
   * This interface is used to update the animation.
   */
  class Updater {
   public:
    virtual ~Updater() = default;

   protected:
    /**
     * Notifies a new frame of the animation needs to be updated. It may be called from an arbitrary
     * thread if the animation is running asynchronously.
     */
    virtual void onUpdate(double progress) = 0;

    friend class PAGAnimator;
  };

  /**
   * This listener can be used to be notified when the status of the associated PAGAnimator is
   * changed. These methods are optional, you can implement only the events you care about.
   */
  class Listener {
   public:
    virtual ~Listener() = default;

   protected:
    /**
     * Notifies the beginning of the animation.
     */
    virtual void onAnimationStart(PAGAnimator*) {
    }

    /**
     * Notifies the end of the animation.
     */
    virtual void onAnimationEnd(PAGAnimator*) {
    }

    /**
     * Notifies the cancellation of the animation.
     */
    virtual void onAnimationCancel(PAGAnimator*) {
    }

    /**
     * Notifies the repetition of the animation.
     */
    virtual void onAnimationRepeat(PAGAnimator*) {
    }

    /**
     * Notifies the frame updating of the animation.
     */
    virtual void onAnimationUpdate(PAGAnimator*) {
    }

    friend class PAGAnimator;
  };

  /**
   * Creates a new PAGAnimator with the specified updater and listener. The caller must ensure that
   * the updater and listener will outlive the lifetime of the returned PAGAnimator.
   */
  static std::shared_ptr<PAGAnimator> MakeFrom(Updater* updater);

  /**
   * Adds a listener to the set of listeners that are sent events through the life of an animation,
   * such as start, repeat, and end.
   */
  void addListener(std::shared_ptr<Listener> listener);

  /**
   * Removes a listener from the set listening to this animation.
   */
  void removeListener(std::shared_ptr<Listener> listener);

  /**
   * Indicates whether the animation is allowed to run in the UI thread. The default value is false.
   * Regardless of whether the animation runs asynchronously, all listener callbacks will be called
   * on the UI thread.
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
   * Indicates whether the animation is playing.
   */
  bool isPlaying();

  /**
   * Starts to play the animation from the current position. Calling the play() method when the
   * animation is already playing has no effect. The play() method does not alter the animation's
   * current position. However, if the animation previously reached its end, it will restart from
   * the beginning.
   */
  void play();

  /**
   * Pauses the animation at the current position. Calling the play() method can resume the
   * animation from the last paused playing position.
   */
  void pause();

  /**
   * Stops the animation at the current position. Unlike pause(), stop() not only pauses the
   * animation but also resets the number of times the animation has played to 0. This method may
   * block the calling thread if there is an asynchronous task running.
   */
  void stop();

  /**
   * Manually update the animation to the current progress without altering its playing status. If
   * the animation is currently running an asynchronous updating task, this action will not have any
   * effect.
   */
  void update();

 private:
  std::mutex locker = {};
  std::weak_ptr<PAGAnimator> weakThis;
  Updater* updater = nullptr;
  std::vector<std::shared_ptr<Listener>> listeners = {};
  std::shared_ptr<tgfx::Task> task = nullptr;
  std::atomic_int64_t _startTime = INT64_MIN;
  int64_t _duration = 0;
  int _repeatCount = 1;
  double _progress = 0;
  bool _isSync = false;
  bool _isPlaying = false;
  bool isAnimating = false;
  bool isEnded = false;
  int playedCount = 0;

  explicit PAGAnimator(Updater* updater);
  void advance();
  std::vector<int> doAdvance();
  void updateProgress(int64_t playTime);
  void onUpdate(double progress, int64_t playTime = INT64_MIN);
  void startAnimation();
  void cancelAnimation();
  void resetStartTime();
  void resetTask();
  int findListener(std::shared_ptr<Listener> listener);
  void notifyListeners(std::vector<int> types);

  friend class AnimationTicker;
};
}  // namespace pag
