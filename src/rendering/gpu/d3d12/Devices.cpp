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

#include "rendering/gpu/Devices.h"
#include <d3d12.h>
#include <dxgiformat.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "base/utils/Log.h"
#include "tgfx/gpu/Context.h"
#include "tgfx/gpu/GPU.h"
#include "tgfx/gpu/Texture.h"
#include "tgfx/gpu/d3d12/D3D12Device.h"

namespace pag {

namespace {

#include "rendering/gpu/d3d12/DeviceRegistry.h"

bool IsSupportedFormat(DXGI_FORMAT format) {
  switch (format) {
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
      return true;
    default:
      return false;
  }
}

bool ValidateResource(void* resourcePtr, int width, int height, bool sampled, bool renderTarget) {
  if (resourcePtr == nullptr || width <= 0 || height <= 0 || (!sampled && !renderTarget)) {
    return false;
  }
  ID3D12Resource* resource = nullptr;
  if (FAILED(static_cast<ID3D12Resource*>(resourcePtr)->QueryInterface(IID_PPV_ARGS(&resource))) ||
      resource == nullptr) {
    return false;
  }
  auto desc = resource->GetDesc();
  resource->Release();
  if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || desc.DepthOrArraySize != 1 ||
      desc.SampleDesc.Count != 1 || desc.MipLevels == 0 || !IsSupportedFormat(desc.Format)) {
    return false;
  }
  if (static_cast<UINT64>(width) > desc.Width || static_cast<UINT>(height) > desc.Height) {
    return false;
  }
  if (sampled && (desc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) != 0) {
    return false;
  }
  if (renderTarget && (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) == 0) {
    return false;
  }
  return true;
}

/**
 * Retains the canonical IUnknown identity of the device that owns an external texture so the
 * identity can be compared later without relying on the caller's lifetime.
 */
class D3D12ExternalDeviceRef : public ExternalDeviceRef {
 public:
  explicit D3D12ExternalDeviceRef(IUnknown* nativeIdentity) : identity(nativeIdentity) {
  }

  ~D3D12ExternalDeviceRef() override {
    if (identity != nullptr) {
      identity->Release();
    }
  }

  D3D12ExternalDeviceRef(const D3D12ExternalDeviceRef&) = delete;
  D3D12ExternalDeviceRef& operator=(const D3D12ExternalDeviceRef&) = delete;

  IUnknown* identity = nullptr;
};

}  // namespace

std::shared_ptr<tgfx::Device> Devices::MakeDefault() {
  {
    auto& registry = GetRegistry();
    std::shared_ptr<DeviceEntry> entry = nullptr;
    {
      std::lock_guard<std::mutex> autoLock(registry.locker);
      entry = registry.defaultEntry.lock();
    }
    if (entry != nullptr) {
      if (IsEntryHealthy(entry)) {
        return MakeAliasedDevice(entry);
      }
      std::lock_guard<std::mutex> autoLock(registry.locker);
      EraseEntryLocked(registry, entry.get());
    }
  }
  std::lock_guard<std::mutex> creationLock(GetCreationLocker());
  {
    auto& registry = GetRegistry();
    std::lock_guard<std::mutex> autoLock(registry.locker);
    auto entry = registry.defaultEntry.lock();
    if (entry != nullptr) {
      if (IsEntryHealthyLocked(registry, entry)) {
        return MakeAliasedDevice(entry);
      }
      EraseEntryLocked(registry, entry.get());
    }
  }
  std::shared_ptr<tgfx::D3D12Device> candidate = nullptr;
#ifdef PAG_D3D12_USE_WARP
  candidate = tgfx::D3D12Device::MakeWarp();
#else
  candidate = tgfx::D3D12Device::Make();
  if (candidate == nullptr) {
    LOGI("Devices::MakeDefault() No D3D12 hardware adapter, falling back to WARP.");
    candidate = tgfx::D3D12Device::MakeWarp();
  }
#endif
  if (candidate == nullptr) {
    LOGE("Devices::MakeDefault() Failed to create a D3D12 device.");
    return nullptr;
  }
  auto entry = RegisterNewDevice(std::move(candidate));
  if (entry == nullptr) {
    return nullptr;
  }
  std::shared_ptr<DeviceEntry> selected = nullptr;
  {
    auto& registry = GetRegistry();
    std::lock_guard<std::mutex> autoLock(registry.locker);
    selected = registry.defaultEntry.lock();
    if (selected == nullptr) {
      selected = entry;
      registry.defaultEntry = selected;
    }
  }
  return MakeAliasedDevice(selected);
}

Devices::DeviceHealthProbe Devices::SetDeviceHealthProbeForTesting(DeviceHealthProbe probe) {
  return DeviceHealthProbeOverride.exchange(probe);
}

size_t Devices::GetRegisteredDeviceCount() {
  return CountRegisteredDevices();
}

void Devices::TrackContextDevice(const std::shared_ptr<tgfx::Device>& device) {
  if (device == nullptr) {
    return;
  }
  auto& registry = GetRegistry();
  std::lock_guard<std::mutex> autoLock(registry.locker);
  PurgeContextIdentitiesLocked(registry);
  auto& record = registry.contextIdentities[device->uniqueID()];
  record.owner = device;
}

tgfx::Context* Devices::LockContext(const std::shared_ptr<tgfx::Device>& device) {
  if (device == nullptr) {
    return nullptr;
  }
  auto context = device->lockContext();
  if (context == nullptr) {
    MarkWrapperLost(device);
  }
  return context;
}

