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

#include "Drawable.h"
#include "base/utils/TGFXCast.h"

namespace pag {

tgfx::Context* Drawable::lockContext() {
  if (currentDevice == nullptr) {
    return nullptr;
  }
  return currentDevice->lockContext();
}

void Drawable::unlockContext() {
  if (currentDevice == nullptr) {
    return;
  }
  currentDevice->unlock();
}

bool Drawable::prepareDevice() {
  if (currentDevice == nullptr) {
    currentDevice = getDevice();
  }
  return currentDevice != nullptr;
}

void Drawable::freeDevice() {
  currentDevice = nullptr;
}

RenderTargetDrawable::RenderTargetDrawable(std::shared_ptr<tgfx::Device> device,
                                           const tgfx::BackendRenderTarget& renderTarget,
                                           tgfx::ImageOrigin origin)
    : device(std::move(device)), renderTarget(renderTarget), origin(origin) {
}

std::shared_ptr<tgfx::Surface> RenderTargetDrawable::createSurface(tgfx::Context* context) {
  return tgfx::Surface::MakeFrom(context, renderTarget, origin);
}

TextureDrawable::TextureDrawable(std::shared_ptr<tgfx::Device> device,
                                 const tgfx::BackendTexture& texture, tgfx::ImageOrigin origin)
    : device(std::move(device)), texture(texture), origin(origin) {
}

std::shared_ptr<tgfx::Surface> TextureDrawable::createSurface(tgfx::Context* context) {
  return tgfx::Surface::MakeFrom(context, texture, origin);
}

OffscreenDrawable::OffscreenDrawable(int width, int height, std::shared_ptr<tgfx::Device> device)
    : _width(width), _height(height), device(std::move(device)) {
}

std::shared_ptr<tgfx::Surface> OffscreenDrawable::createSurface(tgfx::Context* context) {
#ifdef __APPLE__
  return tgfx::Surface::Make(context, _width, _height, tgfx::ColorType::BGRA_8888);
#else
  return tgfx::Surface::Make(context, _width, _height);
#endif
}
}  // namespace pag
