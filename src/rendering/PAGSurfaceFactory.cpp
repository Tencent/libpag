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
  auto adopted = Devices::AdoptCurrent();
  auto drawable =
      RenderTargetDrawable::MakeFrom(adopted.device, ToTGFX(renderTarget), ToTGFX(origin));
  if (drawable == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<PAGSurface>(new PAGSurface(std::move(drawable), adopted.externalContext));
}

std::shared_ptr<PAGSurface> PAGSurface::MakeFrom(const BackendTexture& texture, ImageOrigin origin,
                                                 bool forAsyncThread) {
  std::shared_ptr<tgfx::Device> device = nullptr;
  bool externalContext = false;
  if (forAsyncThread) {
    // Prefer a share-context device derived from the caller's current host context so the async
    // worker can access the caller's external texture; if no host context is current on this
    // thread, fall back to a standalone default device (matches the original behavior of
    // GLDevice::Make(nullptr), which returned an independent device).
    device = Devices::MakeForAsyncThread();
    if (device == nullptr) {
      device = Devices::MakeDefault();
    }
  }
  if (device == nullptr) {
    auto adopted = Devices::AdoptCurrent();
    device = std::move(adopted.device);
    externalContext = adopted.externalContext;
  }
  auto drawable = TextureDrawable::MakeFrom(device, ToTGFX(texture), ToTGFX(origin));
  if (drawable == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<PAGSurface>(new PAGSurface(std::move(drawable), externalContext));
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
