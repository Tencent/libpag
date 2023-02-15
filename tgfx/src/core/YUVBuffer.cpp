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

#include "tgfx/core/YUVBuffer.h"
#include "tgfx/gpu/YUVTexture.h"

namespace tgfx {
class RasterYUVBuffer : public YUVBuffer {
 public:
  RasterYUVBuffer(std::shared_ptr<YUVData> data, YUVPixelFormat format, YUVColorSpace colorSpace,
                  YUVColorRange colorRange)
      : YUVBuffer(colorSpace, colorRange), data(std::move(data)), format(format) {
  }

  int width() const override {
    return data->width();
  }

  int height() const override {
    return data->height();
  }

  size_t planeCount() const override {
    return data->planeCount();
  }

  YUVPixelFormat pixelFormat() const override {
    return format;
  }

 protected:
  std::shared_ptr<tgfx::Texture> onMakeTexture(tgfx::Context* context, bool) const override {
    if (format == YUVPixelFormat::NV12) {
      return YUVTexture::MakeNV12(context, data.get(), colorSpace(), colorRange());
    }
    return YUVTexture::MakeI420(context, data.get(), colorSpace(), colorRange());
  }

 private:
  std::shared_ptr<YUVData> data = nullptr;
  YUVPixelFormat format = YUVPixelFormat::Unknown;
};

std::shared_ptr<YUVBuffer> YUVBuffer::MakeI420(std::shared_ptr<YUVData> yuvData,
                                               YUVColorSpace colorSpace, YUVColorRange colorRange) {
  if (yuvData == nullptr || yuvData->planeCount() != YUVData::I420_PLANE_COUNT) {
    return nullptr;
  }
  return std::make_shared<RasterYUVBuffer>(std::move(yuvData), YUVPixelFormat::I420, colorSpace,
                                           colorRange);
}

std::shared_ptr<YUVBuffer> YUVBuffer::MakeNV12(std::shared_ptr<YUVData> yuvData,
                                               YUVColorSpace colorSpace, YUVColorRange colorRange) {
  if (yuvData == nullptr || yuvData->planeCount() != YUVData::NV12_PLANE_COUNT) {
    return nullptr;
  }
  return std::make_shared<RasterYUVBuffer>(std::move(yuvData), YUVPixelFormat::NV12, colorSpace,
                                           colorRange);
}
}  // namespace tgfx
