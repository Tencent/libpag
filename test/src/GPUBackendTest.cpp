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
//  license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <cstring>
#include <type_traits>
#include "base/PAGTest.h"
#include "base/utils/TGFXCast.h"
#include "base/utils/cast/MetalTexture.h"
#include "pag/pag.h"
#include "tgfx/gpu/Context.h"

namespace pag {

/**
 * Verifies that a scope with no device remains empty and does not acquire a lock.
 */
PAG_TEST(DeviceLockScopeTest, NullDeviceIsEmpty) {
  DeviceLockScope scope(nullptr);
  EXPECT_FALSE(static_cast<bool>(scope));
  EXPECT_EQ(scope.context(), nullptr);
}

/**
 * Verifies that sequential scopes lock the requested device and release it on destruction.
 */
PAG_TEST(DeviceLockScopeTest, SequentialLocksReleaseDevice) {
  auto device = Devices::MakeDefault();
  ASSERT_TRUE(device != nullptr);
  for (int index = 0; index < 3; index++) {
    DeviceLockScope scope(device);
    ASSERT_TRUE(static_cast<bool>(scope));
    EXPECT_EQ(scope.context()->device(), device.get());
  }
}

/**
 * Verifies that a scope retains its device until the lock is released, even if the caller drops
 * the original device reference.
 */
PAG_TEST(DeviceLockScopeTest, RetainsLockedDevice) {
  auto device = Devices::MakeDefault();
  ASSERT_TRUE(device != nullptr);
  std::weak_ptr<tgfx::Device> weakDevice = device;
  std::shared_ptr<tgfx::Device> retainedDevice;
  {
    DeviceLockScope scope(std::move(device));
    ASSERT_TRUE(static_cast<bool>(scope));
    retainedDevice = weakDevice.lock();
    ASSERT_TRUE(retainedDevice != nullptr);
    EXPECT_EQ(scope.context()->device(), retainedDevice.get());
  }
  DeviceLockScope scope(retainedDevice);
  ASSERT_TRUE(static_cast<bool>(scope));
  EXPECT_EQ(scope.context()->device(), retainedDevice.get());
}

/**
 * Verifies GL semaphore initialization, null-handle behavior, conversion, and explicit reset.
 */
PAG_TEST(BackendSemaphoreTest, GLInitializationAndReset) {
  BackendSemaphore semaphore = {};
  semaphore.initGL(nullptr);
  EXPECT_FALSE(semaphore.isInitialized());
  EXPECT_EQ(semaphore.backend(), Backend::MOCK);
  EXPECT_EQ(semaphore.glSync(), nullptr);
  EXPECT_FALSE(ToTGFX(semaphore).isInitialized());

  auto sync = reinterpret_cast<void*>(1);
  semaphore.initGL(sync);
  EXPECT_TRUE(semaphore.isInitialized());
  EXPECT_EQ(semaphore.backend(), Backend::OPENGL);
  EXPECT_EQ(semaphore.glSync(), sync);
  tgfx::GLSyncInfo syncInfo = {};
  ASSERT_TRUE(ToTGFX(semaphore).getGLSync(&syncInfo));
  EXPECT_EQ(syncInfo.sync, sync);

  semaphore.initGL(nullptr);
  EXPECT_TRUE(semaphore.isInitialized());
  EXPECT_EQ(semaphore.glSync(), sync);

  semaphore = {};
  EXPECT_FALSE(semaphore.isInitialized());
  EXPECT_EQ(semaphore.backend(), Backend::MOCK);
  EXPECT_EQ(semaphore.glSync(), nullptr);
  EXPECT_FALSE(ToTGFX(semaphore).isInitialized());
}

/**
 * Verifies that legacy headers can still read the backend, GL handle, and initialized flag.
 */
PAG_TEST(BackendSemaphoreTest, LegacyLayoutCompatibility) {
  struct LegacyLayout {
    Backend backend;
    union {
      void* glSync;
    };
    bool initialized;
  };
  static_assert(std::is_standard_layout<BackendSemaphore>::value,
                "Semaphore must be standard layout");
  static_assert(std::is_trivially_copyable<BackendSemaphore>::value, "Semaphore must be copyable");
  static_assert(sizeof(BackendSemaphore) == sizeof(LegacyLayout), "Semaphore size must not change");
  static_assert(alignof(BackendSemaphore) == alignof(LegacyLayout),
                "Semaphore alignment must not change");

  BackendSemaphore semaphore = {};
  auto checkLayout = [&semaphore]() {
    LegacyLayout legacy = {};
    std::memcpy(&legacy, &semaphore, sizeof(legacy));
    EXPECT_EQ(legacy.backend, semaphore.backend());
    EXPECT_EQ(legacy.glSync, semaphore.glSync());
    EXPECT_EQ(legacy.initialized, semaphore.isInitialized());
  };
  checkLayout();
  semaphore.initGL(reinterpret_cast<void*>(1));
  checkLayout();
  semaphore = {};
  checkLayout();
}

/**
 * 用例描述: ToTGFX 对未支持后端返回空对象（纯逻辑，无需 GPU 设备）
 */
PAG_TEST(TGFXCastTest, UnsupportedBackendReturnsEmpty) {
  auto tgfxTexture = ToTGFX(BackendTexture{});
  EXPECT_FALSE(tgfxTexture.isValid());

  VkImageInfo vkInfo = {};
  vkInfo.image = reinterpret_cast<void*>(1);
  BackendTexture vkTexture(vkInfo, 10, 10);
  EXPECT_TRUE(vkTexture.isValid());
  auto emptyTexture = ToTGFX(vkTexture);
  EXPECT_FALSE(emptyTexture.isValid());

  auto emptyRenderTarget = ToTGFX(BackendRenderTarget{});
  EXPECT_FALSE(emptyRenderTarget.isValid());
}

/**
 * Verifies that every backend's pixel-format query accepts a null Metal texture.
 */
PAG_TEST(TGFXCastTest, NullMetalTexturePixelFormat) {
  EXPECT_EQ(GetMetalTexturePixelFormat(nullptr), 0u);
}

/**
 * Verifies that non-Metal conversions preserve resource tags without accessing native textures.
 */
PAG_TEST(TGFXCastTest, ForeignMetalTexturePreservesBackendTag) {
  if (Devices::SupportsBackend(tgfx::Backend::Metal)) {
    GTEST_SKIP() << "Metal builds require a live native texture.";
  }
  MtlTextureInfo info = {};
  info.texture = reinterpret_cast<void*>(1);
  EXPECT_EQ(GetMetalTexturePixelFormat(info.texture), 0u);

  auto texture = ToTGFX(BackendTexture(info, 10, 10));
  EXPECT_TRUE(texture.isValid());
  EXPECT_EQ(texture.backend(), tgfx::Backend::Metal);
  tgfx::MetalTextureInfo textureInfo = {};
  ASSERT_TRUE(texture.getMetalTextureInfo(&textureInfo));
  EXPECT_EQ(textureInfo.texture, info.texture);
  EXPECT_EQ(textureInfo.format, 0u);

  auto renderTarget = ToTGFX(BackendRenderTarget(info, 10, 10));
  EXPECT_TRUE(renderTarget.isValid());
  EXPECT_EQ(renderTarget.backend(), tgfx::Backend::Metal);
  tgfx::MetalTextureInfo targetInfo = {};
  ASSERT_TRUE(renderTarget.getMetalTextureInfo(&targetInfo));
  EXPECT_EQ(targetInfo.texture, info.texture);
  EXPECT_EQ(targetInfo.format, 0u);
  EXPECT_FALSE(Devices::SupportsBackend(texture.backend()));
  EXPECT_FALSE(Devices::SupportsBackend(renderTarget.backend()));
}

/**
 * 用例描述: WebGPU 后端纹理在 pag 与 tgfx 之间的往返转换（纯逻辑，无需 GPU 设备）
 */
PAG_TEST(TGFXCastTest, WebGPUTextureRoundTrip) {
  WebGPUTextureInfo webGPUInfo = {};
  webGPUInfo.texture = reinterpret_cast<void*>(3);
  webGPUInfo.format = 5;
  BackendTexture texture(webGPUInfo, 640, 360);
  EXPECT_TRUE(texture.isValid());
  EXPECT_EQ(texture.backend(), Backend::WEBGPU);

  // Reading a GL accessor on a WebGPU texture must not leak the union.
  GLTextureInfo glInfo = {};
  EXPECT_FALSE(texture.getGLTextureInfo(&glInfo));

  // Copy semantics preserve the WebGPU variant.
  BackendTexture copy = texture;
  WebGPUTextureInfo readBack = {};
  ASSERT_TRUE(copy.getWebGPUTextureInfo(&readBack));
  EXPECT_EQ(readBack.texture, webGPUInfo.texture);
  EXPECT_EQ(readBack.format, 5u);

  // ToTGFX carries the WebGPU variant through, including the explicit format.
  auto tgfxTexture = ToTGFX(texture);
  ASSERT_TRUE(tgfxTexture.isValid());
  EXPECT_EQ(tgfxTexture.backend(), tgfx::Backend::WebGPU);
  tgfx::WebGPUTextureInfo tgfxInfo = {};
  ASSERT_TRUE(tgfxTexture.getWebGPUTextureInfo(&tgfxInfo));
  EXPECT_EQ(tgfxInfo.texture, webGPUInfo.texture);
  EXPECT_EQ(tgfxInfo.format, 5u);

  // A zero format means unspecified WGPUTextureFormat and must be rejected.
  WebGPUTextureInfo noFormat = webGPUInfo;
  noFormat.format = 0;
  EXPECT_FALSE(ToTGFX(BackendTexture(noFormat, 640, 360)).isValid());
}

}  // namespace pag
