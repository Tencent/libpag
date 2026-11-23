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

#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
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

/**
 * A native device that is intentionally not the registry's default entry. The default device is
 * covered by the other D3D12 tests (including WARP builds); this secondary device only exercises
 * non-default registration and ownership. Prefer a different adapter so the two devices cannot
 * collapse into the same registry entry.
 */
struct SecondaryDeviceTexture {
  Microsoft::WRL::ComPtr<ID3D12Device> device = nullptr;
  Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
  tgfx::BackendTexture texture = {};

  bool init(int width, int height) {
    auto result = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0,
                                    IID_PPV_ARGS(device.ReleaseAndGetAddressOf()));
    if (FAILED(result)) {
      Microsoft::WRL::ComPtr<IDXGIFactory4> factory = nullptr;
      Microsoft::WRL::ComPtr<IDXGIAdapter> warpAdapter = nullptr;
      if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(factory.ReleaseAndGetAddressOf()))) ||
          FAILED(factory->EnumWarpAdapter(IID_PPV_ARGS(warpAdapter.ReleaseAndGetAddressOf())))) {
        return false;
      }
      result = D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                 IID_PPV_ARGS(device.ReleaseAndGetAddressOf()));
    }
    if (FAILED(result)) {
      return false;
    }
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = static_cast<UINT64>(width);
    desc.Height = static_cast<UINT>(height);
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                                               D3D12_RESOURCE_STATE_COMMON, nullptr,
                                               IID_PPV_ARGS(resource.ReleaseAndGetAddressOf())))) {
      return false;
    }
    tgfx::D3D12TextureInfo info = {};
    info.resource = resource.Get();
    info.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texture = tgfx::BackendTexture(info, width, height);
    return true;
  }
};

}  // namespace pag

#endif  // TGFX_USE_D3D12
