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

#include <optional>
#include "gpu/GLFragmentProcessor.h"

namespace tgfx {
/**
 * The 7 threshold positions that define the boundaries of the 8 intervals (excluding t = 0, and t =
 * 1) are packed into two vec4's instead of having up to 7 separate scalar uniforms. For low
 * interval counts, the extra components are ignored in the shader, but the uniform simplification
 * is worth it. It is assumed thresholds are provided in increasing value, mapped as:
 *  - thresholds1_7.x = boundary between (0,1) and (2,3) -> 1_2
 *  -              .y = boundary between (2,3) and (4,5) -> 3_4
 *  -              .z = boundary between (4,5) and (6,7) -> 5_6
 *  -              .w = boundary between (6,7) and (8,9) -> 7_8
 *  - thresholds9_13.x = boundary between (8,9) and (10,11) -> 9_10
 *  -               .y = boundary between (10,11) and (12,13) -> 11_12
 *  -               .z = boundary between (12,13) and (14,15) -> 13_14
 *  -               .w = unused
 */
class GLUnrolledBinaryGradientColorizer : public GLFragmentProcessor {
 public:
  void emitCode(EmitArgs& args) override;

 private:
  void onSetData(const ProgramDataManager&, const FragmentProcessor&) override;

  UniformHandle scale0_1Uniform;
  UniformHandle scale2_3Uniform;
  UniformHandle scale4_5Uniform;
  UniformHandle scale6_7Uniform;
  UniformHandle scale8_9Uniform;
  UniformHandle scale10_11Uniform;
  UniformHandle scale12_13Uniform;
  UniformHandle scale14_15Uniform;
  UniformHandle bias0_1Uniform;
  UniformHandle bias2_3Uniform;
  UniformHandle bias4_5Uniform;
  UniformHandle bias6_7Uniform;
  UniformHandle bias8_9Uniform;
  UniformHandle bias10_11Uniform;
  UniformHandle bias12_13Uniform;
  UniformHandle bias14_15Uniform;
  UniformHandle thresholds1_7Uniform;
  UniformHandle thresholds9_13Uniform;

  std::optional<Color> scale0_1Prev;
  std::optional<Color> scale2_3Prev;
  std::optional<Color> scale4_5Prev;
  std::optional<Color> scale6_7Prev;
  std::optional<Color> scale8_9Prev;
  std::optional<Color> scale10_11Prev;
  std::optional<Color> scale12_13Prev;
  std::optional<Color> scale14_15Prev;
  std::optional<Color> bias0_1Prev;
  std::optional<Color> bias2_3Prev;
  std::optional<Color> bias4_5Prev;
  std::optional<Color> bias6_7Prev;
  std::optional<Color> bias8_9Prev;
  std::optional<Color> bias10_11Prev;
  std::optional<Color> bias12_13Prev;
  std::optional<Color> bias14_15Prev;
  std::optional<Rect> thresholds1_7Prev;
  std::optional<Rect> thresholds9_13Prev;
};
}  // namespace tgfx
