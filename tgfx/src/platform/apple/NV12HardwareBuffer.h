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

#import <CoreVideo/CoreVideo.h>
#include "tgfx/core/YUVBuffer.h"

namespace tgfx {
class NV12HardwareBuffer : public YUVBuffer {
 public:
  static std::shared_ptr<NV12HardwareBuffer> MakeFrom(CVPixelBufferRef pixelBuffer,
                                                      YUVColorSpace colorSpace,
                                                      YUVColorRange colorRange);

  ~NV12HardwareBuffer() override;

  int width() const override;

  int height() const override;

  size_t planeCount() const override {
    return YUVData::NV12_PLANE_COUNT;
  }

  YUVPixelFormat pixelFormat() const override {
    return YUVPixelFormat::NV12;
  }

 protected:
  std::shared_ptr<Texture> onMakeTexture(Context* context, bool mipMapped) const override;

 private:
  CVPixelBufferRef pixelBuffer = nullptr;

  NV12HardwareBuffer(CVPixelBufferRef pixelBuffer, YUVColorSpace colorSpace,
                     YUVColorRange colorRange);
};

}  // namespace tgfx
