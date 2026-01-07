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

#include "HardwareBufferUtil.h"

namespace pag {

tgfx::ColorType HardwareBufferFormatToColorType(tgfx::HardwareBufferFormat format) {
  switch (format) {
    case tgfx::HardwareBufferFormat::RGBA_8888:
      return tgfx::ColorType::RGBA_8888;
    case tgfx::HardwareBufferFormat::BGRA_8888:
      return tgfx::ColorType::BGRA_8888;
    case tgfx::HardwareBufferFormat::ALPHA_8:
      return tgfx::ColorType::ALPHA_8;
    default:
      return tgfx::ColorType::Unknown;
  }
}

tgfx::ImageInfo HardwareBufferInfoToImageInfo(const tgfx::HardwareBufferInfo& hwInfo) {
  auto colorType = HardwareBufferFormatToColorType(hwInfo.format);
  return tgfx::ImageInfo::Make(hwInfo.width, hwInfo.height, colorType,
                               tgfx::AlphaType::Premultiplied, hwInfo.rowBytes);
}

}  // namespace pag
