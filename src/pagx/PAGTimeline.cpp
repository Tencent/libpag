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

#include "pagx/PAGTimeline.h"
#include <algorithm>
#include "pagx/PAGFile.h"
#include "pagx/nodes/Animation.h"

namespace pagx {

static constexpr int64_t kMicrosPerSecond = 1'000'000;

// Returns the animation duration in microseconds. Returns 0 for invalid animations.
static int64_t DurationMicros(const Animation* animation) {
  if (animation == nullptr || animation->frameRate <= 0.0f || animation->duration <= 0) {
    return 0;
  }
  return static_cast<int64_t>(static_cast<double>(animation->duration) *
                              static_cast<double>(kMicrosPerSecond) /
                              static_cast<double>(animation->frameRate));
}

PAGTimeline::PAGTimeline(std::weak_ptr<PAGFile> file, Animation* anim, PAGLayerTree* layerTree,
                         PAGXDocument* contextDoc)
    : ownerFile(std::move(file)),
      animation(anim),
      layerTree(layerTree),
      contextDoc(contextDoc) {
}

const std::string& PAGTimeline::getId() const {
  static const std::string kEmpty;
  return animation != nullptr ? animation->id : kEmpty;
}

int64_t PAGTimeline::getDuration() const {
  return DurationMicros(animation);
}

float PAGTimeline::getFrameRate() const {
  return animation != nullptr ? animation->frameRate : 60.0f;
}

void PAGTimeline::play() {
  playing = true;
}

void PAGTimeline::pause() {
  playing = false;
}

void PAGTimeline::stop() {
  playing = false;
  currentTimeUs = 0;
}

bool PAGTimeline::isPlaying() const {
  return playing;
}

void PAGTimeline::setCurrentTime(int64_t microseconds) {
  currentTimeUs = std::max<int64_t>(0, microseconds);
}

int64_t PAGTimeline::currentTime() const {
  return currentTimeUs;
}

bool PAGTimeline::advance(int64_t deltaMicroseconds) {
  if (!playing || deltaMicroseconds == 0 || animation == nullptr) {
    return false;
  }
  auto duration = DurationMicros(animation);
  if (duration <= 0) {
    return false;
  }
  auto previous = currentTimeUs;
  auto next = currentTimeUs + deltaMicroseconds;
  switch (animation->loop) {
    case LoopMode::Once:
      if (next >= duration) {
        next = duration;
        playing = false;
      } else if (next < 0) {
        next = 0;
        playing = false;
      }
      break;
    case LoopMode::Loop: {
      next = next % duration;
      if (next < 0) {
        next += duration;
      }
      break;
    }
    case LoopMode::PingPong: {
      auto period = duration * 2;
      auto pos = next % period;
      if (pos < 0) {
        pos += period;
      }
      next = pos < duration ? pos : period - pos;
      break;
    }
  }
  currentTimeUs = next;
  return currentTimeUs != previous;
}

void PAGTimeline::apply(float mix) {
  if (animation == nullptr) {
    return;
  }
  auto file = ownerFile.lock();
  if (file == nullptr) {
    return;
  }
  auto clamped = std::clamp(mix, 0.0f, 1.0f);
  if (clamped <= 0.0f) {
    return;
  }
  file->applyAnimation(animation, layerTree, contextDoc, currentTimeUs, clamped);
}

bool PAGTimeline::advanceAndApply(int64_t deltaMicroseconds, float mix) {
  bool changed = advance(deltaMicroseconds);
  apply(mix);
  return changed;
}

}  // namespace pagx
