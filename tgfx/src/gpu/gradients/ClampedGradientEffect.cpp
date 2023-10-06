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

namespace tgfx {
void ClampedGradientEffect::onComputeProcessorKey(BytesKey* bytesKey) const {
  uint32_t flag = makePremultiply ? 1 : 0;
  bytesKey->write(flag);
}

bool ClampedGradientEffect::onIsEqual(const FragmentProcessor& processor) const {
  const auto& that = static_cast<const ClampedGradientEffect&>(processor);
  return makePremultiply == that.makePremultiply && leftBorderColor == that.leftBorderColor &&
         rightBorderColor == that.rightBorderColor;
}

ClampedGradientEffect::ClampedGradientEffect(std::unique_ptr<FragmentProcessor> colorizer,
                                             std::unique_ptr<FragmentProcessor> gradLayout,
                                             Color leftBorderColor, Color rightBorderColor,
                                             bool makePremultiplied)
    : FragmentProcessor(ClassID()),
      leftBorderColor(leftBorderColor),
      rightBorderColor(rightBorderColor),
      makePremultiply(makePremultiplied) {
  colorizerIndex = registerChildProcessor(std::move(colorizer));
  gradLayoutIndex = registerChildProcessor(std::move(gradLayout));
}
}  // namespace tgfx
