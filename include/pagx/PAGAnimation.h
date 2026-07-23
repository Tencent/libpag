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

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "pagx/PAGTimeline.h"
#include "pagx/nodes/Animation.h"

namespace pagx {

struct RuntimeBinding;
class PAGScene;
class PAGXDocument;
class Node;
class Channel;

/**
 * PAGAnimation controls the playback of a single animation in a PAGScene. It holds the current
 * playback time and applies the animation to the scene's content via apply(). Playback advancement
 * is gated by the owning PAGComposition's pausedTimelineIds, not by state on PAGAnimation itself.
 *
 * PAGAnimation must not be constructed directly; obtain instances through PAGScene::getAnimation().
 * Multiple lookups for the same animation name return the same PAGAnimation instance, so playback
 * state is shared across all callers driving that animation.
 *
 * Lifetime: a PAGAnimation references content owned by the PAGScene that vended it. Callers may
 * keep the returned shared_ptr alive past the PAGScene, but once that scene is destroyed the
 * timeline detaches: advance() and apply() become no-ops rather than dereferencing freed content.
 */
class PAGAnimation : public PAGTimeline {
 public:
  /**
   * Returns the concrete timeline kind.
   */
  TimelineType type() const override {
    return TimelineType::Animation;
  }

  /**
   * Returns the animation id. Returns an empty string once the owning PAGScene is destroyed.
   */
  const std::string& getId() const override;

  /**
   * Returns the animation duration in microseconds. Returns 0 once the owning PAGScene is
   * destroyed.
   */
  int64_t duration() const;

  /**
   * Returns the animation frame rate. Returns 0 once the owning PAGScene is destroyed.
   */
  float frameRate() const;

  /**
   * Sets the current playback time in microseconds. Only updates the time; call apply() to reflect
   * it in the content.
   */
  void setCurrentTime(int64_t microseconds);

  /**
   * Returns the current playback time in microseconds.
   */
  int64_t currentTime() const;

  /**
   * Advances the current time by deltaMicroseconds, respecting the loop mode. Does not change the
   * content; call apply() to reflect the new time.
   * @param deltaMicroseconds the elapsed time in microseconds. May be negative.
   * @return true if the current time changed, false when the owning PAGScene is destroyed,
   *         deltaMicroseconds is zero, the animation is missing, or its duration is zero.
   */
  bool advance(int64_t deltaMicroseconds) override;

  /**
   * Evaluates the animation at the current time and applies the results to the content, blended
   * with the existing values by mix.
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
  void apply(float mix = 1.0f) override;


 private:
  PAGAnimation(Animation* animation, RuntimeBinding* binding, PAGXDocument* contextDoc,
               std::weak_ptr<PAGScene> owner);

  // Resolves each animation object's target node against contextDoc into the (node, channels)
  // pairs consumed by apply(). Targets that resolve outside binding's scope (e.g. a document-level
  // animation pointing at a node living inside a nested composition's separate binding) are dropped
  // here, keeping the animation from reaching across a composition boundary. apply() reuses the
  // cached result and only re-runs this when targetsDirty is set.
  void resolveTargets(const RuntimeBinding* effectiveBinding);

  // Owning scene. animation / binding / contextDoc point into content this scene keeps alive, so
  // advance() and apply() bail out once the scene is gone to avoid dereferencing freed memory.
  std::weak_ptr<PAGScene> owner;
  Animation* animation = nullptr;
  // Runtime binding the channel writers should target. A null binding marks a top-level timeline:
  // the owning scene's current root binding is resolved lazily at apply time, so the timeline keeps
  // working after the scene rebuilds its runtime tree (which frees and replaces that binding). A
  // non-null binding is a fixed composition binding co-owned with that composition.
  RuntimeBinding* binding = nullptr;
  // Document used to resolve channel target IDs at apply time. Top-level timelines use the scene's
  // primary document; timelines spawned by external composition layers use the layer's externalDoc
  // so internal IDs of the external file stay self-contained.
  PAGXDocument* contextDoc = nullptr;
  // Scratch buffer holding the resolved (target node, channels) pairs. Rebuilt by resolveTargets()
  // only when targetsDirty is set, and reused across frames otherwise; its capacity is retained to
  // avoid per-frame reallocation.
  std::vector<std::pair<Node*, std::vector<Channel*>>> resolvedTargets = {};
  // Whether resolvedTargets must be rebuilt before the next apply(). Set on construction and when
  // an incremental tree refresh changes binding membership in place, which keeps this timeline and
  // its binding pointer so no other signal would trigger a re-resolve.
  bool targetsDirty = true;
  // Raw accumulated time in microseconds, not folded by the loop mode. Folding is deferred to
  // currentTime() and apply() so the content offset can be subtracted on the raw timeline. May be
  // negative; advance() accumulates, setCurrentTime() clamps to >= 0.
  int64_t elapsedUs = 0;
  // Content evaluation offset in microseconds. Set by PAGComposition from the AnimationTimeline
  // driver's startOffset (frames, converted via the animation's frameRate). Subtracted from raw
  // elapsedUs before loop folding in apply(). A positive offset delays content; a negative
  // offset skips ahead.
  int64_t evaluationOffsetUs = 0;

  friend class PAGScene;
  friend class PAGComposition;
  friend class PAGStateMachine;
};

}  // namespace pagx
