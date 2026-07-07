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

#include "pagx/PAGStateMachineTimeline.h"
#include <algorithm>
#include <cmath>
#include <set>
#include "pagx/PAGScene.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationObject.h"
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/State.h"
#include "pagx/nodes/StateRegion.h"
#include "pagx/nodes/StateTransition.h"
#include "pagx/nodes/TransitionCondition.h"
#include "pagx/PAGViewModelValue.h"
#include "pagx/PAGViewModelValueBoolean.h"
#include "pagx/PAGViewModelValueNumber.h"
#include "renderer/LayerBuilder.h"

namespace pagx {

static constexpr int kMaxTransitionsPerFrame = 100;

// =============================================================================
// RegionInstance (hidden in .cpp)
// =============================================================================

struct PAGStateMachineTimeline::RegionInstance {
  struct FadingState {
    const State* state = nullptr;
    int64_t elapsedUs = 0;
    float mixFrom = 1.0f;
    bool frozen = false;
  };

  const StateRegion* region = nullptr;
  const State* currentState = nullptr;
  int64_t currentElapsedUs = 0;
  float mix = 1.0f;
  const StateTransition* transition = nullptr;
  std::vector<FadingState> fadingOut;
  std::set<std::string> consumedTriggers;
};

// =============================================================================
// Helpers (anonymous namespace)
// =============================================================================

namespace {

int64_t FramesToUs(Frame frames, float frameRate) {
  if (frameRate <= 0.0f) {
    return 0;
  }
  return static_cast<int64_t>(static_cast<double>(frames) / frameRate * 1000000.0);
}

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

float EvalBezier(Point bezierOut, Point bezierIn, float t) {
  if (t <= 0.0f) return 0.0f;
  if (t >= 1.0f) return 1.0f;
  float x0 = 0.0f, x1 = bezierOut.x, x2 = bezierIn.x, x3 = 1.0f;
  for (int i = 0; i < 10; i++) {
    float tMid = (x0 + x3) * 0.5f;
    float oneMinusMid = 1.0f - tMid;
    float x = oneMinusMid * oneMinusMid * oneMinusMid * x0 +
              3.0f * oneMinusMid * oneMinusMid * tMid * x1 + 3.0f * oneMinusMid * tMid * tMid * x2 +
              tMid * tMid * tMid * x3;
    if (x < t) {
      x0 = tMid;
    } else {
      x3 = tMid;
    }
  }
  return (x0 + x3) * 0.5f;
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
      return EvalBezier(transition->bezierOut, transition->bezierIn, t);
    case KeyframeInterpolationType::Linear:
    default:
      return t;
  }
}

}  // namespace

// =============================================================================
// Constructor / Destructor
// =============================================================================

PAGStateMachineTimeline::~PAGStateMachineTimeline() = default;

PAGStateMachineTimeline::PAGStateMachineTimeline(StateMachine* sm, RuntimeBinding* b,
                                                 PAGXDocument* ctx, std::weak_ptr<PAGScene> owner)
    : owner(std::move(owner)), stateMachine(sm), binding(b), contextDoc(ctx) {
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
    regions.push_back(std::move(ri));
  }
}

// =============================================================================
// Identity / queries
// =============================================================================

const std::string& PAGStateMachineTimeline::getId() const {
  if (stateMachine != nullptr) {
    return stateMachine->id;
  }
  static const std::string empty;
  return empty;
}