size_t Devices::GetContextIdentityCacheCountForTesting() {
  auto& registry = GetRegistry();
  std::lock_guard<std::mutex> autoLock(registry.locker);
  PurgeContextIdentitiesLocked(registry);
  return registry.contextIdentities.size();
}

size_t Devices::GetIdentityCaptureCountForTesting() {
  auto& registry = GetRegistry();
  std::lock_guard<std::mutex> autoLock(registry.locker);
  return registry.identityCaptureCount;
}

size_t Devices::GetDeviceCreationCountForTesting() {
  auto& registry = GetRegistry();
  std::lock_guard<std::mutex> autoLock(registry.locker);
  return registry.deviceCreationCount;
}

bool Devices::SupportsBackend(tgfx::Backend backend) {
  return backend == tgfx::Backend::D3D12;
}

bool Devices::ValidateExternalResource(const tgfx::BackendTexture& texture,
                                       ExternalResourceUsage usage) {
  if (!SupportsBackend(texture.backend())) {
    return false;
  }
  tgfx::D3D12TextureInfo info = {};
  if (!texture.getD3D12TextureInfo(&info)) {
    return false;
  }
  bool sampled = false;
  bool renderTarget = false;
  switch (usage) {
    case ExternalResourceUsage::Sampled:
      sampled = true;
      break;
    case ExternalResourceUsage::RenderTarget:
      renderTarget = true;
      break;
    case ExternalResourceUsage::SampledRenderTarget:
      sampled = true;
      renderTarget = true;
      break;
  }
  return ValidateResource(GetResource(info), texture.width(), texture.height(), sampled,
                          renderTarget);
}

bool Devices::ValidateExternalResource(const tgfx::BackendRenderTarget& renderTarget) {
  if (!SupportsBackend(renderTarget.backend())) {
    return false;
  }
  tgfx::D3D12TextureInfo info = {};
  if (!renderTarget.getD3D12TextureInfo(&info)) {
    return false;
  }
  return ValidateResource(GetResource(info), renderTarget.width(), renderTarget.height(), false,
                          true);
}

std::shared_ptr<tgfx::Device> Devices::MakeForAsyncThread() {
  return MakeDefault();
}

std::shared_ptr<tgfx::Device> Devices::MakeForSharedContext(void* sharedContext) {
  if (sharedContext != nullptr) {
    LOGE("Devices::MakeForSharedContext() D3D12 does not support share-context handles.");
    return nullptr;
  }
  return MakeDefault();
}

std::shared_ptr<tgfx::Device> Devices::MakeForAsyncThread(const tgfx::BackendTexture& texture) {
  return MakeForTexture(texture);
}

Devices::AdoptedDevice Devices::AdoptCurrent() {
  return {};
}

std::shared_ptr<tgfx::Device> Devices::MakeForTexture(const tgfx::BackendTexture& texture) {
  tgfx::D3D12TextureInfo info = {};
  if (!texture.getD3D12TextureInfo(&info)) {
    return nullptr;
  }
  return MakeAliasedDevice(GetOrCreateEntry(GetResource(info)));
}

std::shared_ptr<tgfx::Device> Devices::MakeForTexture(
    const tgfx::BackendRenderTarget& renderTarget) {
  tgfx::D3D12TextureInfo info = {};
  if (!renderTarget.getD3D12TextureInfo(&info)) {
    return nullptr;
  }
  return MakeAliasedDevice(GetOrCreateEntry(GetResource(info)));
}

std::shared_ptr<ExternalDeviceRef> Devices::CaptureFromTexture(
    const tgfx::BackendTexture& texture) {
  tgfx::D3D12TextureInfo info = {};
  if (!texture.getD3D12TextureInfo(&info)) {
    return nullptr;
  }
  auto identity = CaptureIdentityFromResource(GetResource(info));
  if (identity == nullptr) {
    return nullptr;
  }
  return std::make_shared<D3D12ExternalDeviceRef>(identity);
}

bool Devices::RequiresCapturedIdentity() {
  return true;
}

bool Devices::IsDeviceHealthy(tgfx::Context* context) {
  if (context == nullptr) {
    return false;
  }
  ID3D12Device* nativeDevice = nullptr;
  auto entry = RegisterContextDevice(context);
  if (entry != nullptr && entry->identity != nullptr) {
    entry->identity->QueryInterface(IID_PPV_ARGS(&nativeDevice));
  }
  if (nativeDevice == nullptr) {
    auto identity = CaptureIdentityFromContext(context);
    if (identity == nullptr) {
      return false;
    }
    identity->QueryInterface(IID_PPV_ARGS(&nativeDevice));
    identity->Release();
  }
  if (nativeDevice == nullptr) {
    return false;
  }
  auto reason = QueryDeviceRemovedReason(nativeDevice);
  nativeDevice->Release();
  if (FAILED(reason)) {
    return false;
  }
  return !IsWrapperLost(context->device());
}

bool Devices::CanSampleFrom(tgfx::Context* context, const ExternalDeviceRef* deviceRef) {
  if (deviceRef == nullptr) {
    return false;
  }
  if (context == nullptr) {
    return false;
  }
  auto reference = static_cast<const D3D12ExternalDeviceRef*>(deviceRef);
  auto entry = RegisterContextDevice(context);
  if (entry != nullptr) {
    return reference->identity == entry->identity;
  }
  auto identity = CaptureIdentityFromContext(context);
  if (identity == nullptr) {
    return false;
  }
  auto matches = reference->identity == identity;
  identity->Release();
  return matches;
}

}  // namespace pag
