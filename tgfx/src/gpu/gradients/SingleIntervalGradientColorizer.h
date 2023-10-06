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

#include "gpu/FragmentProcessor.h"
#include "tgfx/core/Color.h"

namespace tgfx {
class SingleIntervalGradientColorizer : public FragmentProcessor {
 public:
  static std::unique_ptr<SingleIntervalGradientColorizer> Make(Color start, Color end);

  std::string name() const override {
    return "SingleIntervalGradientColorizer";
  }

 protected:
  DEFINE_PROCESSOR_CLASS_ID

  SingleIntervalGradientColorizer(Color start, Color end)
      : FragmentProcessor(ClassID()), start(start), end(end) {
  }

  bool onIsEqual(const FragmentProcessor& processor) const override;

  Color start;
  Color end;
};
}  // namespace tgfx
