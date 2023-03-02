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

#include "AHardwareBufferUtil.h"
#include "HardwareBufferInterface.h"

namespace tgfx {
static constexpr int HARDWAREBUFFER_FORMAT_R8_UNORM = 0x38;

ImageInfo GetImageInfo(AHardwareBuffer* hardwareBuffer) {
  AHardwareBuffer_Desc desc;
  HardwareBufferInterface::Describe(hardwareBuffer, &desc);
  auto colorType = ColorType::RGBA_8888;
  auto alphaType = AlphaType::Premultiplied;
  switch (desc.format) {
    case HARDWAREBUFFER_FORMAT_R8_UNORM:
      colorType = ColorType::ALPHA_8;
      break;
    case AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM:
      colorType = ColorType::RGBA_8888;
      break;
    case AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM:
      colorType = ColorType::RGBA_8888;
      alphaType = AlphaType::Opaque;
      break;
    case AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM:
      colorType = ColorType::RGB_565;
      alphaType = AlphaType::Opaque;
      break;
    case AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM:
      colorType = ColorType::RGBA_1010102;
      break;
    case AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT:
      colorType = ColorType::RGBA_F16;
      break;
    default:
      break;
  }
  auto bytesPerPixel = ImageInfo::GetBytesPerPixel(colorType);
  return ImageInfo::Make(desc.width, desc.height, colorType, alphaType,
                         desc.stride * bytesPerPixel);
}
}  // namespace tgfx