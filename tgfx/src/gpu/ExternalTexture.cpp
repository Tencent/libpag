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

#include "ExternalTexture.h"
#include "gpu/Gpu.h"
#include "utils/PixelFormatUtil.h"

namespace tgfx {
std::shared_ptr<Texture> Texture::MakeFrom(Context* context, const BackendTexture& backendTexture,
                                           SurfaceOrigin origin) {
  return ExternalTexture::MakeFrom(context, backendTexture, origin, false);
}

std::shared_ptr<Texture> Texture::MakeAdopted(Context* context,
                                              const BackendTexture& backendTexture,
                                              SurfaceOrigin origin) {
  return ExternalTexture::MakeFrom(context, backendTexture, origin, true);
}

std::shared_ptr<Texture> ExternalTexture::MakeFrom(Context* context,
                                                   const BackendTexture& backendTexture,
                                                   SurfaceOrigin origin, bool adopted) {
  if (context == nullptr || !backendTexture.isValid()) {
    return nullptr;
  }
  auto sampler = TextureSampler::MakeFrom(context, backendTexture);
  if (sampler == nullptr) {
    return nullptr;
  }
  auto texture = new ExternalTexture(std::move(sampler), backendTexture.width(),
                                     backendTexture.height(), origin, adopted);
  return Resource::Wrap(context, texture);
}

ExternalTexture::ExternalTexture(std::unique_ptr<TextureSampler> sampler, int width, int height,
                                 SurfaceOrigin origin, bool adopted)
    : Texture(width, height, origin), sampler(std::move(sampler)), adopted(adopted) {
}

size_t ExternalTexture::memoryUsage() const {
  if (!adopted) {
    return 0;
  }
  auto colorSize = width() * height() * PixelFormatBytesPerPixel(sampler->format);
  return sampler->hasMipmaps() ? colorSize * 4 / 3 : colorSize;
}

void ExternalTexture::onReleaseGPU() {
  if (adopted) {
    context->gpu()->deleteSampler(sampler.get());
  }
}
}  // namespace tgfx
