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

#include "tgfx/gpu/ImageFilter.h"

namespace tgfx {
bool ImageFilter::applyCropRect(const Rect& srcRect, Rect* dstRect, const Rect* clipRect) const {
  *dstRect = onFilterNodeBounds(srcRect);
  if (!cropRect.isEmpty()) {
    dstRect->intersect(cropRect);
  }
  if (clipRect) {
    return dstRect->intersect(*clipRect);
  }
  return true;
}

Rect ImageFilter::filterBounds(const Rect& rect) const {
  auto dstBounds = Rect::MakeEmpty();
  applyCropRect(rect, &dstBounds);
  return dstBounds;
}

Rect ImageFilter::onFilterNodeBounds(const Rect& srcRect) const {
  return srcRect;
}
}  // namespace tgfx
