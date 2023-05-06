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

#include "RenderTargetDrawable.h"

namespace pag {
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
    : device(std::move(device)), renderTarget(renderTarget), origin(origin) {
}

std::shared_ptr<tgfx::Surface> RenderTargetDrawable::onCreateSurface(tgfx::Context* context) {
  return tgfx::Surface::MakeFrom(context, renderTarget, origin);
}
}  // namespace pag
