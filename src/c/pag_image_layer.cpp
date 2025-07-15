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

#include "pag/c/pag_image_layer.h"
#include "pag/c/pag_image.h"
#include "pag_types_priv.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/ImageInfo.h"

uint8_t* pag_image_layer_get_image_rgba_data(pag_layer* imageLayer, size_t* count, size_t* width,
                                             size_t* height) {
  if (imageLayer == nullptr) {
    return nullptr;
  }
  std::shared_ptr<pag::PAGImageLayer> imgLayer =
      std::static_pointer_cast<pag::PAGImageLayer>(imageLayer->p);
  if (!imgLayer) {
    return nullptr;
  }
  pag::ByteData* imageBytes = imgLayer->imageBytes();
  auto data = tgfx::Data::MakeWithoutCopy(imageBytes->data(), imageBytes->length());
  std::shared_ptr<tgfx::ImageCodec> webp = tgfx::ImageCodec::MakeFrom(data);
  auto info = tgfx::ImageInfo::Make(webp->width(), webp->height(), tgfx::ColorType::RGBA_8888,
                                    tgfx::AlphaType::Premultiplied);
  *count = 4 * webp->width() * webp->height();
  *width = webp->width();
  *height = webp->height();
  auto rgbaData = new uint8_t[*count];
  auto success = webp->readPixels(info, rgbaData);
  if (success) {
    return rgbaData;
  } else {
    return nullptr;
  }
}
