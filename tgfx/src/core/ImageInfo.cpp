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

#include "tgfx/core/ImageInfo.h"

namespace tgfx {
static constexpr int MaxDimension = INT32_MAX >> 2;

bool ImageInfo::IsValidSize(int width, int height) {
  return width > 0 && width <= MaxDimension && height > 0 && height <= MaxDimension;
}

int ImageInfo::GetBytesPerPixel(ColorType colorType) {
  switch (colorType) {
    case ColorType::ALPHA_8:
      return 1;
    case ColorType::RGBA_8888:
    case ColorType::BGRA_8888:
      return 4;
    default:
      return 0;
  }
}

ImageInfo ImageInfo::Make(int width, int height, ColorType colorType, AlphaType alphaType,
                          size_t rowBytes) {
  static ImageInfo emptyInfo = {};
  if (colorType == ColorType::Unknown || alphaType == AlphaType::Unknown ||
      !IsValidSize(width, height)) {
    return emptyInfo;
  }
  auto bytesPerPixel = GetBytesPerPixel(colorType);
  if (rowBytes > 0) {
    if (rowBytes < static_cast<size_t>(width) * bytesPerPixel) {
      return emptyInfo;
    }
  } else {
    rowBytes = width * bytesPerPixel;
  }
  if (colorType == ColorType::ALPHA_8) {
    alphaType = AlphaType::Premultiplied;
  }
  return {width, height, colorType, alphaType, rowBytes};
}

int ImageInfo::bytesPerPixel() const {
  return GetBytesPerPixel(_colorType);
}

ImageInfo ImageInfo::makeIntersect(int x, int y, int targetWidth, int targetHeight) const {
  int left = std::max(0, x);
  int right = std::min(_width, x + targetWidth);
  int top = std::max(0, y);
  int bottom = std::min(_height, y + targetHeight);
  return makeWH(right - left, bottom - top);
}

static int Clamp(int value, int max) {
  if (value < 0) {
    value = 0;
  }
  if (value > max) {
    value = max;
  }
  return value;
}

const void* ImageInfo::computeOffset(const void* pixels, int x, int y) const {
  if (pixels == nullptr || isEmpty()) {
    return pixels;
  }
  x = Clamp(x, _width - 1);
  y = Clamp(y, _height - 1);
  auto offset = y * _rowBytes + x * bytesPerPixel();
  return reinterpret_cast<const uint8_t*>(pixels) + offset;
}

void* ImageInfo::computeOffset(void* pixels, int x, int y) const {
  if (pixels == nullptr || isEmpty()) {
    return pixels;
  }
  x = Clamp(x, _width - 1);
  y = Clamp(y, _height - 1);
  auto offset = y * _rowBytes + x * bytesPerPixel();
  return reinterpret_cast<uint8_t*>(pixels) + offset;
}
}  // namespace tgfx