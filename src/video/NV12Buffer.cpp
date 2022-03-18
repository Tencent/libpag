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
#ifdef FFMPEG
#include "NV12Buffer.h"

namespace pag {
#define NV12_PLANE_COUNT 2

NV12Buffer::NV12Buffer(int width, int height, uint8_t** data, const int* lineSize,
                       tgfx::YUVColorSpace colorSpace, tgfx::YUVColorRange colorRange)
    : VideoBuffer(width, height), colorSpace(colorSpace), colorRange(colorRange) {
  for (int i = 0; i < NV12_PLANE_COUNT; i++) {
    pixelsPlane[i] = data[i];
    rowBytesPlane[i] = lineSize[i];
  }
}

size_t NV12Buffer::planeCount() const {
  return NV12_PLANE_COUNT;
}

std::shared_ptr<tgfx::Texture> NV12Buffer::makeTexture(tgfx::Context* context) const {
  if (context == nullptr) {
    return nullptr;
  }
  return tgfx::YUVTexture::MakeNV12(context, colorSpace, colorRange, width(), height(),
                              const_cast<uint8_t**>(pixelsPlane), rowBytesPlane);
}

}  // namespace pag
#endif