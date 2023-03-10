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

#include "RasterBuffer.h"
#include "gpu/Texture.h"

namespace tgfx {
std::shared_ptr<ImageBuffer> RasterBuffer::MakeFrom(const ImageInfo& info,
                                                    std::shared_ptr<Data> pixels) {
  if (info.isEmpty() || pixels == nullptr || info.byteSize() > pixels->size() ||
      info.alphaType() == AlphaType::Unpremultiplied) {
    return nullptr;
  }
  switch (info.colorType()) {
    case ColorType::ALPHA_8:
    case ColorType::RGBA_8888:
    case ColorType::BGRA_8888:
      return std::shared_ptr<RasterBuffer>(new RasterBuffer(info, std::move(pixels)));
    default:
      return nullptr;
  }
}

RasterBuffer::RasterBuffer(const ImageInfo& info, std::shared_ptr<Data> pixels)
    : info(info), pixels(std::move(pixels)) {
}

std::shared_ptr<Texture> RasterBuffer::onMakeTexture(Context* context, bool mipMapped) const {
  switch (info.colorType()) {
    case ColorType::ALPHA_8:
      return Texture::MakeAlpha(context, info.width(), info.height(), pixels->data(),
                                info.rowBytes(), SurfaceOrigin::TopLeft, mipMapped);
    case ColorType::BGRA_8888:
      return Texture::MakeFormat(context, info.width(), info.height(), pixels->data(),
                                 info.rowBytes(), PixelFormat::BGRA_8888, SurfaceOrigin::TopLeft,
                                 mipMapped);
    case ColorType::RGBA_8888:
      return Texture::MakeRGBA(context, info.width(), info.height(), pixels->data(),
                               info.rowBytes(), SurfaceOrigin::TopLeft, mipMapped);
    default:
      return nullptr;
  }
}
}  // namespace tgfx
