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

#include "base/PAGTest.h"

#if defined(TGFX_USE_D3D12)

#include <d3d12.h>
#include <dxgiformat.h>
#include <cstdint>
#include <thread>
#include <vector>
#include "base/utils/TGFXCast.h"
#include "pag/pag.h"
#include "rendering/gpu/Devices.h"
#include "tgfx/gpu/Context.h"
#include "tgfx/gpu/GPU.h"
#include "tgfx/gpu/Texture.h"
#include "tgfx/gpu/d3d12/D3D12Device.h"
#include "utils/D3D12TestUtils.h"

namespace pag {

static ID3D12Resource* GetNativeResource(const tgfx::BackendTexture& texture) {
  tgfx::D3D12TextureInfo info = {};
  if (!texture.getD3D12TextureInfo(&info)) {
    return nullptr;
  }
  return const_cast<ID3D12Resource*>(static_cast<const ID3D12Resource*>(info.resource));
}

static ID3D12Device* GetNativeDevice(ID3D12Resource* resource) {
  if (resource == nullptr) {
    return nullptr;
  }
  ID3D12Device* device = nullptr;
  if (FAILED(resource->GetDevice(IID_PPV_ARGS(&device)))) {
    return nullptr;
  }
  return device;
}

static ID3D12Resource* CreateNativeResource(ID3D12Device* device, DXGI_FORMAT format,
                                            D3D12_RESOURCE_FLAGS flags, int width, int height) {
  if (device == nullptr) {
    return nullptr;
  }
  D3D12_HEAP_PROPERTIES heap = {};
  heap.Type = D3D12_HEAP_TYPE_DEFAULT;
  D3D12_RESOURCE_DESC desc = {};
  desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  desc.Width = static_cast<UINT64>(width);
  desc.Height = static_cast<UINT>(height);
  desc.DepthOrArraySize = 1;
  desc.MipLevels = 1;
  desc.Format = format;
  desc.SampleDesc.Count = 1;
  desc.Flags = flags;
  ID3D12Resource* resource = nullptr;
  if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                                             D3D12_RESOURCE_STATE_COMMON, nullptr,
                                             IID_PPV_ARGS(&resource)))) {
    return nullptr;
  }
  return resource;
}

struct RegistryLookupTask {
  const tgfx::BackendTexture* texture = nullptr;
  std::shared_ptr<tgfx::Device>* result = nullptr;

  void operator()() const {
    *result = Devices::MakeForTexture(*texture);
  }
};

PAG_TEST(D3D12Test, BackendTextureRoundTrip) {
  D3D12TextureInfo info = {};
  BackendTexture texture(info, 16, 16);
  EXPECT_EQ(texture.backend(), Backend::D3D12);
  EXPECT_TRUE(texture.isValid());
  D3D12TextureInfo result = {};
  EXPECT_TRUE(texture.getD3D12TextureInfo(&result));
  EXPECT_EQ(result.resource, nullptr);
  EXPECT_FALSE(texture.getD3D12TextureInfo(nullptr));
  BackendRenderTarget renderTarget(info, 16, 16);
  EXPECT_FALSE(renderTarget.getD3D12TextureInfo(nullptr));
  MtlTextureInfo metalInfo = {};
  EXPECT_FALSE(texture.getMtlTextureInfo(&metalInfo));
}

PAG_TEST(D3D12Test, SameNativeDeviceReturnsSameDevice) {
  DeviceLockScope scope;
  ASSERT_TRUE(static_cast<bool>(scope));
  auto context = scope.context();
  auto first = MakeD3D12Texture(context, 8, 8, tgfx::TextureUsage::TEXTURE_BINDING);
  auto second = MakeD3D12Texture(context, 8, 8, tgfx::TextureUsage::TEXTURE_BINDING);
  ASSERT_TRUE(first != nullptr);
  ASSERT_TRUE(second != nullptr);
  auto firstDevice = Devices::MakeForTexture(first->getBackendTexture());
  auto secondDevice = Devices::MakeForTexture(second->getBackendTexture());
  ASSERT_TRUE(firstDevice != nullptr);
  ASSERT_TRUE(secondDevice != nullptr);
  EXPECT_EQ(firstDevice.get(), secondDevice.get());
  EXPECT_EQ(firstDevice->uniqueID(), secondDevice->uniqueID());
  auto defaultDevice = Devices::MakeDefault();
  ASSERT_TRUE(defaultDevice != nullptr);
  EXPECT_EQ(defaultDevice.get(), firstDevice.get());
}

static HRESULT ForceRemovedDevice(void*) {
  return DXGI_ERROR_DEVICE_REMOVED;
}

