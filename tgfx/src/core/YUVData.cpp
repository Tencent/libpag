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

#include "tgfx/core/YUVData.h"
#include <vector>

namespace tgfx {
class RasterYUVData : public YUVData {
 public:
  RasterYUVData(int width, int height, const void** buffer, const size_t* planeRowBytes,
                size_t planeCount, ReleaseProc releaseProc, void* context)
      : _width(width), _height(height), releaseProc(releaseProc), releaseContext(context) {
    data.reserve(planeCount);
    rowBytes.reserve(planeCount);
    for (int i = 0; i < static_cast<int>(planeCount); i++) {
      data.push_back(buffer[i]);
      rowBytes.push_back(planeRowBytes[i]);
    }
  }

  ~RasterYUVData() override {
    if (releaseProc != nullptr) {
      releaseProc(releaseContext, &data[0], data.size());
    }
  }

  int width() const override {
    return _width;
  }

  int height() const override {
    return _height;
  }

  size_t planeCount() const override {
    return data.size();
  }

  const void* getBaseAddressAt(int planeIndex) const override {
    return data[planeIndex];
  }

  size_t getRowBytesAt(int planeIndex) const override {
    return rowBytes[planeIndex];
  }

 private:
  int _width = 0;
  int _height = 0;
  ReleaseProc releaseProc = nullptr;
  void* releaseContext = nullptr;
  std::vector<const void*> data = {};
  std::vector<size_t> rowBytes = {};
};

std::shared_ptr<YUVData> YUVData::MakeFrom(int width, int height, const void** data,
                                           const size_t* rowBytes, size_t planeCount,
                                           ReleaseProc releaseProc, void* context) {
  if (width <= 0 || height <= 0 || data == nullptr || rowBytes == nullptr || planeCount == 0) {
    return nullptr;
  }
  return std::make_shared<RasterYUVData>(width, height, data, rowBytes, planeCount, releaseProc,
                                         context);
}

}  // namespace tgfx
