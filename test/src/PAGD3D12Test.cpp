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
#include <dxgi1_4.h>
#include <dxgiformat.h>
#include <wrl/client.h>
#include <cstdint>
#include <thread>
#include <vector>
#include "base/utils/TGFXCast.h"
#include "pag/pag.h"
#include "pagx/PAGImage.h"
#include "pagx/PAGSurface.h"
#include "rendering/gpu/Devices.h"
#include "tgfx/gpu/Context.h"
#include "tgfx/gpu/GPU.h"
#include "tgfx/gpu/Texture.h"
#include "utils/D3D12TestUtils.h"

namespace pag {

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

PAG_TEST(D3D12Test, HealthyDeviceReportsHealthy) {
  DeviceLockScope scope;
  ASSERT_TRUE(static_cast<bool>(scope));
  EXPECT_TRUE(Devices::IsDeviceHealthy(scope.context()));
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

PAG_TEST(D3D12Test, SurfaceFromTextureAndCompletion) {
  auto device = Devices::MakeDefault();
  ASSERT_TRUE(device != nullptr);
  std::shared_ptr<tgfx::Texture> texture = nullptr;
  {
    DeviceLockScope scope(device);
    ASSERT_TRUE(static_cast<bool>(scope));
    texture = MakeD3D12Texture(
        scope.context(), 32, 32,
        tgfx::TextureUsage::TEXTURE_BINDING | tgfx::TextureUsage::RENDER_ATTACHMENT);
  }
  ASSERT_TRUE(texture != nullptr);
  auto tgfxTexture = texture->getBackendTexture();
  tgfx::D3D12TextureInfo rawInfo = {};
  ASSERT_TRUE(tgfxTexture.getD3D12TextureInfo(&rawInfo));
  ASSERT_NE(rawInfo.resource, nullptr);
  EXPECT_NE(rawInfo.format, 0u);
  auto pagTexture = ToPAG(tgfxTexture);
  ASSERT_TRUE(pagTexture.isValid());
  EXPECT_EQ(pagTexture.backend(), Backend::D3D12);
  auto roundTrip = ToTGFX(pagTexture);
  ASSERT_TRUE(roundTrip.isValid());
  auto surface = PAGSurface::MakeFrom(pagTexture, ImageOrigin::TopLeft);
  ASSERT_TRUE(surface != nullptr);
  EXPECT_TRUE(surface->clearAll());
  EXPECT_TRUE(surface->waitUntilCompleted());
  std::vector<uint8_t> pixels(32 * 32 * 4, 0);
  EXPECT_TRUE(
      surface->readPixels(ColorType::RGBA_8888, AlphaType::Premultiplied, pixels.data(), 32 * 4));
  EXPECT_EQ(pixels[3], 0);
}

/**
 * Covers the image's device ownership: after the caller drops its own reference, resolving the
 * same native texture must still return the original device because the image holds it.
 */
PAG_TEST(D3D12Test, PAGXImageKeepsDeviceAlive) {
  SecondaryDeviceTexture secondary = {};
  if (!secondary.init(16, 16)) {
    GTEST_SKIP() << "Failed to create a secondary D3D12 device.";
  }
  auto device = Devices::MakeForTexture(secondary.texture);
  ASSERT_TRUE(device != nullptr);
  auto expected = device.get();
  auto pagTexture = ToPAG(secondary.texture);
  auto image = pagx::PAGImage::MakeFromTexture(pagTexture, ImageOrigin::TopLeft);
  ASSERT_TRUE(image != nullptr);
  device.reset();
  auto revived = Devices::MakeForTexture(secondary.texture);
  ASSERT_TRUE(revived != nullptr);
  EXPECT_EQ(revived.get(), expected);
  auto target = pagx::PAGSurface::MakeFrom(pagTexture, ImageOrigin::TopLeft);
  ASSERT_TRUE(target != nullptr);
  EXPECT_TRUE(target->waitUntilCompleted());
}

PAG_TEST(D3D12Test, OffscreenCompletion) {
  auto surface = PAGSurface::MakeOffscreen(16, 16);
  ASSERT_TRUE(surface != nullptr);
  EXPECT_TRUE(surface->waitUntilCompleted());
}

namespace {

struct HostQueue {
  Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue = nullptr;
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator = nullptr;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList = nullptr;
  Microsoft::WRL::ComPtr<ID3D12Fence> fence = nullptr;
  HANDLE event = nullptr;
  UINT64 fenceValue = 0;

  ~HostQueue() {
    if (event != nullptr) {
      CloseHandle(event);
    }
  }

  bool init(ID3D12Device* device) {
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (FAILED(
            device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(queue.ReleaseAndGetAddressOf())))) {
      return false;
    }
    if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                              IID_PPV_ARGS(allocator.ReleaseAndGetAddressOf())))) {
      return false;
    }
    if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(),
                                         nullptr,
                                         IID_PPV_ARGS(commandList.ReleaseAndGetAddressOf())))) {
      return false;
    }
    if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                   IID_PPV_ARGS(fence.ReleaseAndGetAddressOf())))) {
      return false;
    }
    event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    return event != nullptr;
  }

  bool execute() {
    if (FAILED(commandList->Close())) {
      return false;
    }
    ID3D12CommandList* lists[] = {commandList.Get()};
    queue->ExecuteCommandLists(1, lists);
    ++fenceValue;
    if (FAILED(queue->Signal(fence.Get(), fenceValue))) {
      return false;
    }
    if (fence->GetCompletedValue() < fenceValue) {
      if (FAILED(fence->SetEventOnCompletion(fenceValue, event))) {
        return false;
      }
      if (WaitForSingleObject(event, 30000) != WAIT_OBJECT_0) {
        return false;
      }
    }
    if (FAILED(allocator->Reset())) {
      return false;
    }
    return SUCCEEDED(commandList->Reset(allocator.Get(), nullptr));
  }
};