/**
 * A removed default device must be evicted and replaced instead of being returned forever.
 */
PAG_TEST(D3D12Test, RemovedDefaultDeviceIsRebuilt) {
  auto before = Devices::MakeDefault();
  ASSERT_TRUE(before != nullptr);
  Devices::SetDeviceHealthProbeForTesting(ForceRemovedDevice);
  auto after = Devices::MakeDefault();
  Devices::SetDeviceHealthProbeForTesting(nullptr);
  ASSERT_TRUE(after != nullptr);
  EXPECT_NE(before.get(), after.get());
  DeviceLockScope afterScope(after);
  EXPECT_TRUE(static_cast<bool>(afterScope));
  auto stable = Devices::MakeDefault();
  ASSERT_TRUE(stable != nullptr);
  EXPECT_EQ(stable.get(), after.get());
}

PAG_TEST(D3D12Test, HealthyDeviceReportsHealthy) {
  DeviceLockScope scope;
  ASSERT_TRUE(static_cast<bool>(scope));
  EXPECT_TRUE(Devices::IsDeviceHealthy(scope.context()));
}

/**
 * An identity cached for an external device must not keep that device alive after its owner exits.
 */
PAG_TEST(D3D12Test, ContextIdentityCacheDoesNotRetainDevice) {
  auto cacheCountBefore = Devices::GetContextIdentityCacheCountForTesting();
  auto device = tgfx::D3D12Device::MakeWarp();
  ASSERT_TRUE(device != nullptr);
  std::weak_ptr<tgfx::Device> weakDevice = device;
  {
    DeviceLockScope scope(device);
    ASSERT_TRUE(static_cast<bool>(scope));
    ASSERT_TRUE(Devices::IsDeviceHealthy(scope.context()));
    EXPECT_EQ(Devices::GetContextIdentityCacheCountForTesting(), cacheCountBefore + 1);
  }
  device.reset();
  EXPECT_TRUE(weakDevice.expired());
  EXPECT_EQ(Devices::GetContextIdentityCacheCountForTesting(), cacheCountBefore);
}

/**
 * Repeated checks and lock scopes on a live external device must reuse its one-time identity probe.
 */
PAG_TEST(D3D12Test, ContextIdentityCacheReusesProbe) {
  auto device = tgfx::D3D12Device::MakeWarp();
  ASSERT_TRUE(device != nullptr);
  auto captureCountBefore = Devices::GetIdentityCaptureCountForTesting();
  for (int index = 0; index < 3; index++) {
    DeviceLockScope scope(device);
    ASSERT_TRUE(static_cast<bool>(scope));
    EXPECT_TRUE(Devices::IsDeviceHealthy(scope.context()));
    EXPECT_TRUE(Devices::IsDeviceHealthy(scope.context()));
    EXPECT_EQ(Devices::GetIdentityCaptureCountForTesting(), captureCountBefore + 1);
  }
}

PAG_TEST(D3D12Test, ResourceUsageValidation) {
  DeviceLockScope scope;
  ASSERT_TRUE(static_cast<bool>(scope));
  auto context = scope.context();
  auto sampled = MakeD3D12Texture(context, 8, 8, tgfx::TextureUsage::TEXTURE_BINDING);
  ASSERT_TRUE(sampled != nullptr);
  auto sampledTexture = sampled->getBackendTexture();
  EXPECT_TRUE(Devices::ValidateExternalResource(sampledTexture, ExternalResourceUsage::Sampled));
  // tgfx enables ALLOW_RENDER_TARGET for every renderable colour texture, so a PAG texture
  // created for sampling is still a valid render target from the native flags' point of view.

  auto nativeDevice = GetNativeDevice(GetNativeResource(sampledTexture));
  ASSERT_TRUE(nativeDevice != nullptr);
  auto sampledOnly = CreateNativeResource(nativeDevice, DXGI_FORMAT_R8G8B8A8_UNORM,
                                          D3D12_RESOURCE_FLAG_NONE, 8, 8);
  ASSERT_TRUE(sampledOnly != nullptr);
  D3D12TextureInfo sampledOnlyInfo = {};
  sampledOnlyInfo.resource = sampledOnly;
  BackendTexture sampledOnlyTexture(sampledOnlyInfo, 8, 8);
  EXPECT_TRUE(Devices::ValidateExternalResource(ToTGFX(sampledOnlyTexture),
                                                ExternalResourceUsage::Sampled));
  EXPECT_FALSE(Devices::ValidateExternalResource(ToTGFX(sampledOnlyTexture),
                                                 ExternalResourceUsage::RenderTarget));
  sampledOnly->Release();
  nativeDevice->Release();

  auto renderTarget = MakeD3D12Texture(
      context, 8, 8, tgfx::TextureUsage::TEXTURE_BINDING | tgfx::TextureUsage::RENDER_ATTACHMENT);
  ASSERT_TRUE(renderTarget != nullptr);
  auto renderTargetTexture = renderTarget->getBackendTexture();
  EXPECT_TRUE(
      Devices::ValidateExternalResource(renderTargetTexture, ExternalResourceUsage::Sampled));
  EXPECT_TRUE(Devices::ValidateExternalResource(renderTarget->getBackendRenderTarget()));

  D3D12TextureInfo renderTargetInfo = {};
  ASSERT_TRUE(ToPAG(renderTargetTexture).getD3D12TextureInfo(&renderTargetInfo));
  BackendTexture oversized(renderTargetInfo, 64, 64);
  EXPECT_FALSE(Devices::ValidateExternalResource(ToTGFX(oversized),
                                                 ExternalResourceUsage::SampledRenderTarget));
}

