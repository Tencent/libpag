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

#include "ClampedGradientEffect.h"
#include "core/utils/UniqueID.h"
#include "gpu/opengl/GLClampedGradientEffect.h"

namespace tgfx {
std::unique_ptr<ClampedGradientEffect> ClampedGradientEffect::Make(
    std::unique_ptr<FragmentProcessor> colorizer, std::unique_ptr<FragmentProcessor> gradLayout,
    Color leftBorderColor, Color rightBorderColor, bool makePremultiply) {
  return std::unique_ptr<ClampedGradientEffect>(
      new ClampedGradientEffect(std::move(colorizer), std::move(gradLayout), leftBorderColor,
                                rightBorderColor, makePremultiply));
}

void ClampedGradientEffect::onComputeProcessorKey(BytesKey* bytesKey) const {
  static auto Type = UniqueID::Next();
  bytesKey->write(Type);
  uint32_t flag = makePremultiply ? 1 : 0;
  bytesKey->write(flag);
}

std::unique_ptr<GLFragmentProcessor> ClampedGradientEffect::onCreateGLInstance() const {
  return std::make_unique<GLClampedGradientEffect>();
}

ClampedGradientEffect::ClampedGradientEffect(std::unique_ptr<FragmentProcessor> colorizer,
                                             std::unique_ptr<FragmentProcessor> gradLayout,
                                             Color leftBorderColor, Color rightBorderColor,
                                             bool makePremultiplied)
    : leftBorderColor(leftBorderColor),
      rightBorderColor(rightBorderColor),
      makePremultiply(makePremultiplied) {
  colorizerIndex = registerChildProcessor(std::move(colorizer));
  gradLayoutIndex = registerChildProcessor(std::move(gradLayout));
}
}  // namespace tgfx
