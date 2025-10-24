/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "pag/c/pag_image.h"
#include "pag_types_priv.h"
#include "rendering/editing/StillImage.h"
#include "tgfx/core/Image.h"

pag_image* pag_image_from_pixels(const void* pixels, int width, int height, size_t rowBytes,
                                 pag_color_type colorType, pag_alpha_type alphaType) {
  pag::ColorType color;
  if (!FromCColorType(colorType, &color)) {
    return nullptr;
  }
  pag::AlphaType alpha;
  if (!FromCAlphaType(alphaType, &alpha)) {
    return nullptr;
  }
  if (auto image = pag::PAGImage::FromPixels(pixels, width, height, rowBytes, color, alpha)) {
    return new pag_image(std::move(image));
  }
  return nullptr;
}

pag_image* pag_image_from_hardware_buffer(void* buffer) {
  if (buffer == nullptr) {
    return nullptr;
  }
  if (auto image = pag::StillImage::MakeFrom(
          tgfx::Image::MakeFrom(static_cast<tgfx::HardwareBufferRef>(buffer)))) {
    return new pag_image(std::move(image));
  }
  return nullptr;
}

pag_image* pag_image_from_backend_texture(pag_backend_texture* texture) {
  if (texture == nullptr) {
    return nullptr;
  }
  if (auto image = pag::PAGImage::FromTexture(texture->p, pag::ImageOrigin::TopLeft)) {
    return new pag_image(std::move(image));
  }
  return nullptr;
}

void pag_image_set_scale_mode(pag_image* image, pag_scale_mode mode) {
  if (image == nullptr) {
    return;
  }
  image->p->setScaleMode(static_cast<pag::PAGScaleMode>(static_cast<uint8_t>(mode)));
}
