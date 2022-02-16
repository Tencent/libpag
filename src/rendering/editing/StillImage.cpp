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
#include "gpu/opengl/GLDevice.h"
#include "pag/pag.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/graphics/Graphic.h"
#include "rendering/graphics/Picture.h"
#include "rendering/graphics/Recorder.h"

namespace pag {

std::shared_ptr<PAGImage> PAGImage::FromPath(const std::string& filePath) {
  auto pagImage = std::make_shared<StillImage>();
  pagImage->image = tgfx::Image::MakeFrom(filePath);
  auto picture = Picture::MakeFrom(pagImage->uniqueID(), pagImage->image);
  if (!picture) {
    return nullptr;
  }
  pagImage->reset(picture);
  return pagImage;
}

std::shared_ptr<PAGImage> PAGImage::FromBytes(const void* bytes, size_t length) {
  auto pagImage = std::make_shared<StillImage>();
  auto fileBytes = tgfx::Data::MakeWithCopy(bytes, length);
  pagImage->image = tgfx::Image::MakeFrom(std::move(fileBytes));
  auto picture = Picture::MakeFrom(pagImage->uniqueID(), pagImage->image);
  if (!picture) {
    return nullptr;
  }
  pagImage->reset(picture);
  return pagImage;
}

std::shared_ptr<PAGImage> PAGImage::FromPixels(const void* pixels, int width, int height,
                                               size_t rowBytes, ColorType colorType,
                                               AlphaType alphaType) {
  auto pixelBuffer = tgfx::PixelBuffer::Make(width, height);
  tgfx::Bitmap bitmap(pixelBuffer);
  if (bitmap.isEmpty()) {
    return nullptr;
  }
  auto info = tgfx::ImageInfo::Make(width, height, ToTGFX(colorType), ToTGFX(alphaType), rowBytes);
  auto result = bitmap.writePixels(info, pixels);
  if (!result) {
    return nullptr;
  }
  return StillImage::FromPixelBuffer(pixelBuffer);
}

std::shared_ptr<StillImage> StillImage::FromPixelBuffer(
    std::shared_ptr<tgfx::PixelBuffer> pixelBuffer) {
  if (pixelBuffer == nullptr) {
    return nullptr;
  }
  auto pagImage = std::make_shared<StillImage>();
  auto picture = Picture::MakeFrom(pagImage->uniqueID(), pixelBuffer);
  if (!picture) {
    return nullptr;
  }
  pagImage->reset(picture);
  return pagImage;
}

std::shared_ptr<StillImage> StillImage::FromImage(std::shared_ptr<tgfx::Image> image) {
  if (image == nullptr) {
    return nullptr;
  }
  auto pagImage = std::make_shared<StillImage>();
  pagImage->image = std::move(image);
  auto picture = Picture::MakeFrom(pagImage->uniqueID(), pagImage->image);
  if (!picture) {
    return nullptr;
  }
  pagImage->reset(picture);
  return pagImage;
}

std::shared_ptr<PAGImage> PAGImage::FromTexture(const BackendTexture& texture, ImageOrigin origin) {
  auto context = tgfx::GLDevice::CurrentNativeHandle();
  if (context == nullptr) {
    LOGE("PAGImage.MakeFrom() There is no current GPU context on the calling thread.");
    return nullptr;
  }
  auto pagImage = std::make_shared<StillImage>();
  auto picture = Picture::MakeFrom(pagImage->uniqueID(), ToTGFX(texture), ToTGFX(origin));
  if (!picture) {
    LOGE("PAGImage.MakeFrom() The texture is invalid.");
    return nullptr;
  }
  pagImage->reset(picture);
  return pagImage;
}

void StillImage::measureBounds(tgfx::Rect* bounds) {
  graphic->measureBounds(bounds);
}

tgfx::Rect StillImage::getContentSize() const {
  return tgfx::Rect::MakeWH(static_cast<float>(width), static_cast<float>(height));
}

void StillImage::draw(Recorder* recorder) {
  recorder->drawGraphic(graphic);
}

void StillImage::reset(std::shared_ptr<Graphic> g) {
  tgfx::Rect bounds = {};
  g->measureBounds(&bounds);
  width = static_cast<int>(bounds.width());
  height = static_cast<int>(bounds.height());
  graphic = g;
}
}  // namespace pag
