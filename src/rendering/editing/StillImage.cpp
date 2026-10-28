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

#include "StillImage.h"
#include "base/utils/TGFXCast.h"
#include "base/utils/UniqueID.h"
#include "pag/pag.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/gpu/Devices.h"
#include "rendering/graphics/Graphic.h"
#include "rendering/graphics/Picture.h"

namespace pag {
std::shared_ptr<PAGImage> PAGImage::FromPath(const std::string& filePath) {
  auto image = tgfx::Image::MakeFromFile(filePath);
  return StillImage::MakeFrom(std::move(image));
}

std::shared_ptr<PAGImage> PAGImage::FromBytes(const void* bytes, size_t length) {
  auto fileBytes = tgfx::Data::MakeWithCopy(bytes, length);
  auto image = tgfx::Image::MakeFromEncoded(std::move(fileBytes));
  return StillImage::MakeFrom(std::move(image));
}

std::shared_ptr<PAGImage> PAGImage::FromPixels(const void* pixels, int width, int height,
                                               size_t rowBytes, ColorType colorType,
                                               AlphaType alphaType) {
  auto info = tgfx::ImageInfo::Make(width, height, ToTGFX(colorType), ToTGFX(alphaType), rowBytes);
  tgfx::Bitmap bitmap(width, height, info.isAlphaOnly());
  bitmap.writePixels(info, pixels);
  auto image = tgfx::Image::MakeFrom(bitmap);
  return StillImage::MakeFrom(image);
}

std::shared_ptr<StillImage> StillImage::MakeFrom(std::shared_ptr<tgfx::Image> image) {
  if (image == nullptr) {
    return nullptr;
  }
  auto pagImage = std::shared_ptr<StillImage>(new StillImage(image->width(), image->height()));
  auto picture = Picture::MakeFrom(pagImage->uniqueID(), image);
  if (!picture) {
    return nullptr;
  }
  pagImage->graphic = picture;
  return pagImage;
}

std::shared_ptr<PAGImage> PAGImage::FromTexture(const BackendTexture& texture, ImageOrigin origin) {
  auto tgfxTexture = ToTGFX(texture);
  if (!tgfxTexture.isValid()) {
    LOGE("PAGImage.FromTexture() The texture is invalid.");
    return nullptr;
  }
  if (!Devices::SupportsBackend(tgfxTexture.backend())) {
    // A texture of another GPU backend can never be sampled by this build's context. Reject it
    // here instead of returning an image that silently draws blank forever.
    LOGE(
        "PAGImage.FromTexture() The texture belongs to a different GPU backend than the current "
        "libpag build.");
    return nullptr;
  }
  auto pagImage = std::shared_ptr<StillImage>(new StillImage(texture.width(), texture.height()));
  // With a valid texture of the matching backend, Picture::MakeFrom() can only fail when the
  // owning device identity could not be captured, which GL requires a current GPU context on the
  // calling thread for (the native handle is used for the later share-compatibility check).
  auto picture = Picture::MakeFrom(pagImage->uniqueID(), tgfxTexture, ToTGFX(origin));
  if (!picture) {
    LOGE("PAGImage.FromTexture() There is no current GPU context on the calling thread.");
    return nullptr;
  }
  pagImage->graphic = picture;
  return pagImage;
}
}  // namespace pag
