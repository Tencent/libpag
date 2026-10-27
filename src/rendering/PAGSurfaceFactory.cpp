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
//  and limitations under the license.˙
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <thread>
#include "base/utils/TGFXCast.h"
#include "pag/pag.h"
#include "rendering/drawables/HardwareBufferDrawable.h"
#include "rendering/drawables/OffscreenDrawable.h"
#include "rendering/drawables/RenderTargetDrawable.h"
#include "rendering/drawables/TextureDrawable.h"
#include "rendering/gpu/Devices.h"

namespace pag {
std::shared_ptr<PAGSurface> PAGSurface::MakeFrom(std::shared_ptr<Drawable> drawable) {
  if (drawable == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<PAGSurface>(new PAGSurface(std::move(drawable)));
}

std::shared_ptr<PAGSurface> PAGSurface::MakeFrom(const BackendRenderTarget& renderTarget,
                                                 ImageOrigin origin) {
  auto tgfxRenderTarget = ToTGFX(renderTarget);
  auto adopted = Devices::ResolveForExternalResource(tgfxRenderTarget);
  auto drawable = RenderTargetDrawable::MakeFrom(adopted.device, tgfxRenderTarget, ToTGFX(origin));
  if (drawable == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<PAGSurface>(new PAGSurface(std::move(drawable), adopted.externalContext));
}

std::shared_ptr<PAGSurface> PAGSurface::MakeFrom(const BackendTexture& texture, ImageOrigin origin,
                                                 bool forAsyncThread) {
  auto tgfxTexture = ToTGFX(texture);
  auto adopted = Devices::ResolveForExternalResource(tgfxTexture, forAsyncThread);
  auto drawable = TextureDrawable::MakeFrom(adopted.device, tgfxTexture, ToTGFX(origin));
  if (drawable == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<PAGSurface>(new PAGSurface(std::move(drawable), adopted.externalContext));
}

std::shared_ptr<PAGSurface> PAGSurface::MakeOffscreen(int width, int height) {
  auto drawable = OffscreenDrawable::Make(width, height);
  return MakeFrom(drawable);
}

std::shared_ptr<PAGSurface> PAGSurface::MakeFrom(HardwareBufferRef hardwareBuffer) {
  auto drawable = HardwareBufferDrawable::MakeFrom(hardwareBuffer);
  return MakeFrom(drawable);
}

}  // namespace pag
