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

#include "FragmentProcessor.h"

namespace tgfx {
enum class DualBlurPassMode { Up = 0, Down = 1 };

class DualBlurFragmentProcessor : public FragmentProcessor {
 public:
  static std::unique_ptr<DualBlurFragmentProcessor> Make(DualBlurPassMode passMode,
                                                         std::unique_ptr<FragmentProcessor> texture,
                                                         Point blurOffset, Point texelSize);

  std::string name() const override {
    return "DualBlurFragmentProcessor";
  }

  void onComputeProcessorKey(BytesKey* bytesKey) const override;

  std::unique_ptr<GLFragmentProcessor> onCreateGLInstance() const override;

 private:
  DualBlurFragmentProcessor(DualBlurPassMode passMode, std::unique_ptr<FragmentProcessor> texture,
                            Point blurOffset, Point texelSize);

  DualBlurPassMode passMode;
  Point blurOffset;
  Point texelSize;

  friend class GLDualBlurFragmentProcessor;
};
}  // namespace tgfx
