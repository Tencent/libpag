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

}  // namespace pag
