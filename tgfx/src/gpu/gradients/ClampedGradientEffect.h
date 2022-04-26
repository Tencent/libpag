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

#include <climits>
#include "gpu/FragmentProcessor.h"
#include "tgfx/core/Color.h"

namespace tgfx {
class ClampedGradientEffect : public FragmentProcessor {
 public:
  static std::unique_ptr<ClampedGradientEffect> Make(std::unique_ptr<FragmentProcessor> colorizer,
                                                     std::unique_ptr<FragmentProcessor> gradLayout,
                                                     Color leftBorderColor, Color rightBorderColor,
                                                     bool makePremultiply);

  std::string name() const override {
    return "ClampedGradientEffect";
  }

  void onComputeProcessorKey(BytesKey* bytesKey) const override;

  std::unique_ptr<GLFragmentProcessor> onCreateGLInstance() const override;

 private:
  ClampedGradientEffect(std::unique_ptr<FragmentProcessor> colorizer,
                        std::unique_ptr<FragmentProcessor> gradLayout, Color leftBorderColor,
                        Color rightBorderColor, bool makePremultiplied);

  size_t colorizerIndex = ULONG_MAX;
  size_t gradLayoutIndex = ULONG_MAX;
  Color leftBorderColor;
  Color rightBorderColor;
  bool makePremultiply;

  friend class GLClampedGradientEffect;
};
}  // namespace tgfx
