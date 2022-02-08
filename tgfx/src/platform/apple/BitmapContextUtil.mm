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

#include "BitmapContextUtil.h"

namespace pag {
static uint32_t GetBitmapInfo(AlphaType alphaType, ColorType colorType) {
  CGBitmapInfo bitmapInfo = 0;
  auto premultiplied = alphaType == AlphaType::Premultiplied;
  CGImageAlphaInfo alphaInfo = kCGImageAlphaNone;
  switch (colorType) {
    case ColorType::BGRA_8888:
      bitmapInfo = kCGBitmapByteOrder32Little;
      alphaInfo = premultiplied ? kCGImageAlphaPremultipliedFirst : kCGImageAlphaFirst;
      break;
    case ColorType::RGBA_8888:
      bitmapInfo = kCGBitmapByteOrder32Big;
      alphaInfo = premultiplied ? kCGImageAlphaPremultipliedLast : kCGImageAlphaLast;
      break;
    case ColorType::ALPHA_8:
      alphaInfo = kCGImageAlphaOnly;
      break;
    default:
      break;
  }
  return static_cast<uint32_t>(bitmapInfo) | static_cast<uint32_t>(alphaInfo);
}

CGContextRef CreateBitmapContext(const ImageInfo& info, void* pixels) {
  if (pixels == nullptr) {
    return nullptr;
  }
  auto bitmapInfo = GetBitmapInfo(info.alphaType(), info.colorType());
  if (bitmapInfo == 0) {
    return nullptr;
  }
  CGColorSpaceRef colorspace =
      info.colorType() == ColorType::ALPHA_8 ? nullptr : CGColorSpaceCreateDeviceRGB();
  auto cgContext = CGBitmapContextCreate(pixels, info.width(), info.height(), 8, info.rowBytes(),
                                         colorspace, bitmapInfo);
  if (colorspace) {
    CFRelease(colorspace);
  }
  return cgContext;
}
}  // namespace pag