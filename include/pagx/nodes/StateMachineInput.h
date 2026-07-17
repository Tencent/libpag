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
#include <string>
#include "pagx/nodes/Node.h"

namespace pagx {

/**
 * StateMachineInputType selects the value kind carried by a StateMachineInput.
 */
enum class StateMachineInputType : uint8_t {
  /**
   * A persistent boolean value. Used with Equal / NotEqual conditions.
   */
  Bool = 0,
  /**
   * A persistent numeric value. Used with Equal / NotEqual / LessThan / LessThanOrEqual /
   * GreaterThan / GreaterThanOrEqual conditions.
   */
  Number = 1,
  /**
   * A one-shot event. Fired by host code, consumed at most once per region, then cleared at the
   * end of every advance (whether consumed or not).
   */
  Trigger = 2,
};

/**
 * StateMachineInput declares a named input port on a StateMachine. Inputs are shared by all
 * StateRegions and referenced by TransitionCondition.inputName.
 */
class StateMachineInput : public Node {
 public:
  /**
   * The input name, referenced by conditions. Unique within the owning StateMachine.
   */
  std::string name = {};

  /**
   * The value kind of this input.
   */
  StateMachineInputType type = StateMachineInputType::Bool;

  /**
   * The initial boolean value when type is Bool. Ignored for other types.
   */
  bool defaultBool = false;

  /**
   * The initial numeric value when type is Number. Ignored for other types.
   */
  float defaultNumber = 0.0f;

  NodeType nodeType() const override {
    return NodeType::StateMachineInput;
  }

 private:
  StateMachineInput() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
