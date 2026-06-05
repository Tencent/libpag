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
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationObject.h"
#include "pagx/nodes/Property.h"
#include "renderer/LayerBuilder.h"

namespace pagx {

static constexpr int64_t MICROS_PER_SECOND = 1'000'000;

// Returns the animation duration in microseconds. Returns 0 for invalid animations.
static int64_t DurationMicros(const Animation* animation) {
  if (animation == nullptr || animation->frameRate <= 0.0f || animation->duration <= 0) {
    return 0;
  }
  return static_cast<int64_t>(static_cast<double>(animation->duration) *
                              static_cast<double>(MICROS_PER_SECOND) /
                              static_cast<double>(animation->frameRate));
}

// Evaluates the given animation at the given microsecond time and writes the results into the
// supplied runtime binding, using the pre-resolved (target node, properties) pairs to avoid a
// per-frame node lookup. Stateless: depends only on its arguments.
static void ApplyResolved(
    const std::vector<std::pair<Node*, std::vector<Property*>>>& resolvedTargets,
    Animation* animation, RuntimeBinding* binding, int64_t microseconds, float mix) {
  if (animation == nullptr || binding == nullptr) {
    return;
  }
  for (const auto& entry : resolvedTargets) {
    auto* targetNode = entry.first;
    for (auto* property : entry.second) {
      binding->apply(targetNode, property->channel,
                     property->evaluateAt(microseconds, animation->frameRate), mix);
    }
  }
}

PAGTimeline::PAGTimeline(Animation* anim, RuntimeBinding* binding, PAGXDocument* contextDoc,
                         std::weak_ptr<PAGScene> owner)
    : owner(std::move(owner)), animation(anim), binding(binding), contextDoc(contextDoc) {
}

void PAGTimeline::resolveTargets() {
  resolved = true;
  if (animation == nullptr || contextDoc == nullptr) {
    return;
  }
  for (auto* object : animation->objects) {
    if (object == nullptr) {
      continue;
    }
    auto* targetNode = contextDoc->findNode(object->target);
    if (targetNode == nullptr) {
      continue;
    }
    std::vector<Property*> properties = {};
    for (auto* property : object->properties) {
      if (property != nullptr) {
        properties.push_back(property);
      }
    }
    if (!properties.empty()) {
      resolvedTargets.emplace_back(targetNode, std::move(properties));
    }
  }
}

const std::string& PAGTimeline::getId() const {
  static const std::string EMPTY;
  return (!owner.expired() && animation != nullptr) ? animation->id : EMPTY;
}

int64_t PAGTimeline::duration() const {
  return owner.expired() ? 0 : DurationMicros(animation);
}

float PAGTimeline::frameRate() const {
  return (!owner.expired() && animation != nullptr) ? animation->frameRate : 0.0f;
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
  if (owner.expired() || !playing || deltaMicroseconds == 0 || animation == nullptr) {
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
  if (owner.expired() || animation == nullptr) {
    return;
  }
  auto clamped = std::clamp(mix, 0.0f, 1.0f);
  if (clamped <= 0.0f) {
    return;
  }
  if (!resolved) {
    resolveTargets();
  }
  ApplyResolved(resolvedTargets, animation, binding, currentTimeUs, clamped);
}

bool PAGTimeline::advanceAndApply(int64_t deltaMicroseconds, float mix) {
  bool changed = advance(deltaMicroseconds);
  apply(mix);
  return changed;
}

}  // namespace pagx
