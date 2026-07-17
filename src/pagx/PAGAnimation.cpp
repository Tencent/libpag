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

#include "pagx/PAGAnimation.h"
#include <algorithm>
#include <cstdint>
#include "base/utils/Log.h"
#include "pagx/PAGScene.h"
#include "pagx/PAGStateMachineRegion.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationObject.h"
#include "pagx/nodes/Channel.h"
#include "renderer/LayerBuilder.h"

namespace pagx {

// Returns the animation duration in microseconds. Returns 0 for invalid animations.
static int64_t DurationMicros(const Animation* animation) {
  if (animation == nullptr || animation->frameRate <= 0.0f || animation->duration <= 0) {
    return 0;
  }
  return FramesToUs(animation->duration, animation->frameRate);
}

// Evaluates the given animation at the given microsecond time and writes the results into the
// supplied runtime binding, using the pre-resolved (target node, channels) pairs to avoid a
// per-frame node lookup. Stateless: depends only on its arguments.
static void ApplyResolved(
    const std::vector<std::pair<Node*, std::vector<Channel*>>>& resolvedTargets,
    Animation* animation, RuntimeBinding* binding, int64_t microseconds, float mix) {
  if (animation == nullptr || binding == nullptr) {
    return;
  }
  for (const auto& entry : resolvedTargets) {
    auto* targetNode = entry.first;
    for (auto* channel : entry.second) {
      binding->apply(targetNode, channel->name,
                     channel->evaluateAt(microseconds, animation->frameRate), mix);
    }
  }
}

// Forces the "visible" property of every resolved target node, used to gate rendering by the
// animation's visibility window. mix is ignored: visible is a discrete bool property whose writer
// overwrites the current value.
static void ApplyVisibility(
    const std::vector<std::pair<Node*, std::vector<Channel*>>>& resolvedTargets,
    RuntimeBinding* binding, bool visible) {
  if (binding == nullptr) {
    return;
  }
  for (const auto& entry : resolvedTargets) {
    binding->apply(entry.first, "visible", KeyValue{visible}, 1.0f);
  }
}

PAGAnimation::PAGAnimation(Animation* anim, RuntimeBinding* binding, PAGXDocument* contextDoc,
                           std::weak_ptr<PAGScene> owner, int64_t compositionStartOffsetMicros)
    : owner(std::move(owner)), animation(anim), binding(binding), contextDoc(contextDoc),
      compositionStartOffsetMicros(compositionStartOffsetMicros) {
}

void PAGAnimation::resolveTargets(const RuntimeBinding* effectiveBinding) {
  resolvedTargets.clear();
  if (animation == nullptr || contextDoc == nullptr || effectiveBinding == nullptr) {
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
    // Drop targets outside this binding's scope. findNode does a flat document-wide lookup, so it
    // can resolve a node living inside a nested composition; that node is bound in the composition's
    // own binding, not here, so applying to it would cross the composition boundary.
    if (!effectiveBinding->contains(targetNode)) {
      continue;
    }
    std::vector<Channel*> channels = {};
    for (auto* ch : object->channels) {
      if (ch != nullptr) {
        channels.push_back(ch);
      }
    }
    if (!channels.empty()) {
      resolvedTargets.emplace_back(targetNode, std::move(channels));
    }
  }
}

const std::string& PAGAnimation::getId() const {
  static const std::string EMPTY;
  return (!owner.expired() && animation != nullptr) ? animation->id : EMPTY;
}

int64_t PAGAnimation::duration() const {
  return owner.expired() ? 0 : DurationMicros(animation);
}

float PAGAnimation::frameRate() const {
  return (!owner.expired() && animation != nullptr) ? animation->frameRate : 0.0f;
}

void PAGAnimation::setCurrentTime(int64_t microseconds) {
  currentTimeUs = std::max<int64_t>(0, microseconds);
}

int64_t PAGAnimation::currentTime() const {
  return currentTimeUs;
}

// Maps an absolute time to a valid playback position according to the loop mode. Once clamps to
// [0, duration]; Loop wraps modulo duration; PingPong mirrors over a period of 2 * duration. Pure:
// has no side effect on playback state.
static int64_t WrapTime(int64_t time, int64_t duration, LoopMode loop) {
  if (duration <= 0) {
    return time;
  }
  switch (loop) {
    case LoopMode::Once:
      return std::clamp<int64_t>(time, 0, duration);
    case LoopMode::Loop: {
      auto pos = time % duration;
      if (pos < 0) {
        pos += duration;
      }
      return pos;
    }
    case LoopMode::PingPong: {
      auto period = duration * 2;
      auto pos = time % period;
      if (pos < 0) {
        pos += period;
      }
      return pos < duration ? pos : period - pos;
    }
  }
  return time;
}

bool PAGAnimation::advance(int64_t deltaMicroseconds) {
  if (owner.expired() || deltaMicroseconds == 0 || animation == nullptr) {
    return false;
  }
  auto duration = DurationMicros(animation);
  if (duration <= 0) {
    return false;
  }
  auto previous = currentTimeUs;
  // Use saturating add to prevent signed overflow UB when deltaMicroseconds is extreme.
  int64_t next = 0;
  if (deltaMicroseconds > 0 && currentTimeUs > INT64_MAX - deltaMicroseconds) {
    next = INT64_MAX;
  } else if (deltaMicroseconds < 0 && currentTimeUs < INT64_MIN - deltaMicroseconds) {
    next = INT64_MIN;
  } else {
    next = currentTimeUs + deltaMicroseconds;
  }
  currentTimeUs = WrapTime(next, duration, animation->loop);
  return currentTimeUs != previous;
}

void PAGAnimation::apply(float mix) {
  if (owner.expired() || animation == nullptr) {
    return;
  }
  auto clamped = std::clamp(mix, 0.0f, 1.0f);
  if (clamped <= 0.0f) {
    return;
  }
  // A null binding marks a top-level timeline: resolve the owning scene's current root binding now,
  // so it tracks a runtime-tree rebuild that replaced the scene's binding. A non-null binding is a
  // fixed composition binding co-owned with that composition.
  RuntimeBinding* effectiveBinding = binding;
  if (effectiveBinding == nullptr) {
    auto scene = owner.lock();
    effectiveBinding = scene != nullptr ? scene->mutableBinding() : nullptr;
  }
  if (effectiveBinding == nullptr) {
    return;
  }
  // Re-resolve only when needed: targetsDirty covers an incremental refresh that changed binding
  // membership in place (same binding pointer, patched members). A runtime-tree rebuild recreates
  // PAGAnimation instances, so no separate pointer-based invalidation is needed.
  if (targetsDirty) {
    resolveTargets(effectiveBinding);
    targetsDirty = false;
  }
  auto evaluationTimeUs = std::max<int64_t>(0, currentTimeUs - compositionStartOffsetMicros);
  ApplyResolved(resolvedTargets, animation, effectiveBinding, evaluationTimeUs, clamped);
}

void PAGAnimation::applyWithVisibilityWindow(int64_t compositionElapsedUs, float mix) {
  // Visibility window gate (PAG Layer.startTime + duration semantics): outside [startOffset,
  // startOffset + duration) the driven targets are hidden. The window is measured on the owning
  // composition's monotonic elapsed clock, so it still closes after the animation has played its
  // full duration (unlike currentTimeUs, which loop mode wraps). A zero duration means "no window"
  // and applies unconditionally, so ordinary animations keep their default visibility.
  if (animation == nullptr || animation->duration <= 0) {
    apply(mix);
    return;
  }
  RuntimeBinding* effectiveBinding = binding;
  if (effectiveBinding == nullptr) {
    auto scene = owner.lock();
    effectiveBinding = scene != nullptr ? scene->mutableBinding() : nullptr;
  }
  if (effectiveBinding == nullptr) {
    return;
  }
  if (!resolved) {
    resolveTargets();
  }
  auto startUs = FramesToUs(animation->startOffset, animation->frameRate);
  auto endUs = FramesToUs(animation->startOffset + animation->duration, animation->frameRate);
  if (compositionElapsedUs < startUs || compositionElapsedUs >= endUs) {
    ApplyVisibility(resolvedTargets, effectiveBinding, false);
    return;
  }
  ApplyVisibility(resolvedTargets, effectiveBinding, true);
  apply(mix);
}

}  // namespace pagx
