/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#include "pagx/utils/ColorSpaceUtils.h"
#include "tgfx/core/ColorSpace.h"

namespace pagx {

static const tgfx::ColorSpace* TGFXColorSpace(ColorSpace space) {
  if (space == ColorSpace::DisplayP3) {
    return tgfx::ColorSpace::DisplayP3().get();
  }
  return tgfx::ColorSpace::SRGB().get();
}

Color ConvertColorSpace(const Color& color, ColorSpace target) {
  if (color.colorSpace == target) {
    return color;
  }
  tgfx::ColorMatrix33 matrix = {};
  TGFXColorSpace(color.colorSpace)->gamutTransformTo(TGFXColorSpace(target), &matrix);
  Color result = color;
  result.red = color.red * matrix.values[0][0] + color.green * matrix.values[0][1] +
               color.blue * matrix.values[0][2];
  result.green = color.red * matrix.values[1][0] + color.green * matrix.values[1][1] +
                 color.blue * matrix.values[1][2];
  result.blue = color.red * matrix.values[2][0] + color.green * matrix.values[2][1] +
                color.blue * matrix.values[2][2];
  result.colorSpace = target;
  return result;
}

}  // namespace pagx
