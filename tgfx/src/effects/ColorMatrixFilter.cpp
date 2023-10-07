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

#include "ColorMatrixFilter.h"
#include "gpu/processors/ColorMatrixFragmentProcessor.h"
#include "utils/MathExtra.h"

namespace tgfx {
std::shared_ptr<ColorFilter> ColorFilter::Matrix(const std::array<float, 20>& rowMajor) {
  return std::make_shared<ColorMatrixFilter>(rowMajor);
}

static bool IsAlphaUnchanged(const float matrix[20]) {
  const float* srcA = matrix + 15;
  return FloatNearlyZero(srcA[0]) && FloatNearlyZero(srcA[1]) && FloatNearlyZero(srcA[2]) &&
         FloatNearlyEqual(srcA[3], 1) && FloatNearlyZero(srcA[4]);
}

ColorMatrixFilter::ColorMatrixFilter(const std::array<float, 20>& matrix)
    : matrix(matrix), alphaIsUnchanged(IsAlphaUnchanged(matrix.data())) {
}

std::unique_ptr<FragmentProcessor> ColorMatrixFilter::asFragmentProcessor() const {
  return ColorMatrixFragmentProcessor::Make(matrix);
}
}  // namespace tgfx
