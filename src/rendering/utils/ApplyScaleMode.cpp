/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "ApplyScaleMode.h"

namespace pag {
Matrix ApplyScaleMode(PAGScaleMode scaleMode, int sourceWidth, int sourceHeight, int targetWidth,
                      int targetHeight) {
  Matrix matrix = {};
  matrix.setIdentity();
  if (scaleMode == PAGScaleMode::None || sourceWidth <= 0 || sourceHeight <= 0 ||
      targetWidth <= 0 || targetHeight <= 0) {
    return matrix;
  }
  auto scaleX = targetWidth * 1.0 / sourceWidth;
  auto scaleY = targetHeight * 1.0 / sourceHeight;
  switch (scaleMode) {
    case PAGScaleMode::Stretch: {
      matrix.setScale(scaleX, scaleY);
    } break;
    case PAGScaleMode::Zoom: {
      auto scale = std::max(scaleX, scaleY);
      matrix.setScale(scale, scale);
      if (scaleX > scaleY) {
        matrix.postTranslate(0, (targetHeight - sourceHeight * scale) * 0.5f);
      } else {
        matrix.postTranslate((targetWidth - sourceWidth * scale) * 0.5f, 0);
      }
    } break;
    default: {
      auto scale = std::min(scaleX, scaleY);
      matrix.setScale(scale, scale);
      if (scaleX < scaleY) {
        matrix.postTranslate(0, (targetHeight - sourceHeight * scale) * 0.5f);
      } else {
        matrix.postTranslate((targetWidth - sourceWidth * scale) * 0.5f, 0);
      }
    } break;
  }
  return matrix;
}
}  // namespace pag
