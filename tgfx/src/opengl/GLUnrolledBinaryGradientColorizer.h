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
#include "gpu/gradients/UnrolledBinaryGradientColorizer.h"

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
class GLUnrolledBinaryGradientColorizer : public UnrolledBinaryGradientColorizer {
 public:
  GLUnrolledBinaryGradientColorizer(int intervalCount, Color* scales, Color* biases,
                                    Rect thresholds1_7, Rect thresholds9_13);

  void emitCode(EmitArgs& args) const override;

 private:
  void onSetData(UniformBuffer*) const override;
};
}  // namespace tgfx
