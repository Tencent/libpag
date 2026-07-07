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

#include <string>
#include "pagx/nodes/Node.h"

namespace pagx {

/**
 * TransitionConditionOp is the comparison operator applied between an input's current value and
 * the condition's value. The set of valid operators depends on the referenced input's type:
 *   Bool   -> Equal / NotEqual                       (compares against valueBool)
 *   Number -> Equal / NotEqual / LessThan / LessThanOrEqual / GreaterThan / GreaterThanOrEqual
 *                                                    (compares against valueNumber)
 *   Trigger-> Trigger (values ignored; checks fired-and-not-yet-consumed-in-this-region)
 */
enum class TransitionConditionOp : uint8_t {
  Equal = 0,
  NotEqual = 1,
  LessThan = 2,
  LessThanOrEqual = 3,
  GreaterThan = 4,
  GreaterThanOrEqual = 5,
  Trigger = 6,
};

/**
 * TransitionCondition tests a single StateMachine input against a value using an operator.
 * Multiple conditions on a transition are combined with AND.
 */
class TransitionCondition : public Node {
 public:
  /**
   * The name of the StateMachine input this condition tests.
   */
  std::string inputName = {};

  /**
   * The comparison operator. Must be compatible with the referenced input's type.
   */
  TransitionConditionOp op = TransitionConditionOp::Equal;

  /**
   * The boolean value compared against a Bool input's current value. Used when the referenced
   * input is Bool. Ignored for Number/Trigger inputs.
   */
  bool valueBool = false;

  /**
   * The numeric value compared against a Number input's current value. Used when the referenced
   * input is Number. Ignored for Bool/Trigger inputs.
   */
  float valueNumber = 0.0f;

  NodeType nodeType() const override {
    return NodeType::TransitionCondition;
  }

 private:
  TransitionCondition() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
