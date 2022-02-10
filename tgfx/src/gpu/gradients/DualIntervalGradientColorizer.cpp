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

#include "DualIntervalGradientColorizer.h"
#include "gpu/opengl/GLDualIntervalGradientColorizer.h"

namespace pag {
std::unique_ptr<DualIntervalGradientColorizer> DualIntervalGradientColorizer::Make(
    Color4f c0, Color4f c1, Color4f c2, Color4f c3, float threshold) {
  Color4f scale01;
  // Derive scale and biases from the 4 colors and threshold
  for (int i = 0; i < 4; ++i) {
    auto vc0 = c0[i];
    auto vc1 = c1[i];
    scale01[i] = (vc1 - vc0) / threshold;
  }
  // bias01 = c0

  Color4f scale23;
  Color4f bias23;
  for (int i = 0; i < 4; ++i) {
    auto vc2 = c2[i];
    auto vc3 = c3[i];
    scale23[i] = (vc3 - vc2) / (1 - threshold);
    bias23[i] = vc2 - threshold * scale23[i];
  }

  return std::unique_ptr<DualIntervalGradientColorizer>(
      new DualIntervalGradientColorizer(scale01, c0, scale23, bias23, threshold));
}

void DualIntervalGradientColorizer::onComputeProcessorKey(BytesKey* bytesKey) const {
  static auto Type = UniqueID::Next();
  bytesKey->write(Type);
}

std::unique_ptr<GLFragmentProcessor> DualIntervalGradientColorizer::onCreateGLInstance() const {
  return std::make_unique<GLDualIntervalGradientColorizer>();
}
}  // namespace pag
