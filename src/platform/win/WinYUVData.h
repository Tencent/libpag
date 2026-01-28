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

#pragma once

#ifdef PAG_USE_WIN_HARDWARE_DECODER

#include <memory>
#include <vector>
#include "tgfx/core/YUVData.h"

namespace pag {

/**
 * WinYUVData holds YUV pixel data from FFmpeg hardware decoded frames.
 * Used for AMD/Intel GPUs where D3D11-OpenGL interop is not available,
 * requiring a CPU readback of NV12 data.
 */
class WinYUVData : public tgfx::YUVData {
 public:
  /**
   * Creates a YUVData from NV12 format data (2 planes: Y and UV interleaved).
   */
  static std::shared_ptr<WinYUVData> MakeNV12(int width, int height, const uint8_t* yData,
                                               size_t yRowBytes, const uint8_t* uvData,
                                               size_t uvRowBytes);

  /**
   * Creates a YUVData from I420 format data (3 planes: Y, U, V).
   */
  static std::shared_ptr<WinYUVData> MakeI420(int width, int height, const uint8_t* yData,
                                               size_t yRowBytes, const uint8_t* uData,
                                               size_t uRowBytes, const uint8_t* vData,
                                               size_t vRowBytes);

  ~WinYUVData() override = default;

  int width() const override {
    return _width;
  }

  int height() const override {
    return _height;
  }

  size_t planeCount() const override {
    return _planeCount;
  }

  const void* getBaseAddressAt(size_t planeIndex) const override {
    if (planeIndex >= _planeCount) {
      return nullptr;
    }
    return planeData[planeIndex].data();
  }

  size_t getRowBytesAt(size_t planeIndex) const override {
    if (planeIndex >= _planeCount) {
      return 0;
    }
    return rowBytes[planeIndex];
  }

 private:
  WinYUVData(int width, int height, size_t planeCount);

  int _width = 0;
  int _height = 0;
  size_t _planeCount = 0;

  std::vector<std::vector<uint8_t>> planeData;
  std::vector<size_t> rowBytes;
};

}  // namespace pag

#endif  // PAG_USE_WIN_HARDWARE_DECODER
