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
//  license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <vector>
#include "pagx/nodes/TextSelector.h"

namespace pagx {

/**
 * Evaluates the selectors of one TextModifier for a text of `count` characters. Selector ranges are
 * converted from their unit into the character domain, the offset shifts both ends, and each
 * character's weight comes from the configured shape and easing. Weights are combined in selector
 * order: the first selector seeds the result directly (an explicit Subtract starts from 1), later
 * selectors merge with their own mode. The result is clamped to [-1, 1].
 */
class TextSelectorEvaluator {
 public:
  /**
   * Builds the evaluator. Selectors that are not RangeSelector nodes are ignored; an empty selector
   * list makes every character weigh 1 so a modifier without selectors affects the whole run.
   */
  TextSelectorEvaluator(const std::vector<TextSelector*>& selectors, size_t count);

  /**
   * Returns the combined weight of the character at `index`. Out-of-range indices return 0.
   */
  float weight(size_t index) const;

 private:
  struct Range {
    const TextSelector* selector = nullptr;
    float start = 0.0f;
    float end = 1.0f;
    std::vector<size_t> order = {};
  };

  float evaluate(const Range& range, size_t index) const;

  std::vector<Range> _ranges = {};
  size_t _count = 0;
};

}  // namespace pagx
