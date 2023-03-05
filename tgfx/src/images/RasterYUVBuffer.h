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
#include "tgfx/core/YUVColorSpace.h"
#include "tgfx/core/YUVData.h"
#include "tgfx/gpu/YUVPixelFormat.h"

namespace tgfx {
/**
 * YUVBuffer represents a raster pixel array described in the YUV format.
 */
class RasterYUVBuffer : public ImageBuffer {
 public:
  RasterYUVBuffer(std::shared_ptr<YUVData> data, YUVPixelFormat format, YUVColorSpace colorSpace);

  int width() const override {
    return data->width();
  }

  int height() const override {
    return data->height();
  }

  bool isAlphaOnly() const final {
    return false;
  }

 protected:
  std::shared_ptr<tgfx::Texture> onMakeTexture(tgfx::Context* context,
                                               bool mipMapped) const override;

 private:
  std::shared_ptr<YUVData> data = nullptr;
  YUVColorSpace colorSpace = YUVColorSpace::BT601_LIMITED;
  YUVPixelFormat format = YUVPixelFormat::Unknown;
};
}  // namespace tgfx
