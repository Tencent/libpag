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

#pragma once

#include <memory>
#include <string>
#include "pagx/animation/Animation.h"

namespace pagx {

class PAGFile;

/**
 * PAGTimeline drives a single Animation inside a PAGFile. It owns the playback state (current
 * frame, playing flag) and writes the evaluated property values back to the runtime tgfx tree of
 * its owning PAGFile via apply().
 *
 * PAGTimeline must not be constructed directly; obtain instances through PAGFile::getTimeline().
 * Multiple lookups for the same animation name return the same PAGTimeline instance, so playback
 * state is shared across all callers driving that animation.
 */
class PAGTimeline {
 public:
  ~PAGTimeline() = default;

  /**
   * Returns the animation name. Equal to the corresponding Animation::name in the source document.
   */
  const std::string& getName() const;

  /**
   * Returns the animation duration in microseconds, derived from Animation::duration (in frames)
   * and Animation::frameRate.
   */
  int64_t getDuration() const;

  /**
   * Returns the animation frame rate. Equal to Animation::frameRate.
   */
  float getFrameRate() const;

  /**
   * Starts or resumes playback. Subsequent advance() calls will advance the current time.
   */
  void play();

  /**
   * Pauses playback. The current time is preserved; subsequent advance() calls become no-ops
   * until play() is called again.
   */
  void pause();

  /**
   * Stops playback and resets the current time to zero.
   */
  void stop();

  /**
   * Returns true if the timeline is currently playing.
   */
  bool isPlaying() const;

  /**
   * Sets the current playback time in microseconds. Only updates the internal time; callers must
   * invoke apply() to write the resulting values into the runtime tgfx tree.
   */
  void setCurrentTime(int64_t microseconds);

  /**
   * Returns the current playback time in microseconds.
   */
  int64_t currentTime() const;

  /**
   * Advances the current time by deltaMicroseconds, respecting the loop mode. Does not write any
   * values to the runtime tgfx tree.
   * @param deltaMicroseconds the elapsed time in microseconds. May be negative.
   * @return true if the current time changed, false if the timeline is paused or
   *         deltaMicroseconds is zero.
   */
  bool advance(int64_t deltaMicroseconds);

  /**
   * Evaluates all properties at the current time and writes the results into the runtime tgfx
   * tree of the owning PAGFile, blended with the existing values by mix.
   *
   * Mixing rules:
   *   - Continuous channels (float / Color): result = lerp(current, evaluated, mix). Color channels
   *     blend per RGBA component.
   *   - Discrete channels (bool / int / string / ImageRef): mix is ignored; the evaluated value
   *     overwrites the current value when mix > 0.
   *
   * mix is clamped to [0, 1]. mix == 0 returns immediately without reading or writing.
   * Multiple timelines targeting the same channel are stacked in the order apply() is called; the
   * later writer mixes against the previous writer's result.
   * @param mix blend weight, defaults to 1.0 (full overwrite).
   */
  void apply(float mix = 1.0f);

  /**
   * Convenience method equivalent to advance(deltaMicroseconds) followed by apply(mix). Returns
   * the result of advance(deltaMicroseconds).
   */
  bool advanceAndApply(int64_t deltaMicroseconds, float mix = 1.0f);

 private:
  PAGTimeline(std::weak_ptr<PAGFile> file, Animation* animation);

  std::weak_ptr<PAGFile> ownerFile = {};
  Animation* animation = nullptr;
  int64_t currentTimeUs = 0;
  bool playing = false;

  friend class PAGFile;
};

}  // namespace pagx
