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

#ifdef PAG_USE_WIN_HARDWARE_DECODER

#include "WinYUVData.h"
#include <cstring>

namespace pag {

WinYUVData::WinYUVData(int width, int height, size_t planeCount)
    : _width(width), _height(height), _planeCount(planeCount) {
  planeData.resize(planeCount);
  rowBytes.resize(planeCount);
}

std::shared_ptr<WinYUVData> WinYUVData::MakeNV12(int width, int height, const uint8_t* yData,
                                                  size_t yRowBytes, const uint8_t* uvData,
                                                  size_t uvRowBytes) {
  if (width <= 0 || height <= 0 || yData == nullptr || uvData == nullptr) {
    return nullptr;
  }

  auto data = std::shared_ptr<WinYUVData>(new WinYUVData(width, height, 2));

  // Y plane
  data->rowBytes[0] = width;
  data->planeData[0].resize(width * height);
  if (yRowBytes == static_cast<size_t>(width)) {
    std::memcpy(data->planeData[0].data(), yData, width * height);
  } else {
    for (int y = 0; y < height; y++) {
      std::memcpy(data->planeData[0].data() + y * width, yData + y * yRowBytes, width);
    }
  }

  // UV plane (interleaved)
  int uvHeight = (height + 1) / 2;
  data->rowBytes[1] = width;
  data->planeData[1].resize(width * uvHeight);
  if (uvRowBytes == static_cast<size_t>(width)) {
    std::memcpy(data->planeData[1].data(), uvData, width * uvHeight);
  } else {
    for (int y = 0; y < uvHeight; y++) {
      std::memcpy(data->planeData[1].data() + y * width, uvData + y * uvRowBytes, width);
    }
  }

  return data;
}

std::shared_ptr<WinYUVData> WinYUVData::MakeI420(int width, int height, const uint8_t* yData,
                                                  size_t yRowBytes, const uint8_t* uData,
                                                  size_t uRowBytes, const uint8_t* vData,
                                                  size_t vRowBytes) {
  if (width <= 0 || height <= 0 || yData == nullptr || uData == nullptr || vData == nullptr) {
    return nullptr;
  }

  auto data = std::shared_ptr<WinYUVData>(new WinYUVData(width, height, 3));

  int uvWidth = (width + 1) / 2;
  int uvHeight = (height + 1) / 2;

  // Y plane
  data->rowBytes[0] = width;
  data->planeData[0].resize(width * height);
  if (yRowBytes == static_cast<size_t>(width)) {
    std::memcpy(data->planeData[0].data(), yData, width * height);
  } else {
    for (int y = 0; y < height; y++) {
      std::memcpy(data->planeData[0].data() + y * width, yData + y * yRowBytes, width);
    }
  }

  // U plane
  data->rowBytes[1] = uvWidth;
  data->planeData[1].resize(uvWidth * uvHeight);
  if (uRowBytes == static_cast<size_t>(uvWidth)) {
    std::memcpy(data->planeData[1].data(), uData, uvWidth * uvHeight);
  } else {
    for (int y = 0; y < uvHeight; y++) {
      std::memcpy(data->planeData[1].data() + y * uvWidth, uData + y * uRowBytes, uvWidth);
    }
  }

  // V plane
  data->rowBytes[2] = uvWidth;
  data->planeData[2].resize(uvWidth * uvHeight);
  if (vRowBytes == static_cast<size_t>(uvWidth)) {
    std::memcpy(data->planeData[2].data(), vData, uvWidth * uvHeight);
  } else {
    for (int y = 0; y < uvHeight; y++) {
      std::memcpy(data->planeData[2].data() + y * uvWidth, vData + y * vRowBytes, uvWidth);
    }
  }

  return data;
}

}  // namespace pag

#endif  // PAG_USE_WIN_HARDWARE_DECODER
