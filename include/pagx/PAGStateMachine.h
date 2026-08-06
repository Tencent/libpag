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

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "pagx/ObserverHandle.h"
#include "pagx/PAGTimeline.h"
#include "pagx/nodes/StateMachine.h"
#include "pagx/nodes/StateMachineInput.h"

namespace pagx {

struct RuntimeBinding;
class PAGScene;
class PAGXDocument;
class PAGAnimation;
class State;
class StateTransition;

/**
 * PAGStateMachine drives a single StateMachine. It holds the runtime input values and a per-region
 * execution instance (RegionInstance, hidden in the .cpp) that tracks the current state, crossfade
 * progress, and per-region trigger consumption.
 *
 * PAGStateMachine must not be constructed directly; obtain instances through
 * PAGScene::getStateMachineTimeline(). Multiple lookups for the same state machine id return the
 * same PAGStateMachine instance.
 *
 * Lifetime: references content owned by the PAGScene that vended it. Callers may keep the returned
 * shared_ptr alive past the PAGScene, but once that scene is destroyed the timeline detaches:
 * advance() and apply() become no-ops.
 *
 * Thread safety: PAGStateMachine is not thread-safe. Callers must serialize access.
 */
class PAGStateMachine : public PAGTimeline,
                        public Observable,
                        public std::enable_shared_from_this<PAGStateMachine> {
 public:
  ~PAGStateMachine();

  /**
   * Returns the concrete timeline kind.
   */
  TimelineType type() const override {
    return TimelineType::StateMachine;
  }

  /**
   * Returns the state machine id. Returns an empty string once the owning PAGScene is destroyed.
   */
  const std::string& getId() const override;

  /**
   * Returns the region names in declaration order. Returns an empty vector once the owning
   * PAGScene is destroyed.
   */
  std::vector<std::string> getRegionIds() const;

  /**
   * Returns the name of the current state in the given region. Returns an empty string if the
   * region name is not found or the owning scene has been destroyed.
   * @param regionName the name of the region to query, as returned by getRegionIds().
   */
  std::string getCurrentState(const std::string& regionName) const;

  /**
   * Sets a bool input. The state machine will evaluate conditions referencing this input on the
   * next advance().
   * @param name the name of the SM input declared in <Inputs>.
   * @param value the bool value to set.
   * @return false if name is not found or the input type is not Bool; true otherwise.
   */
  bool setBool(const std::string& name, bool value);

  /**
   * Sets a number input. The state machine will evaluate conditions referencing this input on the
   * next advance().
   * @param name the name of the SM input declared in <Inputs>.
   * @param value the number value to set.
   * @return false if name is not found or the input type is not Number; true otherwise.
   */
  bool setNumber(const std::string& name, float value);

  /**
   * Fires a trigger input. Sets the fired flag; the state machine will evaluate trigger conditions
   * on the next advance(). Fire is idempotent: firing the same trigger multiple times before the
   * next advance() has the same effect as firing it once.
   * @param name the name of the SM input declared in <Inputs>.
   * @return false if name is not found or the input type is not Trigger; true otherwise.
   */
  bool fireTrigger(const std::string& name);

  /**
   * Advances all regions by deltaMicroseconds: advances elapsed time for active animations,
   * advances crossfade progress, and evaluates state transitions (including chained transitions
   * within the same frame, capped at 100 iterations).
   * @param deltaMicroseconds the elapsed time in microseconds.
   * @return true if any region's playback state changed this frame: an active or fading animation
   *         advanced its time, a state transition occurred, or the crossfade mix moved.
   */
  bool advance(int64_t deltaMicroseconds) override;

  /**
   * Evaluates the current state's animation output for every region and applies the results to the
   * content via the runtime binding. During a crossfade, both the outgoing and incoming states are
   * applied with their respective mix weights.
   * @param mix blend weight, defaults to 1.0 (full overwrite).
   */
  void apply(float mix = 1.0f) override;

  /**
   * Registers a state-change listener. The callback receives the region name and the new state
   * name whenever a region transitions. The returned ObserverHandle automatically unregisters the
   * listener when destroyed; call detach() on the handle to unregister early.
   *
   * Reentrant add/remove is safe: listeners are snapshotted before dispatch, so a callback that
   * adds or removes listeners during dispatch does not invalidate the ongoing iteration.
   * @param callback invoked on each region transition with the region name and the new state name.
   * @return an ObserverHandle whose destruction unregisters the listener. Returns an empty handle
   *         if callback is null.
   */
  ObserverHandle addStateChangeListener(
      std::function<void(const std::string& regionName, const std::string& newState)> callback);

  /**
   * InputValue holds the live runtime value of a single StateMachine input. It is an implementation
   * detail exposed publicly so that non-member helper functions in the .cpp can access it; callers
   * should not construct or inspect instances directly.
   */
  struct InputValue {
    /**
     * The value kind, copied from the referenced StateMachineInput at construction.
     */
    StateMachineInputType type = StateMachineInputType::Bool;
    /**
     * The current bool value when type is Bool. Ignored for other types.
     */
    bool boolValue = false;
    /**
     * The current number value when type is Number. Ignored for other types.
     */
    float numberValue = 0.0f;
    /**
     * The fired flag when type is Trigger. Set by fireTrigger(), cleared at the end of each
     * advance(). Ignored for other types.
     */
    bool fired = false;
  };

 private:
  explicit PAGStateMachine(StateMachine* stateMachine, RuntimeBinding* binding,
                           PAGXDocument* contextDoc, std::weak_ptr<PAGScene> owner);

  StateMachine* stateMachine = nullptr;

  struct RegionInstance;
  std::vector<RegionInstance> regions;
  std::vector<InputValue> inputValues;

  int nextListenerId = 1;
  std::vector<std::pair<int, std::function<void(const std::string&, const std::string&)>>>
      stateChangeListeners;

  void removeObserver(int id) override;

  bool advanceRegion(int64_t deltaUs);
  bool advanceMix(RegionInstance& ri, int64_t deltaUs);
  bool tryChangeState(RegionInstance& ri);
  void changeState(RegionInstance& ri, const StateTransition* t);
  std::shared_ptr<PAGAnimation> createTimelineForState(const State* state);

  // Marks every inner PAGAnimation (each region's current timeline and any crossfading-out ones)
  // stale so their targets re-resolve on the next apply(). The inner animations live in regions,
  // not in the owning composition's timeline list, so an incremental tree refresh that patches
  // binding membership must reach them through here.
  void markInternalTargetsDirty();

  friend class PAGScene;
  friend class PAGComposition;
};

}  // namespace pagx
