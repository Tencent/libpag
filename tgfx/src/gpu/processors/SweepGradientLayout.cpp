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

#include "SweepGradientLayout.h"

namespace tgfx {
SweepGradientLayout::SweepGradientLayout(Matrix matrix, float bias, float scale)
    : FragmentProcessor(ClassID()), coordTransform(matrix), bias(bias), scale(scale) {
  addCoordTransform(&coordTransform);
}

bool SweepGradientLayout::onIsEqual(const FragmentProcessor& processor) const {
  const auto& that = static_cast<const SweepGradientLayout&>(processor);
  return coordTransform.matrix == that.coordTransform.matrix && bias == that.bias &&
         scale == that.scale;
}
}  // namespace tgfx
