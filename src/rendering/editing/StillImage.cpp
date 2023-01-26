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

#include "StillImage.h"
#include "base/utils/TGFXCast.h"
#include "base/utils/UniqueID.h"
#include "pag/pag.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/graphics/Graphic.h"
#include "rendering/graphics/Picture.h"
#include "tgfx/gpu/opengl/GLDevice.h"

namespace pag {
std::shared_ptr<PAGImage> PAGImage::FromPath(const std::string& filePath) {
  auto codec = tgfx::ImageCodec::MakeFrom(filePath);
  return StillImage::MakeFrom(std::move(codec));
}

std::shared_ptr<PAGImage> PAGImage::FromBytes(const void* bytes, size_t length) {
  auto fileBytes = tgfx::Data::MakeWithCopy(bytes, length);
  auto codec = tgfx::ImageCodec::MakeFrom(std::move(fileBytes));
  return StillImage::MakeFrom(std::move(codec));
}

std::shared_ptr<PAGImage> PAGImage::FromPixels(const void* pixels, int width, int height,
                                               size_t rowBytes, ColorType colorType,
                                               AlphaType alphaType) {
  auto info = tgfx::ImageInfo::Make(width, height, ToTGFX(colorType), ToTGFX(alphaType), rowBytes);
  if (info.isEmpty()) {
    return nullptr;
  }
  auto pixelBuffer = tgfx::PixelBuffer::Make(width, height, info.isAlphaOnly());
  tgfx::Bitmap bitmap(pixelBuffer);
  if (bitmap.isEmpty()) {
    return nullptr;
  }
  auto result = bitmap.writePixels(info, pixels);
  if (!result) {
    return nullptr;
  }
  return StillImage::MakeFrom(pixelBuffer);
}

std::shared_ptr<StillImage> StillImage::MakeFrom(std::shared_ptr<tgfx::PixelBuffer> pixelBuffer) {
  if (pixelBuffer == nullptr) {
    return nullptr;
  }
  auto pagImage =
      std::shared_ptr<StillImage>(new StillImage(pixelBuffer->width(), pixelBuffer->height()));
  auto picture = Picture::MakeFrom(pagImage->uniqueID(), pixelBuffer);
  if (!picture) {
    return nullptr;
  }
  pagImage->graphic = picture;
  return pagImage;
}

std::shared_ptr<StillImage> StillImage::MakeFrom(std::shared_ptr<tgfx::ImageCodec> codec) {
  if (codec == nullptr) {
    return nullptr;
  }
  auto width = codec->width();
  auto height = codec->height();
  ApplyOrientation(codec->orientation(), &width, &height);
  auto pagImage = std::shared_ptr<StillImage>(new StillImage(width, height));
  auto picture = Picture::MakeFrom(pagImage->uniqueID(), codec);
  if (!picture) {
    return nullptr;
  }
  pagImage->graphic = picture;
  return pagImage;
}

std::shared_ptr<PAGImage> PAGImage::FromTexture(const BackendTexture& texture, ImageOrigin origin) {
  auto context = tgfx::GLDevice::CurrentNativeHandle();
  if (context == nullptr) {
    LOGE("PAGImage.FromTexture() There is no current GPU context on the calling thread.");
    return nullptr;
  }
  auto pagImage = std::shared_ptr<StillImage>(new StillImage(texture.width(), texture.height()));
  auto picture = Picture::MakeFrom(pagImage->uniqueID(), texture, ToTGFX(origin));
  if (!picture) {
    LOGE("PAGImage.MakeFrom() The texture is invalid.");
    return nullptr;
  }
  pagImage->graphic = picture;
  return pagImage;
}
}  // namespace pag
