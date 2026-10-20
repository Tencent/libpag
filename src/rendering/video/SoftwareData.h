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

#include <cstring>
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

// Copies YUV planes out of the caller-provided buffers. tgfx::ImageBuffer::MakeI420() requires
// the yuv data to stay unchanged for the whole lifetime of the ImageBuffer, which the internal
// frame buffers of a software decoder cannot guarantee: a later decode call may recycle and
// overwrite them. Use this class whenever the returned ImageBuffer can outlive the current decode
// call, for example when a VideoReader retains the last decoded frame as a fallback for failed
// decodes.
class CopiedYUVData : public tgfx::YUVData {
 public:
  static std::shared_ptr<YUVData> Make(int width, int height, uint8_t* buffer[3],
                                       const int lineSize[3], int planeCount) {
    if (width <= 0 || height <= 0 || planeCount <= 0 || planeCount > 3) {
      return nullptr;
    }
    auto data = new CopiedYUVData(width, height, buffer, lineSize, planeCount);
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
    return planeAddresses.size();
  }

  const void* getBaseAddressAt(size_t planeIndex) const override {
    return planeAddresses[planeIndex];
  }

  size_t getRowBytesAt(size_t planeIndex) const override {
    return planeRowBytes[planeIndex];
  }

 private:
  int _width = 0;
  int _height = 0;
  std::vector<uint8_t> pixels = {};
  std::vector<const void*> planeAddresses = {};
  std::vector<size_t> planeRowBytes = {};

  CopiedYUVData(int width, int height, uint8_t* buffer[3], const int lineSize[3], int planeCount)
      : _width(width), _height(height) {
    planeAddresses.reserve(planeCount);
    planeRowBytes.reserve(planeCount);
    size_t planeSizes[3] = {};
    int planeHeights[3] = {};
    size_t totalSize = 0;
    for (int i = 0; i < planeCount; i++) {
      auto planeWidth = i == 0 ? width : (width + 1) / 2;
      planeHeights[i] = i == 0 ? height : (height + 1) / 2;
      planeRowBytes.push_back(static_cast<size_t>(planeWidth));
      planeSizes[i] = planeRowBytes[i] * static_cast<size_t>(planeHeights[i]);
      totalSize += planeSizes[i];
    }
    pixels.resize(totalSize);
    size_t offset = 0;
    for (int i = 0; i < planeCount; i++) {
      planeAddresses.push_back(pixels.data() + offset);
      for (int row = 0; row < planeHeights[i]; row++) {
        memcpy(pixels.data() + offset + row * planeRowBytes[i],
               buffer[i] + static_cast<size_t>(row) * static_cast<size_t>(lineSize[i]),
               planeRowBytes[i]);
      }
      offset += planeSizes[i];
    }
  }
};
}  // namespace pag
