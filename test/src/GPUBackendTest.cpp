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

#include "base/PAGTest.h"
#include "base/utils/TGFXCast.h"
#include "pag/pag.h"

namespace pag {

/**
 * 用例描述: BackendSemaphore 跨后端 getter 守卫与复位语义（纯逻辑，无需 GPU 设备）
 */
PAG_TEST(BackendSemaphoreTest, CrossBackendGuards) {
  BackendSemaphore semaphore = {};
  EXPECT_FALSE(semaphore.isInitialized());
  EXPECT_EQ(semaphore.backend(), Backend::MOCK);

  semaphore.initGL(reinterpret_cast<void*>(1));
  EXPECT_TRUE(semaphore.isInitialized());
  EXPECT_EQ(semaphore.backend(), Backend::OPENGL);
  EXPECT_EQ(semaphore.glSync(), reinterpret_cast<void*>(1));
  // Reading Metal accessors on a GL semaphore must not leak the union.
  MtlEventInfo crossInfo = {};
  crossInfo.event = reinterpret_cast<void*>(9);
  crossInfo.value = 9;
  EXPECT_FALSE(semaphore.getMtlEventInfo(&crossInfo));
  EXPECT_EQ(crossInfo.event, reinterpret_cast<void*>(9));

  semaphore = {};
  MtlEventInfo info = {};
  info.event = reinterpret_cast<void*>(2);
  info.value = 5;
  semaphore.initMetal(info);
  EXPECT_TRUE(semaphore.isInitialized());
  EXPECT_EQ(semaphore.backend(), Backend::METAL);
  MtlEventInfo readBack = {};
  EXPECT_TRUE(semaphore.getMtlEventInfo(&readBack));
  EXPECT_EQ(readBack.event, reinterpret_cast<void*>(2));
  EXPECT_EQ(readBack.value, 5u);
  // Reading the GL accessor on a Metal semaphore must not leak the union.
  EXPECT_EQ(semaphore.glSync(), nullptr);

  // A null handle leaves the semaphore uninitialized.
  semaphore = {};
  semaphore.initGL(nullptr);
  EXPECT_FALSE(semaphore.isInitialized());
  MtlEventInfo nullInfo = {};
  semaphore.initMetal(nullInfo);
  EXPECT_FALSE(semaphore.isInitialized());

  // initMetal with a null event resets the semaphore instead of keeping the previous GL state.
  semaphore.initGL(reinterpret_cast<void*>(1));
  semaphore.initMetal(nullInfo);
  EXPECT_FALSE(semaphore.isInitialized());
  EXPECT_EQ(semaphore.glSync(), nullptr);
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
 * 用例描述: WebGPU 后端纹理/渲染目标在 pag 与 tgfx 之间的往返转换（纯逻辑，无需 GPU 设备）
 */
PAG_TEST(TGFXCastTest, WebGPUTextureRoundTrip) {
  WgpuTextureInfo wgpuInfo = {};
  wgpuInfo.texture = reinterpret_cast<void*>(3);
  wgpuInfo.textureView = reinterpret_cast<void*>(4);
  wgpuInfo.format = 5;
  BackendTexture texture(wgpuInfo, 640, 360);
  EXPECT_TRUE(texture.isValid());
  EXPECT_EQ(texture.backend(), Backend::WEBGPU);

  // Reading a GL accessor on a WebGPU texture must not leak the union.
  GLTextureInfo glInfo = {};
  EXPECT_FALSE(texture.getGLTextureInfo(&glInfo));

  // Copy semantics preserve the WebGPU variant.
  BackendTexture copy = texture;
  WgpuTextureInfo readBack = {};
  ASSERT_TRUE(copy.getWgpuTextureInfo(&readBack));
  EXPECT_EQ(readBack.texture, wgpuInfo.texture);
  EXPECT_EQ(readBack.textureView, wgpuInfo.textureView);
  EXPECT_EQ(readBack.format, 5u);

  // ToTGFX carries the WebGPU variant through, including the explicit format.
  auto tgfxTexture = ToTGFX(texture);
  ASSERT_TRUE(tgfxTexture.isValid());
  EXPECT_EQ(tgfxTexture.backend(), tgfx::Backend::WebGPU);
  tgfx::WebGPUTextureInfo tgfxInfo = {};
  ASSERT_TRUE(tgfxTexture.getWebGPUTextureInfo(&tgfxInfo));
  EXPECT_EQ(tgfxInfo.texture, wgpuInfo.texture);
  EXPECT_EQ(tgfxInfo.textureView, wgpuInfo.textureView);
  EXPECT_EQ(tgfxInfo.format, 5u);

  // A zero format means unspecified WGPUTextureFormat and must be rejected.
  WgpuTextureInfo noFormat = wgpuInfo;
  noFormat.format = 0;
  EXPECT_FALSE(ToTGFX(BackendTexture(noFormat, 640, 360)).isValid());

  // Render targets share the same conversion path.
  BackendRenderTarget renderTarget(wgpuInfo, 640, 360);
  auto tgfxTarget = ToTGFX(renderTarget);
  ASSERT_TRUE(tgfxTarget.isValid());
  EXPECT_EQ(tgfxTarget.backend(), tgfx::Backend::WebGPU);
}

}  // namespace pag