bool DrawBottomRightQuadrant(const std::shared_ptr<PAGSurface>& surface, int size) {
  auto composition = PAGComposition::Make(size, size);
  auto solid = PAGSolidLayer::Make(1000000, size / 2, size / 2, Red);
  solid->setMatrix(
      Matrix::MakeTrans(static_cast<float>(size) / 2.0f, static_cast<float>(size) / 2.0f));
  composition->addLayer(solid);
  auto player = std::make_shared<PAGPlayer>();
  player->setSurface(surface);
  player->setComposition(composition);
  player->setMatrix(Matrix::I());
  return player->flush();
}

Microsoft::WRL::ComPtr<ID3D12Resource> CreateBuffer(ID3D12Device* device, UINT64 size,
                                                    D3D12_HEAP_TYPE heapType,
                                                    D3D12_RESOURCE_STATES initialState) {
  D3D12_HEAP_PROPERTIES heap = {};
  heap.Type = heapType;
  D3D12_RESOURCE_DESC desc = {};
  desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  desc.Width = size;
  desc.Height = 1;
  desc.DepthOrArraySize = 1;
  desc.MipLevels = 1;
  desc.SampleDesc.Count = 1;
  desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  Microsoft::WRL::ComPtr<ID3D12Resource> buffer = nullptr;
  if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, initialState,
                                             nullptr,
                                             IID_PPV_ARGS(buffer.ReleaseAndGetAddressOf())))) {
    return nullptr;
  }
  return buffer;
}

void TransitionResource(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource,
                        D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = resource;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  barrier.Transition.StateBefore = before;
  barrier.Transition.StateAfter = after;
  commandList->ResourceBarrier(1, &barrier);
}

}  // namespace

struct RegistryLookupTask {
  const tgfx::BackendTexture* texture = nullptr;
  std::shared_ptr<tgfx::Device>* result = nullptr;

  void operator()() const {
    *result = Devices::MakeForTexture(*texture);
  }
};

