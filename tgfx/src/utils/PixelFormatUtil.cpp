/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "PixelFormatUtil.h"

namespace tgfx {
PixelFormat ColorTypeToPixelFormat(ColorType type) {
  switch (type) {
    case ColorType::ALPHA_8:
      return PixelFormat::ALPHA_8;
    case ColorType::RGBA_8888:
      return PixelFormat::RGBA_8888;
    case ColorType::BGRA_8888:
      return PixelFormat::BGRA_8888;
    default:
      return PixelFormat::RGBA_8888;
  }
}

size_t PixelFormatBytesPerPixel(PixelFormat format) {
  switch (format) {
    case PixelFormat::ALPHA_8:
    case PixelFormat::GRAY_8:
      return 1;
    case PixelFormat::RG_88:
      return 2;
    case PixelFormat::RGBA_8888:
    case PixelFormat::BGRA_8888:
      return 4;
    default:
      return 0;
  }
}
}  // namespace tgfx
