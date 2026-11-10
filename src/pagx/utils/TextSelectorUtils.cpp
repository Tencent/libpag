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

#include "pagx/utils/TextSelectorUtils.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include "base/utils/BezierEasing.h"
#include "pagx/nodes/RangeSelector.h"
#include "pagx/types/SelectorTypes.h"

namespace pagx {

namespace {

float Clamp01(float value) {
  if (value < 0.0f) {
    return 0.0f;
  }
  if (value > 1.0f) {
    return 1.0f;
  }
  return value;
}

float ClampFactor(float value) {
  if (value < -1.0f) {
    return -1.0f;
  }
  if (value > 1.0f) {
    return 1.0f;
  }
  return value;
}

float SquareFactor(float textStart, float textEnd, float rangeStart, float rangeEnd) {
  if (textStart >= rangeEnd || textEnd <= rangeStart) {
    return 0.0f;
  }
  auto textRange = textEnd - textStart;
  if (textStart < rangeStart) {
    textStart = rangeStart;
  }
  if (textEnd > rangeEnd) {
    textEnd = rangeEnd;
  }
  return (textEnd - textStart) / textRange;
}

float RampUpFactor(float textStart, float textEnd, float rangeStart, float rangeEnd) {
  if (rangeEnd <= rangeStart) {
    return 0.0f;
  }
  auto center = (textStart + textEnd) * 0.5f;
  return (center - rangeStart) / (rangeEnd - rangeStart);
}

float RampDownFactor(float textStart, float textEnd, float rangeStart, float rangeEnd) {
  if (rangeEnd <= rangeStart) {
    return 0.0f;
  }
  auto center = (textStart + textEnd) * 0.5f;
  return (rangeEnd - center) / (rangeEnd - rangeStart);
}

float RoundFactor(float textStart, float textEnd, float rangeStart, float rangeEnd) {
  if (rangeEnd <= rangeStart) {
    return 0.0f;
  }
  auto center = (textStart + textEnd) * 0.5f;
  if (center >= rangeEnd || center <= rangeStart) {
    return 0.0f;
  }
  auto radius = (rangeEnd - rangeStart) * 0.5f;
  auto distance = (rangeStart + rangeEnd) * 0.5f - center;
  auto height = std::sqrt(std::max(0.0f, radius * radius - distance * distance));
  return height / radius;
}

float SmoothFactor(float textStart, float textEnd, float rangeStart, float rangeEnd) {
  if (rangeEnd <= rangeStart) {
    return 0.0f;
  }
  auto center = (textStart + textEnd) * 0.5f;
  if (center >= rangeEnd || center <= rangeStart) {
    return 0.0f;
  }
  auto rangeCenter = (rangeStart + rangeEnd) * 0.5f;
  float x = 0.0f;
  if (center < rangeCenter) {
    x = (center - rangeStart) / (rangeCenter - rangeStart);
  } else {
    x = (rangeEnd - center) / (rangeEnd - rangeCenter);
  }
  // AE's smooth shape is a fixed cubic bezier; evaluating it by solving for t keeps easeIn/easeOut
  // free for the triangle shape.
  static pag::BezierEasing easing =
      pag::BezierEasing(pag::Point::Make(0.5f, 0.0f), pag::Point::Make(0.5f, 1.0f));
  return easing.getInterpolation(x);
}

std::vector<double> SolveCubic(double a, double b, double c, double d) {
  if (a == 0) {
    return {};
  }
  auto p = b * b - 3 * a * c;
  auto q = b * c - 9 * a * d;
  auto r = c * c - 3 * b * d;
  auto delta = q * q - 4 * p * r;
  std::vector<double> roots = {};
  if (p == 0 && q == 0) {
    roots.push_back(-b / (3 * a));
  } else if (delta == 0 && p != 0) {
    auto k = q / p;
    roots.push_back(-b / a + k);
    roots.push_back(-0.5 * k);
  } else if (delta > 0) {
    auto y1 = p * b + 1.5 * a * (-q + std::sqrt(delta));
    auto y2 = p * b + 1.5 * a * (-q - std::sqrt(delta));
    roots.push_back((-b - std::cbrt(y1) - std::cbrt(y2)) / (3 * a));
  } else if (delta < 0 && p > 0) {
    auto t = (p * b - 1.5 * a * q) / (p * std::sqrt(p));
    if (t > -1 && t < 1) {
      auto theta = std::acos(t);
      auto sqrtP = std::sqrt(p);
      auto cosA = std::cos(theta / 3);
      auto sinA = std::sin(theta / 3);
      roots.push_back((-b - 2 * sqrtP * cosA) / (3 * a));
      roots.push_back((-b + sqrtP * (cosA + std::sqrt(3.0) * sinA)) / (3 * a));
      roots.push_back((-b + sqrtP * (cosA - std::sqrt(3.0) * sinA)) / (3 * a));
    }
  }
  return roots;
}

float TriangleFactor(float textStart, float textEnd, float rangeStart, float rangeEnd, float easeIn,
                     float easeOut) {
  auto center = (textStart + textEnd) * 0.5f;
  if (center < rangeStart || center > rangeEnd) {
    return 0.0f;
  }
  auto rangeCenter = (rangeStart + rangeEnd) * 0.5f;
  double x1 = center <= rangeCenter ? rangeStart : rangeEnd;
  double step = rangeCenter - x1;
  double x2 = x1 + easeIn * step;
  double x3 = rangeCenter - easeOut * step;
  double x4 = rangeCenter;
  auto a = -x1 + 3 * x2 - 3 * x3 + x4;
  auto b = 3 * (x1 - 2 * x2 + x3);
  auto c = 3 * (-x1 + x2);
  auto d = x1 - center;
  double t = 0;
  for (auto root : SolveCubic(a, b, c, d)) {
    if ((root >= 0 && root <= 1) || std::fabs(root - 1.0) < 1e-6) {
      t = root;
      break;
    }
  }
  return static_cast<float>(3 * (1 - t) * std::pow(t, 2) + std::pow(t, 3));
}

float OverlayFactor(float current, float factor, SelectorMode mode) {
  switch (mode) {
    case SelectorMode::Subtract:
      return factor >= 0.0f ? current * (1.0f - factor) : current * (-1.0f - factor);
    case SelectorMode::Intersect:
      return current * factor;
    case SelectorMode::Min:
      return std::min(current, factor);
    case SelectorMode::Max:
      return std::max(current, factor);
    case SelectorMode::Difference:
      return std::fabs(current - factor);
    case SelectorMode::Add:
    default:
      return current + factor;
  }
}

// AE only randomizes the order of the first character deterministically: for a given character
// count the first slot always maps to a fixed index. The table below reproduces that mapping, which
// the legacy PAG renderer also relies on to keep the first baseline stable.
static const struct {
  int end;
  int index;
} kFirstRandomRanges[] = {{2, 0},   {4, 2},    {5, 4},     {20, 5},
                          {69, 20}, {127, 69}, {206, 127}, {10000, 205}};

static int FirstRandomIndex(size_t count) {
  for (const auto& range : kFirstRandomRanges) {
    if (static_cast<int>(count) <= range.end) {
      return range.index;
    }
  }
  return 0;
}

uint32_t NextRandom(uint32_t* state) {
  *state = *state * 1664525u + 1013904223u;
  return *state >> 8;
}

}  // namespace

TextSelectorEvaluator::TextSelectorEvaluator(const std::vector<TextSelector*>& selectors,
                                             size_t count)
    : _count(count) {
  for (auto* selector : selectors) {
    if (selector == nullptr || selector->nodeType() != NodeType::RangeSelector) {
      continue;
    }
    auto* range = static_cast<const RangeSelector*>(selector);
    Range state = {};
    state.selector = selector;
    // Both ends stay in the selector's own unit: Percentage ranges live in [0, 1] while Index
    // ranges address characters directly, and evaluate() builds the matching character domain.
    state.start = range->start + range->offset;
    state.end = range->end + range->offset;
    if (state.start > state.end) {
      std::swap(state.start, state.end);
    }
    if (range->randomOrder && count > 1) {
      state.order.resize(count);
      for (size_t i = 0; i < count; ++i) {
        state.order[i] = i;
      }
      // A private LCG keeps the order deterministic across platforms; the AE seed table cannot be
      // reproduced here because it depends on the host C runtime's rand().
      uint32_t seed = static_cast<uint32_t>(range->randomSeed) ^ 0x9E3779B9u;
      for (size_t i = count; i > 1; --i) {
        auto j = static_cast<size_t>(NextRandom(&seed) % i);
        std::swap(state.order[i - 1], state.order[j]);
      }
      if (range->randomSeed == 0 && count > 1) {
        auto target = static_cast<size_t>(FirstRandomIndex(count));
        for (size_t i = 0; i < count; ++i) {
          if (state.order[i] == target) {
            std::swap(state.order[0], state.order[i]);
            break;
          }
        }
      }
    }
    _ranges.push_back(std::move(state));
  }
}

float TextSelectorEvaluator::evaluate(const Range& range, size_t index) const {
  auto* selector = static_cast<const RangeSelector*>(range.selector);
  auto position = index;
  if (!range.order.empty()) {
    position = range.order[index];
  }
  float textStart = 0.0f;
  float textEnd = 0.0f;
  if (selector->unit == SelectorUnit::Percentage) {
    auto scale = static_cast<float>(_count);
    textStart = static_cast<float>(position) / scale;
    textEnd = static_cast<float>(position + 1) / scale;
  } else {
    textStart = static_cast<float>(position);
    textEnd = static_cast<float>(position + 1);
  }
  float factor = 0.0f;
  switch (selector->shape) {
    case SelectorShape::RampUp:
      factor = RampUpFactor(textStart, textEnd, range.start, range.end);
      break;
    case SelectorShape::RampDown:
      factor = RampDownFactor(textStart, textEnd, range.start, range.end);
      break;
    case SelectorShape::Triangle:
      factor = TriangleFactor(textStart, textEnd, range.start, range.end, Clamp01(selector->easeIn),
                              Clamp01(selector->easeOut));
      break;
    case SelectorShape::Round:
      factor = RoundFactor(textStart, textEnd, range.start, range.end);
      break;
    case SelectorShape::Smooth:
      factor = SmoothFactor(textStart, textEnd, range.start, range.end);
      break;
    case SelectorShape::Square:
    default:
      factor = SquareFactor(textStart, textEnd, range.start, range.end);
      break;
  }
  return Clamp01(factor) * Clamp01(selector->weight);
}

float TextSelectorEvaluator::weight(size_t index) const {
  if (_count == 0 || index >= _count) {
    return 0.0f;
  }
  if (_ranges.empty()) {
    return 1.0f;
  }
  float total = 1.0f;
  bool first = true;
  for (const auto& range : _ranges) {
    auto* selector = static_cast<const RangeSelector*>(range.selector);
    auto factor = evaluate(range, index);
    if (first && selector->mode != SelectorMode::Subtract) {
      total = factor;
    } else {
      total = OverlayFactor(total, factor, selector->mode);
    }
    total = ClampFactor(total);
    first = false;
  }
  return total;
}

}  // namespace pagx