/**
 * Covers concurrent first-time resolution of an unregistered native device: the registry must
 * publish exactly one tgfx device even when every worker misses the cache.
 */
PAG_TEST(D3D12Test, DeviceRegistryIsThreadSafe) {
  SecondaryDeviceTexture secondary = {};
  if (!secondary.init(8, 8)) {
    GTEST_SKIP() << "Failed to create a secondary D3D12 device.";
  }
  constexpr int ThreadCount = 8;
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
  EXPECT_NE(canonical.get(), Devices::MakeDefault().get());
}

struct AsyncRenderTask {
  std::shared_ptr<PAGSurface> surface = nullptr;
  std::shared_ptr<bool> flushed = std::make_shared<bool>(false);
  int size = 0;

  void operator()() {
    auto composition = PAGComposition::Make(size, size);
    auto solid = PAGSolidLayer::Make(1000000, size, size, Red);
    composition->addLayer(solid);
    auto player = std::make_shared<PAGPlayer>();
    player->setSurface(surface);
    player->setComposition(composition);
    player->setMatrix(Matrix::I());
    *flushed = player->flush();
  }
};

/**
 * Covers the forAsyncThread path on a non-default native device: D3D12 reuses the texture's
 * registry device (there is no separate context), and the worker's frame is readable afterwards.
 */
PAG_TEST(D3D12Test, AsyncSurfaceRendersOnWorkerThread) {
  const int size = 32;
  SecondaryDeviceTexture secondary = {};
  if (!secondary.init(size, size)) {
    GTEST_SKIP() << "Failed to create a secondary D3D12 device.";
  }
  auto expected = Devices::MakeForTexture(secondary.texture);
  ASSERT_TRUE(expected != nullptr);
  auto surface = PAGSurface::MakeFrom(ToPAG(secondary.texture), ImageOrigin::TopLeft, true);
  ASSERT_TRUE(surface != nullptr);
  EXPECT_EQ(Devices::MakeForTexture(secondary.texture).get(), expected.get());
  EXPECT_NE(expected.get(), Devices::MakeDefault().get());

  AsyncRenderTask task = {};
  task.surface = surface;
  task.size = size;
  std::thread worker(task);
  worker.join();
  ASSERT_TRUE(*task.flushed);
  ASSERT_TRUE(surface->waitUntilCompleted());

  std::vector<uint8_t> pixels(size * size * 4);
  ASSERT_TRUE(
      surface->readPixels(ColorType::RGBA_8888, AlphaType::Premultiplied, pixels.data(), size * 4));
  EXPECT_EQ(pixels[0], 255);
  EXPECT_EQ(pixels[1], 0);
  EXPECT_EQ(pixels[2], 0);
  EXPECT_EQ(pixels[3], 255);
}

/**
 * Covers external render targets on both origins. readPixels() normalizes BottomLeft surfaces, so
 * the same composition must produce identical pixel rows for both origins.
 */
