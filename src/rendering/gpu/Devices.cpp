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

#include "rendering/gpu/Devices.h"
#include "base/utils/Log.h"

namespace pag {

Devices::AdoptedDevice Devices::ResolveForExternalResource(const tgfx::BackendTexture& texture,
                                                           bool forAsyncThread) {
  if (!texture.isValid()) {
    LOGE("Devices::ResolveForExternalResource() The texture is invalid.");
    return {};
  }
  if (!SupportsBackend(texture.backend())) {
    LOGE(
        "Devices::ResolveForExternalResource() The texture belongs to a different GPU backend "
        "than the current libpag build.");
    return {};
  }
  if (forAsyncThread) {
    // The worker renders into the caller's texture, so prefer a device that can actually access
    // it: backends reaching a device back-reference from the texture handle (Metal / D3D12)
    // derive the worker device from the texture itself, while GL derives a share-context device
    // from the calling thread's current context.
    AdoptedDevice adopted = {MakeForAsyncThread(texture), false};
    if (adopted.device == nullptr) {
      adopted.device = MakeDefault();
    }
    return adopted;
  }
  auto adopted = AdoptCurrent();
  if (adopted.device == nullptr) {
    // Backends without a thread-local "current context" (Metal / D3D12) walk back from the
    // caller's texture; Vulkan / WebGPU fall back to MakeDefault() inside MakeForTexture().
    adopted.device = MakeForTexture(texture);
  }
  return adopted;
}

Devices::AdoptedDevice Devices::ResolveForExternalResource(
    const tgfx::BackendRenderTarget& renderTarget) {
  if (!renderTarget.isValid()) {
    LOGE("Devices::ResolveForExternalResource() The render target is invalid.");
    return {};
  }
  if (!SupportsBackend(renderTarget.backend())) {
    LOGE(
        "Devices::ResolveForExternalResource() The render target belongs to a different GPU "
        "backend than the current libpag build.");
    return {};
  }
  auto adopted = AdoptCurrent();
  if (adopted.device == nullptr) {
    adopted.device = MakeForTexture(renderTarget);
  }
  return adopted;
}

}  // namespace pag
