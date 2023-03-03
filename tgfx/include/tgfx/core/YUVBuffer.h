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

#pragma once

#include "tgfx/core/Data.h"
#include "tgfx/core/ImageBuffer.h"
#include "tgfx/core/YUVData.h"
#include "tgfx/core/YUVInfo.h"

namespace tgfx {
/**
 * YUVBuffer represents a two-dimensional array of pixels described in the YUV format.
 */
class YUVBuffer : public ImageBuffer {
 public:
  /**
   * Creates a new YUVBuffer in the I420 format from the specified YUVData.
   */
  static std::shared_ptr<YUVBuffer> MakeI420(std::shared_ptr<YUVData> yuvData,
                                             YUVColorSpace colorSpace = YUVColorSpace::Rec601,
                                             YUVColorRange colorRange = YUVColorRange::MPEG);

  /**
   * Creates a new YUVBuffer in the NV12 format from the specified YUVData.
   */
  static std::shared_ptr<YUVBuffer> MakeNV12(std::shared_ptr<YUVData> yuvData,
                                             YUVColorSpace colorSpace = YUVColorSpace::Rec601,
                                             YUVColorRange colorRange = YUVColorRange::MPEG);

  /**
   * Creates a new YUVBuffer from a platform-specific hardware buffer. The hardwareBuffer could be
   * an AHardwareBuffer on the android platform or a CVPixelBufferRef on the apple platform. The
   * returned YUVBuffer takes a reference on the hardwareBuffer. Returns nullptr if the
   * hardwareBuffer is nullptr or has only one plane.
   */
  static std::shared_ptr<YUVBuffer> MakeFrom(HardwareBufferRef hardwareBuffer,
                                             YUVColorSpace colorSpace = YUVColorSpace::Rec601,
                                             YUVColorRange colorRange = YUVColorRange::MPEG);

  bool isAlphaOnly() const final {
    return false;
  }

  bool isYUV() const final {
    return true;
  }

  /**
   * Returns number of planes in this yuv buffer.
   */
  virtual size_t planeCount() const = 0;

  /**
   * The pixel format of this yuv buffer.
   */
  virtual YUVPixelFormat pixelFormat() const = 0;

  /**
   * The color space of this yuv buffer.
   */
  YUVColorSpace colorSpace() const {
    return _colorSpace;
  }

  /**
   * The color range of this yuv buffer.
   */
  YUVColorRange colorRange() const {
    return _colorRange;
  }

 protected:
  YUVBuffer(YUVColorSpace colorSpace, YUVColorRange colorRange)
      : _colorSpace(colorSpace), _colorRange(colorRange) {
  }

 private:
  YUVColorSpace _colorSpace = YUVColorSpace::Rec601;
  YUVColorRange _colorRange = YUVColorRange::MPEG;
};
}  // namespace tgfx