PAG_TEST(D3D12Test, ExternalRenderTargetOriginReadback) {
  const int size = 32;
  auto topLeftTexture = MakeD3D12RenderableTexture(size, size);
  ASSERT_TRUE(topLeftTexture != nullptr);
  auto bottomLeftTexture = MakeD3D12RenderableTexture(size, size);
  ASSERT_TRUE(bottomLeftTexture != nullptr);

  auto topLeftSurface =
      PAGSurface::MakeFrom(MakeD3D12RenderTarget(topLeftTexture, size, size), ImageOrigin::TopLeft);
  ASSERT_TRUE(topLeftSurface != nullptr);
  auto bottomLeftSurface = PAGSurface::MakeFrom(
      MakeD3D12RenderTarget(bottomLeftTexture, size, size), ImageOrigin::BottomLeft);
  ASSERT_TRUE(bottomLeftSurface != nullptr);

  ASSERT_TRUE(DrawBottomRightQuadrant(topLeftSurface, size));
  ASSERT_TRUE(DrawBottomRightQuadrant(bottomLeftSurface, size));
  ASSERT_TRUE(topLeftSurface->waitUntilCompleted());
  ASSERT_TRUE(bottomLeftSurface->waitUntilCompleted());

  std::vector<uint8_t> topLeftPixels(size * size * 4);
  std::vector<uint8_t> bottomLeftPixels(size * size * 4);
  ASSERT_TRUE(topLeftSurface->readPixels(ColorType::RGBA_8888, AlphaType::Premultiplied,
                                         topLeftPixels.data(), size * 4));
  ASSERT_TRUE(bottomLeftSurface->readPixels(ColorType::RGBA_8888, AlphaType::Premultiplied,
                                            bottomLeftPixels.data(), size * 4));
  EXPECT_EQ(topLeftPixels, bottomLeftPixels);

  EXPECT_EQ(topLeftPixels[0], 0);
  EXPECT_EQ(topLeftPixels[1], 0);
  EXPECT_EQ(topLeftPixels[2], 0);
  EXPECT_EQ(topLeftPixels[3], 0);
  auto lastPixel = (size * size - 1) * 4;
  EXPECT_EQ(topLeftPixels[lastPixel], 255);
  EXPECT_EQ(topLeftPixels[lastPixel + 1], 0);
  EXPECT_EQ(topLeftPixels[lastPixel + 2], 0);
  EXPECT_EQ(topLeftPixels[lastPixel + 3], 255);
}

/**
 * Covers the COMMON handoff contract with an independent host queue: the host writes the texture,
 * PAG renders it after an explicit completion wait, and the host reads it back on its own queue.
 */
