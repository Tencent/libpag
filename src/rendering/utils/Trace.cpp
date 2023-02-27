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

#include "Trace.h"
#include "platform/Platform.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/PixelBuffer.h"
#include "tgfx/gpu/Surface.h"

namespace pag {
void Trace(std::shared_ptr<tgfx::PixelBuffer> pixelBuffer, const std::string& tag) {
  tgfx::Pixmap bitmap(pixelBuffer);
  Trace(bitmap, tag);
}

void Trace(std::shared_ptr<tgfx::Texture> texture, const std::string& path) {
  if (texture == nullptr) {
    return;
  }
  auto surface = tgfx::Surface::Make(texture->getContext(), texture->width(), texture->height());
  if (surface == nullptr) {
    return;
  }
  auto canvas = surface->getCanvas();
  canvas->drawTexture(std::move(texture));
  tgfx::Bitmap bitmap = {};
  bitmap.allocPixels(surface->width(), surface->height());
  tgfx::Pixmap pixmap(bitmap);
  if (pixmap.isEmpty()) {
    return;
  }
  surface->readPixels(pixmap.info(), pixmap.writablePixels());
  Trace(pixmap, path);
}

void Trace(const tgfx::Pixmap& pixmap, const std::string& tag) {
  if (pixmap.isEmpty()) {
    return;
  }
  Platform::Current()->traceImage(pixmap.info(), pixmap.pixels(), tag);
}
}  // namespace pag
