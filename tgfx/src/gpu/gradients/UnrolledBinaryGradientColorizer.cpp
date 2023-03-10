/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "UnrolledBinaryGradientColorizer.h"
#include "opengl/GLUnrolledBinaryGradientColorizer.h"
#include "utils/MathExtra.h"

namespace tgfx {
static constexpr int kMaxIntervals = 8;
std::unique_ptr<UnrolledBinaryGradientColorizer> UnrolledBinaryGradientColorizer::Make(
    const Color* colors, const float* positions, int count) {
  // Depending on how the positions resolve into hard stops or regular stops, the number of
  // intervals specified by the number of colors/positions can change. For instance, a plain
  // 3 color gradient is two intervals, but a 4 color gradient with a hard stop is also
  // two intervals. At the most extreme end, an 8 interval gradient made entirely of hard
  // stops has 16 colors.

  if (count > kMaxColorCount) {
    // Definitely cannot represent this gradient configuration
    return nullptr;
  }

  // The raster implementation also uses scales and biases, but since they must be calculated
  // after the dst color space is applied, it limits our ability to cache their values.
  Color scales[kMaxIntervals];
  Color biases[kMaxIntervals];
  float thresholds[kMaxIntervals];

  int intervalCount = 0;

  for (int i = 0; i < count - 1; i++) {
    if (intervalCount >= kMaxIntervals) {
      // Already reached kMaxIntervals, and haven't run out of color stops so this
      // gradient cannot be represented by this shader.
      return nullptr;
    }

    auto t0 = positions[i];
    auto t1 = positions[i + 1];
    auto dt = t1 - t0;
    // If the interval is empty, skip to the next interval. This will automatically create
    // distinct hard stop intervals as needed. It also protects against malformed gradients
    // that have repeated hard stops at the very beginning that are effectively unreachable.
    if (FloatNearlyZero(dt)) {
      continue;
    }

    for (int j = 0; j < 4; ++j) {
      auto c0 = colors[i][j];
      auto c1 = colors[i + 1][j];
      auto scale = (c1 - c0) / dt;
      auto bias = c0 - t0 * scale;
      scales[intervalCount][j] = scale;
      biases[intervalCount][j] = bias;
    }
    thresholds[intervalCount] = t1;
    intervalCount++;
  }

  // set the unused values to something consistent
  for (int i = intervalCount; i < kMaxIntervals; i++) {
    scales[i] = Color::Transparent();
    biases[i] = Color::Transparent();
    thresholds[i] = 0.0;
  }

  return std::unique_ptr<UnrolledBinaryGradientColorizer>(new UnrolledBinaryGradientColorizer(
      intervalCount, scales, biases,
      Rect::MakeLTRB(thresholds[0], thresholds[1], thresholds[2], thresholds[3]),
      Rect::MakeLTRB(thresholds[4], thresholds[5], thresholds[6], 0.0)));
}

void UnrolledBinaryGradientColorizer::onComputeProcessorKey(BytesKey* bytesKey) const {
  bytesKey->write(static_cast<uint32_t>(intervalCount));
}

bool UnrolledBinaryGradientColorizer::onIsEqual(const FragmentProcessor& processor) const {
  const auto& that = static_cast<const UnrolledBinaryGradientColorizer&>(processor);
  return intervalCount == that.intervalCount && scale0_1 == that.scale0_1 &&
         scale2_3 == that.scale2_3 && scale4_5 == that.scale4_5 && scale6_7 == that.scale6_7 &&
         scale8_9 == that.scale8_9 && scale10_11 == that.scale10_11 &&
         scale12_13 == that.scale12_13 && scale14_15 == that.scale14_15 &&
         bias0_1 == that.bias0_1 && bias2_3 == that.bias2_3 && bias4_5 == that.bias4_5 &&
         bias6_7 == that.bias6_7 && bias8_9 == that.bias8_9 && bias10_11 == that.bias10_11 &&
         bias12_13 == that.bias12_13 && bias14_15 == that.bias14_15 &&
         thresholds1_7 == that.thresholds1_7 && thresholds9_13 == that.thresholds9_13;
}

std::unique_ptr<GLFragmentProcessor> UnrolledBinaryGradientColorizer::onCreateGLInstance() const {
  return std::make_unique<GLUnrolledBinaryGradientColorizer>();
}
}  // namespace tgfx