PAG_TEST(D3D12Test, IndependentQueueCompletion) {
  const int size = 32;
  const UINT rowPitch = 256;
  auto texture = MakeD3D12RenderableTexture(size, size);
  ASSERT_TRUE(texture != nullptr);
  tgfx::D3D12TextureInfo textureInfo = {};
  ASSERT_TRUE(texture->getBackendTexture().getD3D12TextureInfo(&textureInfo));
  auto resource = static_cast<ID3D12Resource*>(const_cast<void*>(textureInfo.resource));
  ASSERT_TRUE(resource != nullptr);
  Microsoft::WRL::ComPtr<ID3D12Device> nativeDevice = nullptr;
  auto nativeResult = resource->GetDevice(IID_PPV_ARGS(nativeDevice.ReleaseAndGetAddressOf()));
  ASSERT_TRUE(SUCCEEDED(nativeResult)) << "GetDevice HRESULT=" << static_cast<long>(nativeResult);
  auto format = static_cast<DXGI_FORMAT>(textureInfo.format);

  HostQueue host = {};
  ASSERT_TRUE(host.init(nativeDevice.Get()));
  auto upload = CreateBuffer(nativeDevice.Get(), static_cast<UINT64>(rowPitch) * size,
                             D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
  ASSERT_TRUE(upload.Get() != nullptr);
  auto readback = CreateBuffer(nativeDevice.Get(), static_cast<UINT64>(rowPitch) * size,
                               D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
  ASSERT_TRUE(readback.Get() != nullptr);

  uint8_t* uploadPixels = nullptr;
  auto uploadResult = upload->Map(0, nullptr, reinterpret_cast<void**>(&uploadPixels));
  ASSERT_TRUE(SUCCEEDED(uploadResult)) << "Map HRESULT=" << static_cast<long>(uploadResult);
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      auto pixel = uploadPixels + y * rowPitch + x * 4;
      pixel[0] = 0;
      pixel[1] = 255;
      pixel[2] = 0;
      pixel[3] = 255;
    }
  }
  upload->Unmap(0, nullptr);

  D3D12_TEXTURE_COPY_LOCATION textureLocation = {};
  textureLocation.pResource = resource;
  textureLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  D3D12_TEXTURE_COPY_LOCATION uploadLocation = {};
  uploadLocation.pResource = upload.Get();
  uploadLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  uploadLocation.PlacedFootprint.Footprint.Format = format;
  uploadLocation.PlacedFootprint.Footprint.Width = size;
  uploadLocation.PlacedFootprint.Footprint.Height = size;
  uploadLocation.PlacedFootprint.Footprint.Depth = 1;
  uploadLocation.PlacedFootprint.Footprint.RowPitch = rowPitch;
  D3D12_TEXTURE_COPY_LOCATION readbackLocation = uploadLocation;
  readbackLocation.pResource = readback.Get();

  TransitionResource(host.commandList.Get(), resource, D3D12_RESOURCE_STATE_COMMON,
                     D3D12_RESOURCE_STATE_COPY_DEST);
  host.commandList->CopyTextureRegion(&textureLocation, 0, 0, 0, &uploadLocation, nullptr);
  TransitionResource(host.commandList.Get(), resource, D3D12_RESOURCE_STATE_COPY_DEST,
                     D3D12_RESOURCE_STATE_COMMON);
  ASSERT_TRUE(host.execute());

  TransitionResource(host.commandList.Get(), resource, D3D12_RESOURCE_STATE_COMMON,
                     D3D12_RESOURCE_STATE_COPY_SOURCE);
  host.commandList->CopyTextureRegion(&readbackLocation, 0, 0, 0, &textureLocation, nullptr);
  TransitionResource(host.commandList.Get(), resource, D3D12_RESOURCE_STATE_COPY_SOURCE,
                     D3D12_RESOURCE_STATE_COMMON);
  ASSERT_TRUE(host.execute());

  D3D12_RANGE readRange = {0, static_cast<SIZE_T>(rowPitch) * size};
  uint8_t* readbackPixels = nullptr;
  auto readbackResult = readback->Map(0, &readRange, reinterpret_cast<void**>(&readbackPixels));
  ASSERT_TRUE(SUCCEEDED(readbackResult)) << "Map HRESULT=" << static_cast<long>(readbackResult);
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      auto pixel = readbackPixels + y * rowPitch + x * 4;
      EXPECT_EQ(pixel[1], 255);
      EXPECT_EQ(pixel[3], 255);
    }
  }
  readback->Unmap(0, nullptr);

  auto surface = PAGSurface::MakeFrom(ToPAG(texture->getBackendTexture()), ImageOrigin::TopLeft);
  ASSERT_TRUE(surface != nullptr);
  ASSERT_TRUE(DrawBottomRightQuadrant(surface, size));
  ASSERT_TRUE(surface->waitUntilCompleted());

  TransitionResource(host.commandList.Get(), resource, D3D12_RESOURCE_STATE_COMMON,
                     D3D12_RESOURCE_STATE_COPY_SOURCE);
  host.commandList->CopyTextureRegion(&readbackLocation, 0, 0, 0, &textureLocation, nullptr);
  TransitionResource(host.commandList.Get(), resource, D3D12_RESOURCE_STATE_COPY_SOURCE,
                     D3D12_RESOURCE_STATE_COMMON);
  ASSERT_TRUE(host.execute());

  auto secondReadbackResult = readback->Map(0, &readRange, reinterpret_cast<void**>(&readbackPixels));
  ASSERT_TRUE(SUCCEEDED(secondReadbackResult)) << "Map HRESULT=" << static_cast<long>(readbackResult);
  auto corner = readbackPixels;
  EXPECT_EQ(corner[0], 0);
  EXPECT_EQ(corner[3], 0);
  auto last = readbackPixels + (size - 1) * rowPitch + (size - 1) * 4;
  EXPECT_EQ(last[0], 255);
  EXPECT_EQ(last[1], 0);
  EXPECT_EQ(last[2], 0);
  EXPECT_EQ(last[3], 255);
  readback->Unmap(0, nullptr);
}

}  // namespace pag

#endif  // TGFX_USE_D3D12
