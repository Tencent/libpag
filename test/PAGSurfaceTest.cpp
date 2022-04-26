/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "framework/pag_test.h"
#include "framework/utils/PAGTestUtils.h"
#include "gpu/opengl/GLCaps.h"
#include "gpu/opengl/GLUtil.h"
#include "rendering/Drawable.h"
#include "tgfx/gpu/opengl/GLDevice.h"

namespace pag {
using namespace tgfx;

PAG_TEST_SUIT(PAGSurfaceTest)

/**
 * 用例描述: 测试 PAGSurface 数据同步
 */
PAG_TEST(PAGSurfaceTest, FromTexture) {
  auto pagFile = PAGFile::Load("../resources/apitest/test.pag");
  int width = pagFile->width();
  int height = pagFile->height();
  auto device = GLDevice::Make();
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);

  auto glVersion = GLCaps::Get(context)->version;
  tgfx::GLSampler textureInfo;
  CreateGLTexture(context, width, height, &textureInfo);
  auto backendTexture = ToBackendTexture(textureInfo, width, height);
  auto pagSurface = PAGSurface::MakeFrom(backendTexture, ImageOrigin::TopLeft);
  auto nativeHandle = GLDevice::CurrentNativeHandle();
  device->unlock();
  auto glDevice = std::static_pointer_cast<GLDevice>(pagSurface->drawable->getDevice());
  EXPECT_TRUE(glDevice->sharableWith(nativeHandle));

  auto drawable =
      std::make_shared<TextureDrawable>(device, backendTexture, tgfx::ImageOrigin::TopLeft);
  auto pagSurface2 = PAGSurface::MakeFrom(drawable);
  auto pagPlayer2 = std::make_shared<PAGPlayer>();
  pagPlayer2->setSurface(pagSurface2);
  pagPlayer2->setComposition(pagFile);
  pagPlayer2->setProgress(0.1);
  BackendSemaphore semaphore;
  EXPECT_FALSE(pagPlayer2->wait(semaphore));
  semaphore.initGL(nullptr);
  EXPECT_FALSE(pagPlayer2->wait(semaphore));
  semaphore = {};
  pagPlayer2->flushAndSignalSemaphore(&semaphore);
  if (glVersion < GL_VER(3, 0)) {
    EXPECT_FALSE(semaphore.isInitialized());
  } else {
    EXPECT_TRUE(semaphore.isInitialized());
    EXPECT_NE(semaphore.glSync(), nullptr);
    auto pagPlayer = std::make_shared<PAGPlayer>();
    pagPlayer->setComposition(pagFile);
    pagPlayer->setSurface(pagSurface);
    pagPlayer->setProgress(0.3);
    EXPECT_TRUE(pagPlayer->wait(semaphore));
    semaphore = {};
    pagPlayer->flushAndSignalSemaphore(&semaphore);
    EXPECT_TRUE(semaphore.isInitialized());
    EXPECT_NE(semaphore.glSync(), nullptr);
    EXPECT_TRUE(pagPlayer2->wait(semaphore));
    EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGSurfaceTest/FromTexture"));
  }
}

/**
 * 用例描述: 遮罩使用屏幕坐标系时origin不一致的情况
 */
PAG_TEST(PAGSurfaceTest, Mask) {
  auto pagFile = PAGFile::Load("../assets/test2.pag");
  auto width = pagFile->width();
  auto height = pagFile->height();
  auto device = GLDevice::Make();
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  tgfx::GLSampler textureInfo;
  CreateGLTexture(context, width, height, &textureInfo);
  auto backendTexture = ToBackendTexture(textureInfo, width, height);
  auto pagSurface = PAGSurface::MakeFrom(backendTexture, ImageOrigin::BottomLeft);
  device->unlock();

  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setMatrix(Matrix::I());
  pagPlayer->setProgress(0.9);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGSurfaceTest/Mask"));

  context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto gl = GLFunctions::Get(context);
  gl->deleteTextures(1, &textureInfo.id);
  device->unlock();
}

/**
 * 用例描述: PAGSurface 的 origin 是 BottomLeft 时 scissor rect 需要 flip Y。
 */
PAG_TEST(PAGSurfaceTest, BottomLeftScissor) {
  auto pagFile = PAGFile::Load("../assets/test.pag");
  auto width = pagFile->width();
  auto height = pagFile->height() * 2;
  auto device = GLDevice::Make();
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  tgfx::GLSampler textureInfo;
  CreateGLTexture(context, width, height, &textureInfo);
  auto backendTexture = ToBackendTexture(textureInfo, width, height);
  auto pagSurface = PAGSurface::MakeFrom(backendTexture, ImageOrigin::BottomLeft);
  device->unlock();

  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setMatrix(pag::Matrix::MakeTrans(0, static_cast<float>(pagFile->height())));
  pagPlayer->setProgress(0.5);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGSurfaceTest/BottomLeftScissor"));

  context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto gl = GLFunctions::Get(context);
  gl->deleteTextures(1, &textureInfo.id);
  device->unlock();
}
}  // namespace pag
