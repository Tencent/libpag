/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#include "pagx/runtime/RenderTargetDrawable.h"

namespace pagx {

std::shared_ptr<RenderTargetDrawable> RenderTargetDrawable::MakeFrom(
    std::shared_ptr<tgfx::Device> device, const tgfx::BackendRenderTarget& renderTarget,
    tgfx::ImageOrigin origin) {
  if (device == nullptr || !renderTarget.isValid()) {
    return nullptr;
  }
  return std::shared_ptr<RenderTargetDrawable>(
      new RenderTargetDrawable(std::move(device), renderTarget, origin));
}

RenderTargetDrawable::RenderTargetDrawable(std::shared_ptr<tgfx::Device> device,
                                           const tgfx::BackendRenderTarget& renderTarget,
                                           tgfx::ImageOrigin origin)
    : _device(std::move(device)), _renderTarget(renderTarget), _origin(origin) {
}

std::shared_ptr<tgfx::Surface> RenderTargetDrawable::getSurface(tgfx::Context* context) {
  if (surface != nullptr) {
    return surface;
  }
  if (context == nullptr) {
    return nullptr;
  }
  surface = tgfx::Surface::MakeFrom(context, _renderTarget, _origin);
  return surface;
}

}  // namespace pagx
