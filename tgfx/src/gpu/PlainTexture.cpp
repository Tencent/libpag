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

#include "PlainTexture.h"
#include "gpu/Gpu.h"
#include "utils/PixelFormatUtil.h"
#include "utils/UniqueID.h"

namespace tgfx {
static void ComputeScratchKey(BytesKey* scratchKey, int width, int height, PixelFormat format,
                              bool mipMapped) {
  static const uint32_t PlainTextureType = UniqueID::Next();
  scratchKey->write(PlainTextureType);
  scratchKey->write(static_cast<uint32_t>(width));
  scratchKey->write(static_cast<uint32_t>(height));
  auto formatValue = static_cast<uint32_t>(format);
  auto mipMapValue = static_cast<uint32_t>(mipMapped ? 1 : 0);
  scratchKey->write(formatValue | (mipMapValue << 30));
}

std::shared_ptr<Texture> Texture::MakeFormat(Context* context, int width, int height,
                                             const void* pixels, size_t rowBytes,
                                             PixelFormat pixelFormat, ImageOrigin origin,
                                             bool mipMapped) {
  if (!PlainTexture::CheckSizeAndFormat(context, width, height, pixelFormat)) {
    return nullptr;
  }
  auto caps = context->caps();
  bool enableMipMap = mipMapped && caps->mipMapSupport;
  ScratchKey scratchKey = {};
  ComputeScratchKey(&scratchKey, width, height, pixelFormat, enableMipMap);
  auto texture =
      std::static_pointer_cast<Texture>(context->resourceCache()->findScratchResource(scratchKey));
  if (texture) {
    texture->_origin = origin;
  } else {
    int maxMipmapLevel = 0;
    if (enableMipMap) {
      maxMipmapLevel = static_cast<int>(std::floor(std::log2(std::max(width, height))));
    }
    auto sampler = context->gpu()->createSampler(width, height, pixelFormat, maxMipmapLevel + 1);
    if (sampler == nullptr) {
      return nullptr;
    }
    texture = Resource::Wrap(context, new PlainTexture(std::move(sampler), width, height, origin));
  }
  if (pixels != nullptr) {
    context->gpu()->writePixels(texture->getSampler(), Rect::MakeWH(width, height), pixels,
                                rowBytes, pixelFormat);
  }
  return texture;
}

bool PlainTexture::CheckSizeAndFormat(Context* context, int width, int height, PixelFormat format) {
  if (context == nullptr || width < 1 || height < 1) {
    return false;
  }
  if (format != PixelFormat::ALPHA_8 && format != PixelFormat::RGBA_8888 &&
      format != PixelFormat::BGRA_8888) {
    return false;
  }
  auto caps = context->caps();
  auto maxTextureSize = caps->maxTextureSize;
  return width <= maxTextureSize && height <= maxTextureSize;
}

PlainTexture::PlainTexture(std::unique_ptr<TextureSampler> sampler, int width, int height,
                           ImageOrigin origin)
    : Texture(width, height, origin), sampler(std::move(sampler)) {
}

size_t PlainTexture::memoryUsage() const {
  auto colorSize = width() * height() * PixelFormatBytesPerPixel(sampler->format);
  return sampler->hasMipmaps() ? colorSize * 4 / 3 : colorSize;
}

void PlainTexture::computeScratchKey(BytesKey* scratchKey) const {
  ComputeScratchKey(scratchKey, width(), height(), sampler->format, sampler->hasMipmaps());
}

void PlainTexture::onReleaseGPU() {
  context->gpu()->deleteSampler(sampler.get());
}
}  // namespace tgfx
