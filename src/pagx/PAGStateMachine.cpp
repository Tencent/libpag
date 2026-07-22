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

#include "pagx/PAGStateMachine.h"
#include <algorithm>
#include <set>
#include "base/utils/Log.h"
#include "base/utils/MathUtil.h"
#include "pagx/PAGAnimation.h"
#include "pagx/PAGScene.h"
#include "pagx/PAGStateMachineRegion.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/State.h"
#include "pagx/nodes/StateRegion.h"
#include "pagx/nodes/StateTransition.h"
#include "pagx/nodes/TransitionCondition.h"
#include "pagx/runtime/BezierEasing.h"

namespace pagx {

static constexpr int MAX_TRANSITIONS_PER_FRAME = 100;

// =============================================================================
// Helpers (anonymous namespace)
// =============================================================================

namespace {

const State* FindStateByName(const StateRegion* region, const std::string& name) {
  if (region == nullptr) {
    return nullptr;
  }
  for (const auto* s : region->states) {
    if (s != nullptr && s->name == name) {
      return s;
    }
  }
  return nullptr;
}

float CurveMix(const StateTransition* transition, float t) {
  if (transition == nullptr) {
    return t;
  }
  switch (transition->interpolation) {
    case KeyframeInterpolationType::None:
    case KeyframeInterpolationType::Hold:
      return t >= 1.0f ? 1.0f : 0.0f;
    case KeyframeInterpolationType::Bezier:
      return static_cast<float>(SolveBezierEasing(transition->bezierOut.x, transition->bezierOut.y,
                                                  transition->bezierIn.x, transition->bezierIn.y,
                                                  t));
    case KeyframeInterpolationType::Linear:
    default:
      return t;
  }
}

}  // namespace

// =============================================================================
// Constructor / Destructor
// =============================================================================

PAGStateMachine::~PAGStateMachine() = default;

PAGStateMachine::PAGStateMachine(StateMachine* sm, RuntimeBinding* b, PAGXDocument* ctx,
                                 std::weak_ptr<PAGScene> owner)
    : PAGTimeline(b, ctx, std::move(owner)), stateMachine(sm) {
  if (stateMachine == nullptr) {
    return;
  }
  inputValues.reserve(stateMachine->inputs.size());
  for (const auto* input : stateMachine->inputs) {
    InputValue value = {};
    value.type = input->type;
    value.boolValue = (input->type == StateMachineInputType::Bool) ? input->defaultBool : false;
    value.numberValue =
        (input->type == StateMachineInputType::Number) ? input->defaultNumber : 0.0f;
    value.fired = false;
    inputValues.push_back(value);
  }
  regions.reserve(stateMachine->regions.size());
  for (const auto* region : stateMachine->regions) {
    RegionInstance ri = {};
    ri.region = region;
    ri.mix = 1.0f;
    for (const auto* state : region->states) {
      if (state != nullptr && state->name == region->initialState) {
        ri.currentState = state;
        break;
      }
    }
    if (ri.currentState == nullptr && !region->states.empty()) {
      ri.currentState = region->states[0];
    }
    ri.currentTimeline = createTimelineForState(ri.currentState);
    regions.push_back(std::move(ri));
  }
}

// =============================================================================
// Identity / queries
// =============================================================================

const std::string& PAGStateMachine::getId() const {
  if (stateMachine == nullptr || owner.expired()) {
    static const std::string empty;
    return empty;
  }
  return stateMachine->id;
}

std::vector<std::string> PAGStateMachine::getRegionIds() const {
  std::vector<std::string> ids;
  if (stateMachine == nullptr || owner.expired()) {
    return ids;
  }
  ids.reserve(regions.size());
  for (const auto& ri : regions) {
    if (ri.region != nullptr) {
      ids.push_back(ri.region->name);
    }
  }
  return ids;
}

std::string PAGStateMachine::getCurrentState(const std::string& regionName) const {
  if (stateMachine == nullptr || owner.expired()) {
    return {};
  }
  for (const auto& ri : regions) {
    if (ri.region != nullptr && ri.region->name == regionName && ri.currentState != nullptr) {
      return ri.currentState->name;
    }
  }
  return {};
}

// =============================================================================
// Input setters
// =============================================================================

static int FindInputIndex(const StateMachine* sm, const std::string& name) {
  for (size_t i = 0; i < sm->inputs.size(); i++) {
    if (sm->inputs[i] != nullptr && sm->inputs[i]->name == name) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

bool PAGStateMachine::setBool(const std::string& name, bool value) {
  if (stateMachine == nullptr || owner.expired()) {
    return false;
  }
  int idx = FindInputIndex(stateMachine, name);
  if (idx < 0 || inputValues[idx].type != StateMachineInputType::Bool) {
    return false;
  }
  inputValues[idx].boolValue = value;
  return true;
}

bool PAGStateMachine::setNumber(const std::string& name, float value) {
  if (stateMachine == nullptr || owner.expired()) {
    return false;
  }
  int idx = FindInputIndex(stateMachine, name);
  if (idx < 0 || inputValues[idx].type != StateMachineInputType::Number) {
    return false;
  }
  inputValues[idx].numberValue = value;
  return true;
}

bool PAGStateMachine::fireTrigger(const std::string& name) {
  if (stateMachine == nullptr || owner.expired()) {
    return false;
  }
  int idx = FindInputIndex(stateMachine, name);
  if (idx < 0 || inputValues[idx].type != StateMachineInputType::Trigger) {
    return false;
  }
  if (inputValues[idx].fired) {
    return true;
  }
  inputValues[idx].fired = true;
  return true;
}

// =============================================================================
// Condition evaluation (member)
// =============================================================================

static bool EvaluateCondition(const TransitionCondition* condition,
                              const PAGStateMachine::InputValue& input) {
  if (condition == nullptr) {
    return true;
  }
  switch (input.type) {
    case StateMachineInputType::Bool:
      switch (condition->op) {
        case TransitionConditionOp::Equal:
          return input.boolValue == condition->valueBool;
        case TransitionConditionOp::NotEqual:
          return input.boolValue != condition->valueBool;
        default:
          return false;
      }
    case StateMachineInputType::Number:
      switch (condition->op) {
        case TransitionConditionOp::Equal:
          return pag::FloatNearlyEqual(input.numberValue, condition->valueNumber);
        case TransitionConditionOp::NotEqual:
          return !pag::FloatNearlyEqual(input.numberValue, condition->valueNumber);
        case TransitionConditionOp::LessThan:
          return input.numberValue < condition->valueNumber;
        case TransitionConditionOp::LessThanOrEqual:
          return input.numberValue <= condition->valueNumber;
        case TransitionConditionOp::GreaterThan:
          return input.numberValue > condition->valueNumber;
        case TransitionConditionOp::GreaterThanOrEqual:
          return input.numberValue >= condition->valueNumber;
        default:
          return false;
      }
    case StateMachineInputType::Trigger:
      return false;  // Handled separately via consumedTriggers.
  }
  return false;
}

// =============================================================================
// Transition lookup (member helpers)
// =============================================================================

static bool IsTransitionAllowed(const StateTransition* transition,
                                const std::vector<PAGStateMachine::InputValue>& inputValues,
                                const StateMachine* stateMachine,
                                const std::set<std::string>& consumedTriggers) {
  if (transition == nullptr) {
    return false;
  }
  // All conditions must hold (AND).
  for (const auto* condition : transition->conditions) {
    if (condition == nullptr) {
      continue;
    }
    if (condition->op == TransitionConditionOp::Trigger) {
      if (consumedTriggers.find(condition->inputName) != consumedTriggers.end()) {
        return false;
      }
      int idx = FindInputIndex(stateMachine, condition->inputName);
      if (idx < 0 || stateMachine->inputs[idx]->type != StateMachineInputType::Trigger ||
          !inputValues[idx].fired) {
        return false;
      }
    } else {
      // Non-trigger condition: find input and evaluate.
      int idx = FindInputIndex(stateMachine, condition->inputName);
      if (idx < 0 || !EvaluateCondition(condition, inputValues[idx])) {
        return false;
      }
    }
  }
  return true;
}

static const StateTransition* FindAllowedTransition(
    const StateRegion* region, const std::string& fromName, bool inTransition,
    const std::vector<PAGStateMachine::InputValue>& inputValues, const StateMachine* stateMachine,
    const std::set<std::string>& consumedTriggers) {
  if (region == nullptr) {
    return nullptr;
  }
  for (const auto* t : region->transitions) {
    if (t == nullptr || t->from != fromName) {
      continue;
    }
    if (inTransition && !t->enableEarlyExit) {
      continue;
    }
    if (IsTransitionAllowed(t, inputValues, stateMachine, consumedTriggers)) {
      return t;
    }
  }
  return nullptr;
}

// =============================================================================
// Region state transition (member)
// =============================================================================

std::shared_ptr<PAGAnimation> PAGStateMachine::createTimelineForState(const State* state) {
  if (state == nullptr || state->stateType() != StateType::Animation || contextDoc == nullptr) {
    return nullptr;
  }
  auto* animState = static_cast<const AnimationState*>(state);
  if (animState->animationId.empty()) {
    return nullptr;
  }
  auto* anim = contextDoc->findNode<Animation>(animState->animationId);
  // A state referencing a missing or wrong-typed node is a malformed document (import only
  // validates the "@id" syntax, not existence or type). Surface it loudly in debug builds; in
  // release fall back to an empty state that drives no channels.
  DEBUG_ASSERT(anim != nullptr);
  DEBUG_ASSERT(anim == nullptr || anim->nodeType() == NodeType::Animation);
  if (anim == nullptr || anim->nodeType() != NodeType::Animation) {
    return nullptr;
  }
  return std::shared_ptr<PAGAnimation>(new PAGAnimation(anim, binding, contextDoc, owner));
}

void PAGStateMachine::markInternalTargetsDirty() {
  for (auto& ri : regions) {
    if (ri.currentTimeline != nullptr) {
      ri.currentTimeline->targetsDirty = true;
    }
    for (auto& fading : ri.fadingOut) {
      if (fading.timeline != nullptr) {
        fading.timeline->targetsDirty = true;
      }
    }
  }
}

void PAGStateMachine::changeState(RegionInstance& ri, const StateTransition* t) {
  if (t == nullptr) {
    return;
  }
  RegionInstance::FadingState fading;
  fading.timeline = ri.currentTimeline;
  fading.mixFrom = ri.mix;
  fading.frozen = false;
  if (t->pauseOnExit && fading.timeline != nullptr) {
    // Freeze the outgoing animation at its current frame so it holds that pose while it mixes out,
    // instead of continuing to advance during the crossfade.
    fading.frozen = true;
  }
  ri.fadingOut.push_back(std::move(fading));
  ri.currentState = FindStateByName(ri.region, t->to);
  ri.currentTimeline = createTimelineForState(ri.currentState);
  ri.transition = t;
  ri.mix = (t->duration <= 0) ? 1.0f : 0.0f;
  if (ri.mix >= 1.0f) {
    ri.fadingOut.clear();
    ri.transition = nullptr;
  }
  for (const auto* c : t->conditions) {
    if (c != nullptr && c->op == TransitionConditionOp::Trigger) {
      ri.consumedTriggers.insert(c->inputName);
    }
  }
  if (ri.currentState != nullptr) {
    // Snapshot listeners so a reentrant add/remove during dispatch does not invalidate the
    // vector being iterated.
    auto listenersSnapshot = stateChangeListeners;
    for (auto& [listenerId, cb] : listenersSnapshot) {
      if (cb) {
        cb(ri.region->name, ri.currentState->name);
      }
    }
  }
}

bool PAGStateMachine::tryChangeState(RegionInstance& ri) {
  bool inTransition = (ri.transition != nullptr && ri.mix < 1.0f);
  const StateTransition* t = FindAllowedTransition(ri.region, AnyStateName, inTransition,
                                                   inputValues, stateMachine, ri.consumedTriggers);
  if (t == nullptr) {
    t = FindAllowedTransition(ri.region,
                              ri.currentState != nullptr ? ri.currentState->name : std::string{},
                              inTransition, inputValues, stateMachine, ri.consumedTriggers);
  }
  if (t == nullptr) {
    return false;
  }
  changeState(ri, t);
  return true;
}

bool PAGStateMachine::advanceMix(RegionInstance& ri, int64_t deltaUs) {
  if (ri.transition == nullptr) {
    return false;
  }
  // The crossfade duration is authored in frames and converted using the transition's own frame
  // rate, so the crossfade speed does not depend on which state animations it connects.
  float frameRate = ri.transition->frameRate > 0.0f ? ri.transition->frameRate : 60.0f;
  int64_t mixDurationUs = FramesToUs(ri.transition->duration, frameRate);
  float previous = ri.mix;
  if (mixDurationUs <= 0) {
    ri.mix = 1.0f;
  } else {
    ri.mix = std::min(1.0f, std::max(0.0f, ri.mix + static_cast<float>(deltaUs) / mixDurationUs));
  }
  if (ri.mix >= 1.0f) {
    ri.fadingOut.clear();
    ri.transition = nullptr;
  }
  return ri.mix != previous;
}

bool PAGStateMachine::advanceRegion(int64_t deltaUs) {
  bool changed = false;
  for (auto& ri : regions) {
    if (ri.region == nullptr) {
      continue;
    }
    if (ri.currentTimeline != nullptr && ri.currentTimeline->advance(deltaUs)) {
      changed = true;
    }
    for (auto& f : ri.fadingOut) {
      if (!f.frozen && f.timeline != nullptr && f.timeline->advance(deltaUs)) {
        changed = true;
      }
    }
    changed |= advanceMix(ri, deltaUs);
    const StateTransition* alreadyAdvanced = ri.transition;
    for (int i = 0; i < MAX_TRANSITIONS_PER_FRAME; i++) {
      if (!tryChangeState(ri)) {
        break;
      }
      changed = true;
      if (i == MAX_TRANSITIONS_PER_FRAME - 1) {
        LOGE(
            "PAGStateMachine: exceeded max transitions per frame (%d), "
            "possible zero-duration state loop.",
            MAX_TRANSITIONS_PER_FRAME);
      }
    }
    // A new transition started by tryChangeState begins at mix=0. Advance it by this frame's
    // delta so the first apply sees a non-zero mix (prevents a one-frame blank output). Skip a
    // carried-over transition that was already advanced above this frame, to avoid 2x speed.
    if (ri.transition != alreadyAdvanced) {
      changed |= advanceMix(ri, deltaUs);
    }
  }
  return changed;
}

// =============================================================================
// advance / apply
// =============================================================================

bool PAGStateMachine::advance(int64_t deltaMicroseconds) {
  if (stateMachine == nullptr || owner.expired()) {
    return false;
  }
  bool changed = advanceRegion(deltaMicroseconds);
  // Clear consumed triggers and fired flags at the end of advance so that nested state machines
  // driven via separate advance() + apply() calls also clear correctly.
  for (auto& iv : inputValues) {
    if (iv.type == StateMachineInputType::Trigger) {
      iv.fired = false;
    }
  }
  for (auto& ri : regions) {
    ri.consumedTriggers.clear();
  }
  return changed;
}

void PAGStateMachine::apply(float smMix) {
  if (stateMachine == nullptr || owner.expired()) {
    return;
  }
  auto clamped = std::clamp(smMix, 0.0f, 1.0f);
  if (clamped <= 0.0f) {
    return;
  }
  // Each playback slot owns its PAGAnimation, which holds the playback time and resolves the binding
  // (including the null-binding lazy path for top-level timelines) internally.
  // Crossfade weight model: the outgoing animation keeps its full weight (mixFrom, typically 1.0)
  // so the source state remains stable while the incoming animation blends in on top (mix 0 to 1).
  // This is a "new-over-old" model, matching rive's crossfade semantics, not a bidirectional fade.
  for (auto& ri : regions) {
    for (const auto& f : ri.fadingOut) {
      float weight = clamped * CurveMix(ri.transition, f.mixFrom);
      if (weight <= 0.0f || f.timeline == nullptr) {
        continue;
      }
      f.timeline->apply(weight);
    }
    float weight = clamped * CurveMix(ri.transition, ri.mix);
    if (weight <= 0.0f || ri.currentTimeline == nullptr) {
      continue;
    }
    ri.currentTimeline->apply(weight);
  }
}

// =============================================================================
// State change listeners
// =============================================================================

ObserverHandle PAGStateMachine::addStateChangeListener(
    std::function<void(const std::string&, const std::string&)> callback) {
  if (!callback) {
    return ObserverHandle();
  }
  auto self = weak_from_this().lock();
  if (self == nullptr) {
    return ObserverHandle();
  }
  int id = nextListenerId++;
  stateChangeListeners.emplace_back(id, std::move(callback));
  return ObserverHandle(self, id);
}

void PAGStateMachine::removeObserver(int listenerId) {
  for (auto it = stateChangeListeners.begin(); it != stateChangeListeners.end(); ++it) {
    if (it->first == listenerId) {
      stateChangeListeners.erase(it);
      break;
    }
  }
}

}  // namespace pagx
