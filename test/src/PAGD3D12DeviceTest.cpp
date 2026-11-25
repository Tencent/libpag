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
#include <atomic>
#include <cstdint>
#include <cstdlib>
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

static Microsoft::WRL::ComPtr<ID3D12Device> GetNativeDevice(ID3D12Resource* resource) {
  if (resource == nullptr) {
    return nullptr;
  }
  Microsoft::WRL::ComPtr<ID3D12Device> device = nullptr;
  if (FAILED(resource->GetDevice(IID_PPV_ARGS(device.ReleaseAndGetAddressOf())))) {
    return nullptr;
  }
  return device;
}

static Microsoft::WRL::ComPtr<ID3D12Resource> CreateNativeResource(ID3D12Device* device,
                                                                   DXGI_FORMAT format,
                                                                   D3D12_RESOURCE_FLAGS flags,
                                                                   int width, int height) {
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
  Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
  if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                                             D3D12_RESOURCE_STATE_COMMON, nullptr,
                                             IID_PPV_ARGS(resource.ReleaseAndGetAddressOf())))) {
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
  EXPECT_FALSE(Devices::ValidateExternalResource(ToTGFX(texture), ExternalResourceUsage::Sampled));
  EXPECT_FALSE(Devices::ValidateExternalResource(ToTGFX(renderTarget)));
  EXPECT_EQ(PAGImage::FromTexture(texture, ImageOrigin::TopLeft), nullptr);
  EXPECT_EQ(PAGSurface::MakeFrom(texture, ImageOrigin::TopLeft), nullptr);
  EXPECT_FALSE(MakeD3D12RenderTarget(nullptr, 16, 16).isValid());
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

static std::atomic<bool> SimulateNextRemoval = false;

static HRESULT ForceRemovedDevice(void* nativeDevice) {
  if (SimulateNextRemoval.exchange(false)) {
    return DXGI_ERROR_DEVICE_REMOVED;
  }
  return static_cast<ID3D12Device*>(nativeDevice)->GetDeviceRemovedReason();
}

/**
 * Restores the previous health probe even when a test exits early after an assertion failure.
 */
class ScopedDeviceHealthProbe {
 public:
  explicit ScopedDeviceHealthProbe(Devices::DeviceHealthProbe probe)
      : previous(Devices::SetDeviceHealthProbeForTesting(probe)) {
  }

  ~ScopedDeviceHealthProbe() {
    Devices::SetDeviceHealthProbeForTesting(previous);
    SimulateNextRemoval.store(false);
  }

  ScopedDeviceHealthProbe(const ScopedDeviceHealthProbe&) = delete;
  ScopedDeviceHealthProbe& operator=(const ScopedDeviceHealthProbe&) = delete;

 private:
  Devices::DeviceHealthProbe previous = nullptr;
};

/**
 * A simulated removal query must evict the cached wrapper. This does not remove the native GPU.
 */
PAG_TEST(D3D12Test, RemovedDefaultDeviceIsRebuilt) {
  auto before = Devices::MakeDefault();
  ASSERT_TRUE(before != nullptr);
  ScopedDeviceHealthProbe probe(ForceRemovedDevice);
  SimulateNextRemoval.store(true);
  auto after = Devices::MakeDefault();
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
 * The native removal probe must also be observed by health checks on an already locked context.
 */
PAG_TEST(D3D12Test, RemovedDeviceReportsUnhealthy) {
  DeviceLockScope scope;
  ASSERT_TRUE(static_cast<bool>(scope));
  ScopedDeviceHealthProbe probe(ForceRemovedDevice);
  SimulateNextRemoval.store(true);
  EXPECT_FALSE(Devices::IsDeviceHealthy(scope.context()));
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

  Microsoft::WRL::ComPtr<ID3D12Device> nativeDevice =
      GetNativeDevice(GetNativeResource(sampledTexture));
  ASSERT_TRUE(nativeDevice != nullptr);
  Microsoft::WRL::ComPtr<ID3D12Resource> sampledOnly = CreateNativeResource(
      nativeDevice.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_NONE, 8, 8);
  ASSERT_TRUE(sampledOnly != nullptr);
  D3D12TextureInfo sampledOnlyInfo = {};
  sampledOnlyInfo.resource = sampledOnly.Get();
  BackendTexture sampledOnlyTexture(sampledOnlyInfo, 8, 8);
  EXPECT_TRUE(Devices::ValidateExternalResource(ToTGFX(sampledOnlyTexture),
                                                ExternalResourceUsage::Sampled));
  EXPECT_FALSE(Devices::ValidateExternalResource(ToTGFX(sampledOnlyTexture),
                                                 ExternalResourceUsage::RenderTarget));

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
  Microsoft::WRL::ComPtr<ID3D12Device> device = nullptr;
  auto deviceResult = resource->GetDevice(IID_PPV_ARGS(device.ReleaseAndGetAddressOf()));
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
  Microsoft::WRL::ComPtr<ID3D12Resource> typeless = nullptr;
  auto hr = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                                            D3D12_RESOURCE_STATE_COMMON, nullptr,
                                            IID_PPV_ARGS(typeless.ReleaseAndGetAddressOf()));
  ASSERT_TRUE(SUCCEEDED(hr)) << "HRESULT=" << static_cast<long>(hr);
  D3D12TextureInfo info = {};
  info.resource = typeless.Get();
  BackendTexture texture(info, 8, 8);
  EXPECT_FALSE(Devices::ValidateExternalResource(ToTGFX(texture), ExternalResourceUsage::Sampled));
}

/**
 * Concurrent external imports reuse an existing wrapper or create at most one on a cache miss.
 * Per-adapter native singletons may already be registered by another test.
 */
PAG_TEST(D3D12Test, DeviceRegistryIsThreadSafe) {
  ExternalDeviceTexture external = {};
  ASSERT_TRUE(external.init(8, 8))
      << external.lastStage << " HRESULT=" << static_cast<long>(external.lastError);
  constexpr int ThreadCount = 8;
  auto deviceCountBefore = Devices::GetRegisteredDeviceCount();
  auto creationCountBefore = Devices::GetDeviceCreationCountForTesting();
  std::vector<std::shared_ptr<tgfx::Device>> results(ThreadCount);
  std::vector<std::thread> threads = {};
  for (int i = 0; i < ThreadCount; i++) {
    RegistryLookupTask task = {&external.texture, &results[static_cast<size_t>(i)]};
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
  EXPECT_EQ(Devices::MakeForTexture(external.texture).get(), canonical.get());
  EXPECT_LE(Devices::GetRegisteredDeviceCount(), deviceCountBefore + 1);
  EXPECT_LE(Devices::GetDeviceCreationCountForTesting(), creationCountBefore + 1);
#ifdef PAG_D3D12_USE_WARP
  Microsoft::WRL::ComPtr<IDXGIFactory4> factory = nullptr;
  Microsoft::WRL::ComPtr<IDXGIAdapter> warpAdapter = nullptr;
  auto factoryResult = CreateDXGIFactory1(IID_PPV_ARGS(factory.ReleaseAndGetAddressOf()));
  ASSERT_TRUE(SUCCEEDED(factoryResult)) << "CreateDXGIFactory1 HRESULT=" << factoryResult;
  auto adapterResult = factory->EnumWarpAdapter(IID_PPV_ARGS(warpAdapter.ReleaseAndGetAddressOf()));
  ASSERT_TRUE(SUCCEEDED(adapterResult)) << "EnumWarpAdapter HRESULT=" << adapterResult;
  DXGI_ADAPTER_DESC adapterDescription = {};
  auto descriptionResult = warpAdapter->GetDesc(&adapterDescription);
  ASSERT_TRUE(SUCCEEDED(descriptionResult)) << "GetDesc HRESULT=" << descriptionResult;
  auto actualAdapter = external.device->GetAdapterLuid();
  EXPECT_EQ(actualAdapter.LowPart, adapterDescription.AdapterLuid.LowPart);
  EXPECT_EQ(actualAdapter.HighPart, adapterDescription.AdapterLuid.HighPart);
#endif
}

#if defined(PAG_TEST_ACCESS_PRIVATE) && GTEST_HAS_DEATH_TEST
static int VerifyLostWrapperRecovery() {
  auto before = Devices::MakeDefault();
  if (before == nullptr) {
    return 1;
  }
  std::shared_ptr<tgfx::Texture> texture = nullptr;
  {
    DeviceLockScope scope(before);
    if (!scope) {
      return 2;
    }
    texture = MakeD3D12Texture(
        scope.context(), 8, 8,
        tgfx::TextureUsage::TEXTURE_BINDING | tgfx::TextureUsage::RENDER_ATTACHMENT);
  }
  if (texture == nullptr) {
    return 3;
  }
  auto backendTexture = texture->getBackendTexture();
  before->_contextLost.store(true);
  if (Devices::LockContext(before) != nullptr) {
    before->unlock();
    return 4;
  }
  auto creationCountBefore = Devices::GetDeviceCreationCountForTesting();
  constexpr int ThreadCount = 8;
  std::vector<std::shared_ptr<tgfx::Device>> results(ThreadCount);
  std::vector<std::thread> threads = {};
  for (int index = 0; index < ThreadCount; index++) {
    RegistryLookupTask task = {&backendTexture, &results[static_cast<size_t>(index)]};
    threads.emplace_back(task);
  }
  for (auto& thread : threads) {
    thread.join();
  }
  auto canonical = results.front();
  if (canonical == nullptr || canonical.get() == before.get()) {
    return 5;
  }
  for (const auto& result : results) {
    if (result.get() != canonical.get()) {
      return 6;
    }
  }
  if (Devices::GetDeviceCreationCountForTesting() != creationCountBefore + 1) {
    return 7;
  }
  {
    DeviceLockScope scope(canonical);
    if (!scope || !Devices::IsDeviceHealthy(scope.context())) {
      return 8;
    }
  }
  auto surface = PAGSurface::MakeFrom(ToPAG(backendTexture), ImageOrigin::TopLeft);
  if (surface == nullptr || !surface->clearAll() || !surface->waitUntilCompleted()) {
    return 9;
  }
  return 0;
}

/**
 * Exercises real wrapper lock failure and concurrent replacement in an isolated process so the
 * permanently lost device cannot contaminate another test's cached device. This is not native TDR.
 */
PAG_TEST(D3D12Test, WrapperLockFailureIsEvictedAndRebuilt) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  EXPECT_EXIT(std::exit(VerifyLostWrapperRecovery()), ::testing::ExitedWithCode(0), "");
}
#endif

}  // namespace pag

#endif  // TGFX_USE_D3D12