std::vector<std::string> PAGStateMachineTimeline::getRegionIds() const {
  std::vector<std::string> ids;
  if (stateMachine == nullptr) {
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

std::string PAGStateMachineTimeline::getCurrentState(const std::string& regionName) const {
  for (const auto& ri : regions) {
    if (ri.region != nullptr && ri.region->name == regionName && ri.currentState != nullptr) {
      return ri.currentState->name;
    }
  }
  return {};
}

PAGStateMachineTimeline::RegionInstance* PAGStateMachineTimeline::findRegion(
    const std::string& regionName) {
  for (auto& ri : regions) {
    if (ri.region != nullptr && ri.region->name == regionName) {
      return &ri;
    }
  }
  return nullptr;
}

// =============================================================================
// Input setters
// =============================================================================

static int findInputIdx(const StateMachine* sm, const std::string& name) {
  for (size_t i = 0; i < sm->inputs.size(); i++) {
    if (sm->inputs[i] != nullptr && sm->inputs[i]->name == name) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

bool PAGStateMachineTimeline::setBool(const std::string& name, bool value) {
  int idx = findInputIdx(stateMachine, name);
  if (idx < 0 || inputValues[idx].type != StateMachineInputType::Bool) {
    return false;
  }
  inputValues[idx].boolValue = value;
  return true;
}

bool PAGStateMachineTimeline::setNumber(const std::string& name, float value) {
  int idx = findInputIdx(stateMachine, name);
  if (idx < 0 || inputValues[idx].type != StateMachineInputType::Number) {
    return false;
  }
  inputValues[idx].numberValue = value;
  return true;
}

bool PAGStateMachineTimeline::fireTrigger(const std::string& name) {
  int idx = findInputIdx(stateMachine, name);
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
                              const PAGStateMachineTimeline::InputValue& input) {
  if (condition == nullptr) {
    return true;
  }
  switch (input.type) {
    case StateMachineInputType::Bool:
      return condition->op == TransitionConditionOp::Equal
                 ? (input.boolValue == condition->valueBool)
                 : (input.boolValue != condition->valueBool);
    case StateMachineInputType::Number:
      switch (condition->op) {
        case TransitionConditionOp::Equal:
          return input.numberValue == condition->valueNumber;
        case TransitionConditionOp::NotEqual:
          return input.numberValue != condition->valueNumber;
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

static bool IsTransitionAllowed(const StateTransition* transition, int64_t currentElapsedUs,
                                float frameRate, const PAGXDocument* contextDoc,
                                const State* currentState,
                                const std::vector<PAGStateMachineTimeline::InputValue>& inputValues,
                                const StateMachine* stateMachine,
                                const std::set<std::string>& consumedTriggers) {
  if (transition == nullptr) {
    return false;
  }
  // Exit-time gate.
  if (transition->exitTime.has_value()) {
    auto* animState = dynamic_cast<const AnimationState*>(currentState);
    if (animState != nullptr && !animState->animationId.empty() && contextDoc != nullptr) {
      auto* anim = contextDoc->findNode<Animation>(animState->animationId);
      if (anim != nullptr) {
        int64_t exitUs = FramesToUs(transition->exitTime.value(), frameRate);
        int64_t durationUs = FramesToUs(anim->duration, frameRate);
        int64_t alignedExitUs = exitUs;
        if (exitUs <= durationUs && anim->loop != LoopMode::Once && durationUs > 0) {
          alignedExitUs +=
              static_cast<int64_t>(std::floor(static_cast<double>(currentElapsedUs) / durationUs)) *
              durationUs;
        }
        if (currentElapsedUs < alignedExitUs) {
          return false;
        }
      }
    }
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
      // Find the trigger input and check fired.
      for (size_t i = 0; i < stateMachine->inputs.size(); i++) {
        if (stateMachine->inputs[i] != nullptr &&
            stateMachine->inputs[i]->name == condition->inputName &&
            stateMachine->inputs[i]->type == StateMachineInputType::Trigger) {
          if (!inputValues[i].fired) {
            return false;
          }
          break;
        }
      }
    } else {
      // Non-trigger condition: find input and evaluate.
      bool found = false;
      for (size_t i = 0; i < stateMachine->inputs.size(); i++) {
        if (stateMachine->inputs[i] != nullptr &&
            stateMachine->inputs[i]->name == condition->inputName) {
          if (!EvaluateCondition(condition, inputValues[i])) {
            return false;
          }
          found = true;
          break;
        }
      }
      if (!found) {
        return false;
      }
    }
  }
  return true;
}

static const StateTransition* FindAllowedTransition(
    const StateRegion* region, const std::string& fromName, bool inTransition,
    int64_t currentElapsedUs, float frameRate, const PAGXDocument* contextDoc,
    const State* currentState, const std::vector<PAGStateMachineTimeline::InputValue>& inputValues,
    const StateMachine* stateMachine, const std::set<std::string>& consumedTriggers) {
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
    if (IsTransitionAllowed(t, currentElapsedUs, frameRate, contextDoc, currentState, inputValues,
                            stateMachine, consumedTriggers)) {
      return t;
    }
  }
  return nullptr;
}

// =============================================================================
// Region state transition (member)
// =============================================================================

void PAGStateMachineTimeline::changeState(RegionInstance& ri, const StateTransition* t) {
  if (t == nullptr) {
    return;
  }
  auto frameRate = contextDoc != nullptr && !contextDoc->animations.empty()
                       ? contextDoc->animations[0]->frameRate
                       : 60.0f;
  RegionInstance::FadingState fading;
  fading.state = ri.currentState;
  fading.elapsedUs = ri.currentElapsedUs;
  fading.mixFrom = ri.mix;
  fading.frozen = false;
  if (t->pauseOnExit && t->exitTime.has_value()) {
    fading.elapsedUs = FramesToUs(t->exitTime.value(), frameRate);
    fading.frozen = true;
  }
  ri.fadingOut.push_back(fading);
  ri.currentState = FindStateByName(ri.region, t->to);
  ri.currentElapsedUs = 0;
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
    for (auto& [listenerId, cb] : stateChangeListeners) {
      if (cb) {
        cb(ri.region->name, ri.currentState->name);
      }
    }
  }
}

bool PAGStateMachineTimeline::tryChangeState(RegionInstance& ri) {
  auto frameRate = contextDoc != nullptr && !contextDoc->animations.empty()
                       ? contextDoc->animations[0]->frameRate
                       : 60.0f;
  bool inTransition = (ri.transition != nullptr && ri.mix < 1.0f);
  const StateTransition* t = FindAllowedTransition(
      ri.region, AnyStateName, inTransition, ri.currentElapsedUs, frameRate, contextDoc,
      ri.currentState, inputValues, stateMachine, ri.consumedTriggers);
  if (t == nullptr) {
    t = FindAllowedTransition(ri.region,
                              ri.currentState != nullptr ? ri.currentState->name : std::string{},
                              inTransition, ri.currentElapsedUs, frameRate, contextDoc,
                              ri.currentState, inputValues, stateMachine, ri.consumedTriggers);
  }
  if (t == nullptr) {
    return false;
  }
  changeState(ri, t);
  return true;
}

void PAGStateMachineTimeline::advanceRegion(int64_t deltaUs) {
  for (auto& ri : regions) {
    if (ri.region == nullptr || ri.currentState == nullptr) {
      continue;
    }
    auto frameRate = contextDoc != nullptr && !contextDoc->animations.empty()
                         ? contextDoc->animations[0]->frameRate
                         : 60.0f;
    ri.currentElapsedUs += deltaUs;
    for (auto& f : ri.fadingOut) {
      if (!f.frozen) {
        f.elapsedUs += deltaUs;
      }
    }
    if (ri.transition != nullptr) {
      int64_t mixDurationUs = FramesToUs(ri.transition->duration, frameRate);
      if (mixDurationUs <= 0) {
        ri.mix = 1.0f;
      } else {
        ri.mix =
            std::min(1.0f, std::max(0.0f, ri.mix + static_cast<float>(deltaUs) / mixDurationUs));
      }
      if (ri.mix >= 1.0f) {
        ri.fadingOut.clear();
        ri.transition = nullptr;
      }
    }
    for (int i = 0; i < kMaxTransitionsPerFrame; i++) {
      if (!tryChangeState(ri)) {
        break;
      }
    }
  }
}

// =============================================================================
// advance / apply
// =============================================================================

bool PAGStateMachineTimeline::advance(int64_t deltaMicroseconds) {
  if (stateMachine == nullptr || owner.expired()) {
    return false;
  }
  bool hadActive = false;
  for (const auto& ri : regions) {
    if (ri.transition != nullptr && ri.mix < 1.0f) {
      hadActive = true;
      break;
    }
  }
  advanceRegion(deltaMicroseconds);
  bool hasActive = false;
  for (const auto& ri : regions) {
    if (ri.transition != nullptr && ri.mix < 1.0f) {
      hasActive = true;
      break;
    }
  }
  return hadActive || hasActive;
}

void PAGStateMachineTimeline::apply(float smMix) {
  if (stateMachine == nullptr || owner.expired()) {
    return;
  }
  auto clamped = std::clamp(smMix, 0.0f, 1.0f);
  if (clamped <= 0.0f) {
    return;
  }
  // Resolve effective binding: null binding is a top-level timeline that resolves the scene's
  // current root binding lazily at apply time, same as PAGTimeline.
  RuntimeBinding* effectiveBinding = binding;
  if (effectiveBinding == nullptr) {
    auto scene = owner.lock();
    effectiveBinding = scene != nullptr ? scene->mutableBinding() : nullptr;
  }
  if (effectiveBinding == nullptr) {
    return;
  }
  for (auto& ri : regions) {
    for (const auto& f : ri.fadingOut) {
      float weight = smMix * CurveMix(ri.transition, f.mixFrom);
      if (weight <= 0.0f || f.state == nullptr) {
        continue;
      }
      auto* animState = dynamic_cast<const AnimationState*>(f.state);
      if (animState == nullptr || animState->animationId.empty() || contextDoc == nullptr) {
        continue;
      }
      auto* anim = contextDoc->findNode<Animation>(animState->animationId);
      if (anim == nullptr) {
        continue;
      }
      int64_t sampleUs = f.elapsedUs;
      int64_t durationUs = FramesToUs(anim->duration, anim->frameRate);
      if (durationUs > 0) {
        if (anim->loop == LoopMode::Loop) {
          sampleUs = sampleUs % durationUs;
          if (sampleUs < 0) sampleUs += durationUs;
        } else if (anim->loop == LoopMode::Once) {
          sampleUs = std::min(sampleUs, durationUs);
        }
      }
      for (const auto* object : anim->objects) {
        if (object == nullptr) {
          continue;
        }
        auto* targetNode = contextDoc->findNode(object->target);
        if (targetNode == nullptr || effectiveBinding == nullptr) {
          continue;
        }
        for (const auto* channel : object->channels) {
          if (channel == nullptr) {
            continue;
          }
          auto value = channel->evaluateAt(sampleUs, anim->frameRate);
          effectiveBinding->apply(targetNode, channel->name, value, weight);
        }
      }
    }
    float weight = smMix * CurveMix(ri.transition, ri.mix);
    if (weight <= 0.0f || ri.currentState == nullptr) {
      continue;
    }
    auto* animState = dynamic_cast<const AnimationState*>(ri.currentState);
    if (animState == nullptr || animState->animationId.empty() || contextDoc == nullptr) {
      continue;
    }
    auto* anim = contextDoc->findNode<Animation>(animState->animationId);
    if (anim == nullptr) {
      continue;
    }
    int64_t sampleUs = ri.currentElapsedUs;
    int64_t durationUs = FramesToUs(anim->duration, anim->frameRate);
    if (durationUs > 0) {
      if (anim->loop == LoopMode::Loop) {
        sampleUs = sampleUs % durationUs;
        if (sampleUs < 0) sampleUs += durationUs;
      } else if (anim->loop == LoopMode::Once) {
        sampleUs = std::min(sampleUs, durationUs);
      }
    }
    for (const auto* object : anim->objects) {
      if (object == nullptr) {
        continue;
      }
      auto* targetNode = contextDoc->findNode(object->target);
      if (targetNode == nullptr || binding == nullptr) {
        continue;
      }
      for (const auto* channel : object->channels) {
        if (channel == nullptr) {
          continue;
        }
        auto value = channel->evaluateAt(sampleUs, anim->frameRate);
        binding->apply(targetNode, channel->name, value, weight);
      }
    }
  }
}

bool PAGStateMachineTimeline::advanceAndApply(int64_t deltaMicroseconds, float smMix) {
  bool result = advance(deltaMicroseconds);
  apply(smMix);
  for (auto& iv : inputValues) {
    if (iv.type == StateMachineInputType::Trigger) {
      iv.fired = false;
    }
  }
  for (auto& ri : regions) {
    ri.consumedTriggers.clear();
  }
  return result;
}

// =============================================================================
// State change listeners
// =============================================================================

int PAGStateMachineTimeline::addStateChangeListener(
    std::function<void(const std::string&, const std::string&)> callback) {
  if (!callback) {
    return -1;
  }
  int id = nextListenerId++;
  stateChangeListeners.emplace_back(id, std::move(callback));
  return id;
}

void PAGStateMachineTimeline::removeStateChangeListener(int listenerId) {
  for (auto it = stateChangeListeners.begin(); it != stateChangeListeners.end(); ++it) {
    if (it->first == listenerId) {
      stateChangeListeners.erase(it);
      break;
    }
  }
}

bool PAGStateMachineTimeline::bindInput(const std::string& inputName,
                                        const std::shared_ptr<PAGViewModelValue>& vmValue) {
  if (!vmValue || !stateMachine) {
    return false;
  }
  int idx = -1;
  for (size_t i = 0; i < stateMachine->inputs.size(); i++) {
    if (stateMachine->inputs[i] && stateMachine->inputs[i]->name == inputName) {
      idx = static_cast<int>(i);
      break;
    }
  }
  if (idx < 0) {
    return false;
  }
  auto rawThis = this;
  auto inputType = inputValues[idx].type;

  if (inputType == StateMachineInputType::Bool) {
    auto boolVal = std::dynamic_pointer_cast<PAGViewModelValueBoolean>(vmValue);
    if (!boolVal) {
      return false;
    }
    auto handle = boolVal->addObserver([rawThis, inputName, boolVal]() {
      rawThis->setBool(inputName, boolVal->value());
    });
    inputBindings.push_back(std::move(handle));
    return true;
  }
  if (inputType == StateMachineInputType::Number) {
    auto numVal = std::dynamic_pointer_cast<PAGViewModelValueNumber>(vmValue);
    if (!numVal) {
      return false;
    }
    auto handle = numVal->addObserver([rawThis, inputName, numVal]() {
      rawThis->setNumber(inputName, numVal->value());
    });
    inputBindings.push_back(std::move(handle));
    return true;
  }
  if (inputType == StateMachineInputType::Trigger) {
    auto boolVal = std::dynamic_pointer_cast<PAGViewModelValueBoolean>(vmValue);
    if (!boolVal) {
      return false;
    }
    auto handle = boolVal->addObserver([rawThis, inputName, boolVal]() {
      if (boolVal->value()) {
        rawThis->fireTrigger(inputName);
      }
    });
    inputBindings.push_back(std::move(handle));
    return true;
  }
  return false;
}

}  // namespace pagx
