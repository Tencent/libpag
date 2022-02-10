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
class DualIntervalGradientColorizer : public FragmentProcessor {
 public:
  static std::unique_ptr<DualIntervalGradientColorizer> Make(Color4f c0, Color4f c1, Color4f c2,
                                                             Color4f c3, float threshold);

  std::string name() const override {
    return "DualIntervalGradientColorizer";
  }

  void onComputeProcessorKey(BytesKey* bytesKey) const override;

  std::unique_ptr<GLFragmentProcessor> onCreateGLInstance() const override;

 private:
  DualIntervalGradientColorizer(Color4f scale01, Color4f bias01, Color4f scale23, Color4f bias23,
                                float threshold)
      : scale01(scale01), bias01(bias01), scale23(scale23), bias23(bias23), threshold(threshold) {
  }

  Color4f scale01;
  Color4f bias01;
  Color4f scale23;
  Color4f bias23;
  float threshold;

  friend class GLDualIntervalGradientColorizer;
};
}  // namespace pag
