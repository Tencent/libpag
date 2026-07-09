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
#include <utility>
#include <vector>
#include "pagx/nodes/Animation.h"

namespace pagx {

struct RuntimeBinding;
class PAGScene;
class PAGXDocument;
class Node;
class Channel;

/**
 * PAGTimeline controls the playback of a single animation in a PAGScene. It holds the playback state
 * (current time and playing flag) and applies the animation to the scene's content via apply().
 *
 * PAGTimeline must not be constructed directly; obtain instances through PAGScene::getTimeline().
 * Multiple lookups for the same animation name return the same PAGTimeline instance, so playback
 * state is shared across all callers driving that animation.
 *
 * Lifetime: a PAGTimeline references content owned by the PAGScene that vended it. Callers may keep
 * the returned shared_ptr alive past the PAGScene, but once that scene is destroyed the timeline
 * detaches: advance() and apply() become no-ops rather than dereferencing freed content.
 *
 * Thread safety: PAGTimeline is not thread-safe. currentTime and the playing flag are plain
 * members with no synchronization, so calls on a given timeline (and on the PAGScene that owns it)
 * must be serialized by the caller.
 */
class PAGTimeline {
 public:
  ~PAGTimeline() = default;

  /**
   * Returns the animation id. Returns an empty string once the owning PAGScene is destroyed.
   */
  const std::string& getId() const;

  /**
   * Returns the animation duration in microseconds. Returns 0 once the owning PAGScene is destroyed.
   */
  int64_t duration() const;

  /**
   * Returns the animation frame rate. Returns 0 once the owning PAGScene is destroyed.
   */
  float frameRate() const;

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
   * @return true if the current time changed, false if the timeline is paused or
   *         deltaMicroseconds is zero.
   */
  bool advance(int64_t deltaMicroseconds);

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
  void apply(float mix = 1.0f);

  /**
   * Convenience method equivalent to advance(deltaMicroseconds) followed by apply(mix). Returns
   * the result of advance(deltaMicroseconds).
   */
  bool advanceAndApply(int64_t deltaMicroseconds, float mix = 1.0f);

 private:
  PAGTimeline(Animation* animation, RuntimeBinding* binding, PAGXDocument* contextDoc,
              std::weak_ptr<PAGScene> owner);

  // Resolves each animation object's target node against contextDoc once and caches the
  // (node, channels) pairs, so apply() avoids a per-frame findNode() hash lookup. Built lazily on
  // the first apply(). Stale caches are replaced by rebuilding the PAGTimeline (driven by
  // PAGXDocument::notifyChange when a timeline node is dirty); this cache is never reset in place.
  void resolveTargets();

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
  // Cached target resolution: each entry pairs a resolved target node with the channels driving
  // it. Populated by resolveTargets() on first apply(); never reset in place — stale caches are
  // replaced by rebuilding the PAGTimeline (see resolveTargets()).
  std::vector<std::pair<Node*, std::vector<Channel*>>> resolvedTargets = {};
  bool resolved = false;
  int64_t currentTimeUs = 0;
  bool playing = true;

  friend class PAGScene;
  friend class PAGComposition;
  friend class PAGStateMachineTimeline;
};

}  // namespace pagx
