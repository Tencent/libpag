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

#include "gpu/processors/FragmentProcessor.h"
#include "tgfx/core/Color.h"

namespace tgfx {
class DualIntervalGradientColorizer : public FragmentProcessor {
 public:
  static std::unique_ptr<DualIntervalGradientColorizer> Make(Color c0, Color c1, Color c2, Color c3,
                                                             float threshold);

  std::string name() const override {
    return "DualIntervalGradientColorizer";
  }

 protected:
  DEFINE_PROCESSOR_CLASS_ID

  DualIntervalGradientColorizer(Color scale01, Color bias01, Color scale23, Color bias23,
                                float threshold)
      : FragmentProcessor(ClassID()),
        scale01(scale01),
        bias01(bias01),
        scale23(scale23),
        bias23(bias23),
        threshold(threshold) {
  }

  bool onIsEqual(const FragmentProcessor& processor) const override;

  Color scale01;
  Color bias01;
  Color scale23;
  Color bias23;
  float threshold;
};
}  // namespace tgfx
