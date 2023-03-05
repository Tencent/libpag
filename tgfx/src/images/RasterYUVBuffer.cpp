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

#include "RasterYUVBuffer.h"
#include "tgfx/gpu/YUVTexture.h"

namespace tgfx {
RasterYUVBuffer::RasterYUVBuffer(std::shared_ptr<YUVData> data, YUVPixelFormat format,
                                 YUVColorSpace colorSpace)
    : data(std::move(data)), colorSpace(colorSpace), format(format) {
}

std::shared_ptr<tgfx::Texture> RasterYUVBuffer::onMakeTexture(tgfx::Context* context, bool) const {
  if (format == YUVPixelFormat::NV12) {
    return YUVTexture::MakeNV12(context, data.get(), colorSpace);
  }
  return YUVTexture::MakeI420(context, data.get(), colorSpace);
}
}  // namespace tgfx
