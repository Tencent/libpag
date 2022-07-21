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

#include "tgfx/gpu/ImageFilter.h"

namespace tgfx {
class DropShadowImageFilter : public ImageFilter {
 public:
  DropShadowImageFilter(float dx, float dy, float blurrinessX, float blurrinessY,
                        const Color& color, bool shadowOnly, const Rect& cropRect)
      : ImageFilter(cropRect),
        dx(dx),
        dy(dy),
        blurrinessX(blurrinessX),
        blurrinessY(blurrinessY),
        color(color),
        shadowOnly(shadowOnly) {
  }

  std::pair<std::shared_ptr<Texture>, Point> filterImage(
      const ImageFilterContext& context) override;

 private:
  Rect onFilterNodeBounds(const Rect& srcRect) const override;

  float dx;
  float dy;
  float blurrinessX;
  float blurrinessY;
  Color color;
  bool shadowOnly;
};
}  // namespace tgfx
