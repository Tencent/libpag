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

#pragma once

#include "core/Color4f.h"
#include "gpu/FragmentProcessor.h"

namespace pag {
class UnrolledBinaryGradientColorizer : public FragmentProcessor {
 public:
  static constexpr int kMaxColorCount = 16;
  static std::unique_ptr<UnrolledBinaryGradientColorizer> Make(const Color4f* colors,
                                                               const float* positions, int count);

  std::string name() const override {
    return "UnrolledBinaryGradientColorizer";
  }

  void onComputeProcessorKey(BytesKey* bytesKey) const override;

  std::unique_ptr<GLFragmentProcessor> onCreateGLInstance() const override;

 private:
  UnrolledBinaryGradientColorizer(int intervalCount, Color4f* scales, Color4f* biases,
                                  Rect thresholds1_7, Rect thresholds9_13)
      : intervalCount(intervalCount),
        scale0_1(scales[0]),
        scale2_3(scales[1]),
        scale4_5(scales[2]),
        scale6_7(scales[3]),
        scale8_9(scales[4]),
        scale10_11(scales[5]),
        scale12_13(scales[6]),
        scale14_15(scales[7]),
        bias0_1(biases[0]),
        bias2_3(biases[1]),
        bias4_5(biases[2]),
        bias6_7(biases[3]),
        bias8_9(biases[4]),
        bias10_11(biases[5]),
        bias12_13(biases[6]),
        bias14_15(biases[7]),
        thresholds1_7(thresholds1_7),
        thresholds9_13(thresholds9_13) {
  }

  int intervalCount;
  Color4f scale0_1;
  Color4f scale2_3;
  Color4f scale4_5;
  Color4f scale6_7;
  Color4f scale8_9;
  Color4f scale10_11;
  Color4f scale12_13;
  Color4f scale14_15;
  Color4f bias0_1;
  Color4f bias2_3;
  Color4f bias4_5;
  Color4f bias6_7;
  Color4f bias8_9;
  Color4f bias10_11;
  Color4f bias12_13;
  Color4f bias14_15;
  Rect thresholds1_7;
  Rect thresholds9_13;

  friend class GLUnrolledBinaryGradientColorizer;
};
}  // namespace pag
