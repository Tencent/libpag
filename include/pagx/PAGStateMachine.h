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
class PAGViewModelValue;
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
class PAGStateMachine : public PAGTimeline {
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
   * Convenience method exactly equivalent to advance(deltaMicroseconds) followed by apply(mix).
   * Returns the result of advance(deltaMicroseconds).
   */
  bool advanceAndApply(int64_t deltaMicroseconds, float mix = 1.0f) override;

  /**
   * Registers a state-change listener. The callback receives the region name and the new state
   * name whenever a region transitions. Returns an opaque listener id; pass it to
   * removeStateChangeListener() to unregister. Returns -1 if callback is null.
   *
   * Reentrant add/remove is safe: listeners are snapshotted before dispatch, so a callback that
   * adds or removes listeners during dispatch does not invalidate the ongoing iteration.
   */
  int addStateChangeListener(
      std::function<void(const std::string& regionName, const std::string& newState)> callback);

  /**
   * Removes a previously registered state-change listener. Safe to call with an unknown id.
   */
  void removeStateChangeListener(int listenerId);

  /**
   * Binds a ViewModel property to a StateMachine input, so that changes to the VM value are
   * automatically synchronized to the SM input. The VM property type must be compatible with the
   * SM input type: Boolean for Bool inputs, Number for Number inputs, Boolean for Trigger inputs
   * (where a true transition fires the trigger).
   * @param inputName the name of the SM input declared in <Inputs>.
   * @param vmValue the ViewModel property value to bind. The binding holds a weak reference to
   *        vmValue; if vmValue is destroyed, the binding becomes inactive and future notifications
   *        are dropped silently.
   * @return true if the binding was established successfully.
   */
  bool bindInput(const std::string& inputName, const std::shared_ptr<PAGViewModelValue>& vmValue);

  struct InputValue {
    StateMachineInputType type = StateMachineInputType::Bool;
    bool boolValue = false;
    float numberValue = 0.0f;
    bool fired = false;
  };

 private:
  explicit PAGStateMachine(StateMachine* stateMachine, RuntimeBinding* binding,
                           PAGXDocument* contextDoc, std::weak_ptr<PAGScene> owner);

  std::weak_ptr<PAGScene> owner;
  StateMachine* stateMachine = nullptr;
  RuntimeBinding* binding = nullptr;
  PAGXDocument* contextDoc = nullptr;

  struct RegionInstance;
  std::vector<RegionInstance> regions;
  std::vector<InputValue> inputValues;

  int nextListenerId = 1;
  std::vector<std::pair<int, std::function<void(const std::string&, const std::string&)>>>
      stateChangeListeners;

  // VM→SM input bindings. Each entry holds an ObserverHandle that detaches on destruction.
  std::vector<ObserverHandle> inputBindings;

  // Region advance helpers.
  // Advances every region by deltaUs and returns true if any region's playback state changed this
  // frame: an active or fading animation advanced its time, a state transition occurred, or the
  // crossfade mix moved.
  bool advanceRegion(int64_t deltaUs);
  // Advances the region's crossfade mix by one frame's worth of deltaUs, using the transition's
  // own frame rate to convert its duration (in frames) to time. Clears the transition and fading
  // slots once mix reaches 1.0. Returns true if the mix value changed.
  bool advanceMix(RegionInstance& ri, int64_t deltaUs);
  bool tryChangeState(RegionInstance& ri);
  void changeState(RegionInstance& ri, const StateTransition* t);
  // Constructs a fresh PAGAnimation that plays the given state's animation. Each playback slot
  // (current or fading) owns its own instance so their playback times stay independent. Returns
  // nullptr for an empty state or a missing animation (asserts in debug builds to surface a
  // dangling reference).
  std::shared_ptr<PAGAnimation> createTimelineForState(const State* state);

  friend class PAGScene;
  friend class PAGComposition;
};

}  // namespace pagx
