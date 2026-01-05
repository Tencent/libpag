/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 Tencent. All rights reserved.
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

#include <vector>
#include "tgfx/core/YUVData.h"

namespace pag {
template <typename T>
class SoftwareData : public tgfx::YUVData {
 public:
  static std::shared_ptr<YUVData> Make(int width, int height, uint8_t* buffer[3],
                                       const int lineSize[3], int planeCount,
                                       std::shared_ptr<T> softwareDecoder) {
    auto data =
        new SoftwareData(width, height, buffer, lineSize, planeCount, std::move(softwareDecoder));
    return std::shared_ptr<YUVData>(data);
  }

 private:
  int width() const override {
    return _width;
  }

  int height() const override {
    return _height;
  }

  size_t planeCount() const override {
    return data.size();
  }

  const void* getBaseAddressAt(size_t planeIndex) const override {
    return data[planeIndex];
  }

  size_t getRowBytesAt(size_t planeIndex) const override {
    return rowBytes[planeIndex];
  }

 private:
  int _width = 0;
  int _height = 0;
  std::vector<const void*> data = {};
  std::vector<size_t> rowBytes = {};
  // hold a reference to the software decoder to keep the yuv data alive.
  std::shared_ptr<T> softwareDecoder = nullptr;

  SoftwareData(int width, int height, uint8_t* buffer[3], const int lineSize[3], int planeCount,
               std::shared_ptr<T> softwareDecoder)
      : _width(width), _height(height), softwareDecoder(std::move(softwareDecoder)) {
    data.reserve(planeCount);
    rowBytes.reserve(planeCount);
    for (int i = 0; i < planeCount; i++) {
      data.push_back(buffer[i]);
      rowBytes.push_back(lineSize[i]);
    }
  }
};
}  // namespace pag
