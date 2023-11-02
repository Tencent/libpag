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

#include "opengl/GLCaps.h"
#include "opengl/GLUtil.h"
#include "rendering/drawables/TextureDrawable.h"
#include "tgfx/opengl/GLDevice.h"
#include "utils/TestUtils.h"

namespace pag {
using namespace tgfx;

/**
 * 用例描述: 测试 PAGSurface 数据同步
 */
PAG_TEST(PAGSurfaceTest, FromTexture) {
  auto pagFile = LoadPAGFile("resources/apitest/test.pag");
  int width = pagFile->width();
  int height = pagFile->height();
  auto device = DevicePool::Make();
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);

  auto glVersion = GLCaps::Get(context)->version;
  tgfx::GLTextureInfo textureInfo;
  CreateGLTexture(context, width, height, &textureInfo);
  auto backendTexture = ToBackendTexture(textureInfo, width, height);
  auto pagSurface = PAGSurface::MakeFrom(backendTexture, ImageOrigin::TopLeft);
  auto nativeHandle = GLDevice::CurrentNativeHandle();
  device->unlock();
  auto glDevice = std::static_pointer_cast<GLDevice>(pagSurface->drawable->onCreateDevice());
  EXPECT_TRUE(glDevice->sharableWith(nativeHandle));

  auto drawable =
      std::make_shared<TextureDrawable>(device, ToTGFX(backendTexture), tgfx::ImageOrigin::TopLeft);
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
  auto pagFile = LoadPAGFile("assets/test2.pag");
  auto width = pagFile->width();
  auto height = pagFile->height();
  auto device = DevicePool::Make();
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  tgfx::GLTextureInfo textureInfo;
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
  auto pagFile = LoadPAGFile("assets/test.pag");
  auto width = pagFile->width();
  auto height = pagFile->height() * 2;
  auto device = DevicePool::Make();
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  tgfx::GLTextureInfo textureInfo;
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

PAG_TEST(PAGSurfaceTest, ImageSnapshot) {
  auto device = DevicePool::Make();
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  tgfx::GLTextureInfo textureInfo;
  auto width = 200;
  auto height = 200;
  CreateGLTexture(context, width, height, &textureInfo);
  tgfx::BackendTexture backendTexture = {textureInfo, width, height};
  auto surface = Surface::MakeFrom(context, backendTexture, tgfx::ImageOrigin::BottomLeft);
  ASSERT_TRUE(surface != nullptr);
  auto image = MakeImage("resources/apitest/imageReplacement.png");
  ASSERT_TRUE(image != nullptr);
  auto canvas = surface->getCanvas();
  canvas->clear();
  canvas->drawImage(image);
  auto snapshotImage = surface->makeImageSnapshot();
  auto snapshotImage2 = surface->makeImageSnapshot();
  EXPECT_TRUE(snapshotImage == snapshotImage2);
  auto compareSurface = Surface::Make(context, width, height);
  auto compareCanvas = compareSurface->getCanvas();
  compareCanvas->drawImage(snapshotImage);
  EXPECT_TRUE(Baseline::Compare(compareSurface, "PAGSurfaceTest/ImageSnapshot1"));
  canvas->drawImage(image, 100, 100);
  EXPECT_TRUE(Baseline::Compare(surface, "PAGSurfaceTest/ImageSnapshot_Surface1"));
  compareCanvas->clear();
  compareCanvas->drawImage(snapshotImage);
  EXPECT_TRUE(Baseline::Compare(compareSurface, "PAGSurfaceTest/ImageSnapshot1"));

  surface = Surface::Make(context, width, height);
  canvas = surface->getCanvas();
  snapshotImage = surface->makeImageSnapshot();
  auto texture = surface->texture;
  snapshotImage = nullptr;
  canvas->drawImage(image);
  canvas->flush();
  EXPECT_TRUE(texture == surface->texture);
  snapshotImage = surface->makeImageSnapshot();
  snapshotImage2 = surface->makeImageSnapshot();
  EXPECT_TRUE(snapshotImage == snapshotImage2);
  compareCanvas->clear();
  compareCanvas->drawImage(snapshotImage);
  EXPECT_TRUE(Baseline::Compare(compareSurface, "PAGSurfaceTest/ImageSnapshot2"));
  canvas->drawImage(image, 100, 100);
  EXPECT_TRUE(Baseline::Compare(surface, "PAGSurfaceTest/ImageSnapshot_Surface2"));
  compareCanvas->clear();
  compareCanvas->drawImage(snapshotImage);
  EXPECT_TRUE(Baseline::Compare(compareSurface, "PAGSurfaceTest/ImageSnapshot2"));

  auto gl = GLFunctions::Get(context);
  gl->deleteTextures(1, &textureInfo.id);
  device->unlock();
}
}  // namespace pag
