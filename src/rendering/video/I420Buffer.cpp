/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "I420Buffer.h"

namespace pag {
I420Buffer::I420Buffer(std::shared_ptr<tgfx::YUVData> yuvData, tgfx::YUVColorSpace colorSpace,
                       tgfx::YUVColorRange colorRange)
    : VideoBuffer(yuvData->width(), yuvData->height()), yuvData(std::move(yuvData)),
      colorSpace(colorSpace), colorRange(colorRange) {
}

size_t I420Buffer::planeCount() const {
  return tgfx::YUVData::I420_PLANE_COUNT;
}

std::shared_ptr<tgfx::Texture> I420Buffer::onMakeTexture(tgfx::Context* context, bool) const {
  return tgfx::YUVTexture::MakeI420(context, yuvData.get(), colorSpace, colorRange);
}
}  // namespace pag