PAG_TEST(D3D12Test, TypelessResourceRejected) {
  DeviceLockScope scope;
  ASSERT_TRUE(static_cast<bool>(scope));
  auto context = scope.context();
  auto source = MakeD3D12Texture(context, 8, 8, tgfx::TextureUsage::TEXTURE_BINDING);
  ASSERT_TRUE(source != nullptr);
  auto resource = GetNativeResource(source->getBackendTexture());
  ASSERT_TRUE(resource != nullptr);
  ID3D12Device* device = nullptr;
  auto deviceResult = resource->GetDevice(IID_PPV_ARGS(&device));
  ASSERT_TRUE(SUCCEEDED(deviceResult)) << "GetDevice HRESULT=" << static_cast<long>(deviceResult);
  D3D12_HEAP_PROPERTIES heap = {};
  heap.Type = D3D12_HEAP_TYPE_DEFAULT;
  D3D12_RESOURCE_DESC desc = {};
  desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  desc.Width = 8;
  desc.Height = 8;
  desc.DepthOrArraySize = 1;
  desc.MipLevels = 1;
  desc.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
  desc.SampleDesc.Count = 1;
  ID3D12Resource* typeless = nullptr;
  auto hr = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                                            D3D12_RESOURCE_STATE_COMMON, nullptr,
                                            IID_PPV_ARGS(&typeless));
  ASSERT_TRUE(SUCCEEDED(hr)) << "HRESULT=" << static_cast<long>(hr);
  D3D12TextureInfo info = {};
  info.resource = typeless;
  BackendTexture texture(info, 8, 8);
  EXPECT_FALSE(Devices::ValidateExternalResource(ToTGFX(texture), ExternalResourceUsage::Sampled));
  typeless->Release();
  device->Release();
}

/**
 * Covers concurrent first-time resolution of an unregistered native device: the registry must
 * publish exactly one tgfx device even when every worker misses the cache.
 */
PAG_TEST(D3D12Test, DeviceRegistryIsThreadSafe) {
  SecondaryDeviceTexture secondary = {};
  if (!secondary.init(8, 8)) {
    GTEST_SKIP() << "Failed to create a secondary D3D12 device, HRESULT="
                 << static_cast<long>(secondary.lastError);
  }
  constexpr int ThreadCount = 8;
  auto deviceCountBefore = Devices::GetRegisteredDeviceCount();
  auto creationCountBefore = Devices::GetDeviceCreationCountForTesting();
  std::vector<std::shared_ptr<tgfx::Device>> results(ThreadCount);
  std::vector<std::thread> threads = {};
  for (int i = 0; i < ThreadCount; i++) {
    RegistryLookupTask task = {&secondary.texture, &results[static_cast<size_t>(i)]};
    threads.emplace_back(task);
  }
  for (auto& thread : threads) {
    thread.join();
  }
  auto canonical = results.front();
  ASSERT_TRUE(canonical != nullptr);
  for (const auto& result : results) {
    EXPECT_EQ(result.get(), canonical.get());
  }
  EXPECT_EQ(Devices::MakeForTexture(secondary.texture).get(), canonical.get());
  EXPECT_EQ(Devices::GetRegisteredDeviceCount(), deviceCountBefore + 1);
  EXPECT_EQ(Devices::GetDeviceCreationCountForTesting(), creationCountBefore + 1);
  EXPECT_NE(canonical.get(), Devices::MakeDefault().get());
}

}  // namespace pag

#endif  // TGFX_USE_D3D12
