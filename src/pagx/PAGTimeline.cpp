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
#include <cmath>
#include "pagx/animation/Animation.h"

namespace pagx {

PAGTimeline::PAGTimeline(std::weak_ptr<PAGFile> file, Animation* anim)
    : ownerFile(std::move(file)), animation(anim) {
}

const std::string& PAGTimeline::getName() const {
  static const std::string kEmpty;
  return animation != nullptr ? animation->name : kEmpty;
}

Frame PAGTimeline::getDuration() const {
  return animation != nullptr ? animation->duration : 0;
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
  currentFrame = ZeroFrame;
}

bool PAGTimeline::isPlaying() const {
  return playing;
}

void PAGTimeline::setTime(float seconds) {
  auto rate = getFrameRate();
  if (rate <= 0.0f) {
    currentFrame = ZeroFrame;
    return;
  }
  auto frame = static_cast<Frame>(std::llround(static_cast<double>(seconds) * rate));
  currentFrame = std::max<Frame>(0, frame);
}

float PAGTimeline::getTime() const {
  auto rate = getFrameRate();
  if (rate <= 0.0f) {
    return 0.0f;
  }
  return static_cast<float>(static_cast<double>(currentFrame) / rate);
}

bool PAGTimeline::advance(float dt) {
  if (!playing || dt == 0.0f || animation == nullptr) {
    return false;
  }
  auto rate = animation->frameRate;
  auto duration = animation->duration;
  if (rate <= 0.0f || duration <= 0) {
    return false;
  }
  auto previous = currentFrame;
  auto delta = static_cast<Frame>(std::llround(static_cast<double>(dt) * rate));
  if (delta == 0) {
    return false;
  }
  auto next = currentFrame + delta;
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
  currentFrame = next;
  return currentFrame != previous;
}

void PAGTimeline::apply(float /*mix*/) {
  // TODO(PR4/PR8): evaluate properties at currentFrame and write through ChannelRegistry.
}

bool PAGTimeline::advanceAndApply(float dt, float mix) {
  bool changed = advance(dt);
  apply(mix);
  return changed;
}

}  // namespace pagx
