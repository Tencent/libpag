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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#ifdef TGFX_USE_D3D12

#include <memory>
#include "base/utils/TGFXCast.h"
#include "pag/pag.h"
#include "rendering/gpu/Devices.h"
#include "tgfx/gpu/Context.h"
#include "tgfx/gpu/GPU.h"
#include "tgfx/gpu/Texture.h"
#include "utils/DevicePool.h"

namespace pag {

inline std::shared_ptr<tgfx::Texture> MakeD3D12Texture(tgfx::Context* context, int width,
                                                       int height, uint32_t usage) {
  tgfx::TextureDescriptor descriptor(width, height, tgfx::PixelFormat::RGBA_8888, false, 1, usage);
  return context->gpu()->createTexture(descriptor);
}

inline std::shared_ptr<tgfx::Texture> MakeD3D12RenderableTexture(int width, int height) {
  auto device = DevicePool::Make();
  if (device == nullptr) {
    return nullptr;
  }
  std::shared_ptr<tgfx::Texture> texture = nullptr;
  {
    DeviceLockScope scope(device);
    if (!static_cast<bool>(scope)) {
      return nullptr;
    }
    texture = MakeD3D12Texture(
        scope.context(), width, height,
        tgfx::TextureUsage::TEXTURE_BINDING | tgfx::TextureUsage::RENDER_ATTACHMENT);
  }
  return texture;
}

inline BackendRenderTarget MakeD3D12RenderTarget(const std::shared_ptr<tgfx::Texture>& texture,
                                                 int width, int height) {
  tgfx::D3D12TextureInfo textureInfo = {};
  if (!texture->getBackendTexture().getD3D12TextureInfo(&textureInfo)) {
    return BackendRenderTarget();
  }
  D3D12TextureInfo info = {};
  info.resource = const_cast<void*>(textureInfo.resource);
  return BackendRenderTarget(info, width, height);
}

}  // namespace pag

#endif  // TGFX_USE_D3D12
