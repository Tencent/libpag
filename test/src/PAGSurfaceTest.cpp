/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include <chrono>
#include <thread>
#include "rendering/drawables/TextureDrawable.h"
#include "utils/TestUtils.h"

// The GL section exercises GL-specific behavior (external GLTextureInfo binding, share-context
// verification via GLDevice::sharableWith, and the GLRestorer external-state guard). None of
// these have exact Metal / Vulkan / D3D12 / WebGPU equivalents, so it is gated on
// TGFX_USE_OPENGL. Metal equivalents (mirroring the ImageOrigin-related tests via MTLTexture)
// are tracked for a follow-up PR together with their baseline keys (main has no Metal code yet,
// so the baseline script cannot generate them here).
#ifdef TGFX_USE_OPENGL

#include "tgfx/gpu/opengl/GLDevice.h"

#ifdef PAG_USE_SWIFTSHADER
#include <GLES3/gl3.h>
#else
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#endif
#include <OpenGL/gl3.h>
#endif

namespace pag {
using namespace tgfx;

#define GL_VER(major, minor) ((static_cast<uint32_t>(major) << 16) | static_cast<uint32_t>(minor))

uint32_t GetGLVersion(const char* versionString) {
  if (versionString == nullptr) {
    return GL_VER(-1, -1);
  }
  int major, minor;
  int mesaMajor, mesaMinor;
  int n = sscanf(versionString, "%d.%d Mesa %d.%d", &major, &minor, &mesaMajor, &mesaMinor);
  if (4 == n) {
    return GL_VER(major, minor);
  }
  n = sscanf(versionString, "%d.%d", &major, &minor);
  if (2 == n) {
    return GL_VER(major, minor);
  }
  int esMajor, esMinor;
  n = sscanf(versionString, "OpenGL ES %d.%d (WebGL %d.%d", &esMajor, &esMinor, &major, &minor);
  if (4 == n) {
    return GL_VER(major, minor);
  }
  n = sscanf(versionString, "OpenGL ES %d.%d", &major, &minor);
  if (2 == n) {
    return GL_VER(major, minor);
  }
  return GL_VER(-1, -1);
}

/**
 * 用例描述: 测试 PAGSurface 数据同步
 */
PAG_TEST(PAGSurfaceTest, FromTexture) {
  auto pagFile = LoadPAGFile("resources/apitest/test.pag");
  int width = pagFile->width();
  int height = pagFile->height();
  auto device = DevicePool::Make();
  ASSERT_TRUE(device != nullptr);
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);

  auto versionString = (const char*)glGetString(GL_VERSION);
  auto glVersion = GetGLVersion(versionString);

  tgfx::GLTextureInfo textureInfo;
  CreateGLTexture(context, width, height, &textureInfo);
  auto backendTexture = ToBackendTexture(textureInfo, width, height);
  auto pagSurface = PAGSurface::MakeFrom(backendTexture, ImageOrigin::TopLeft);
  auto nativeHandle = GLDevice::CurrentNativeHandle();
  device->unlock();
  auto glDevice = std::static_pointer_cast<GLDevice>(pagSurface->drawable->getDevice());
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
  ASSERT_TRUE(device != nullptr);
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
  glDeleteTextures(1, &textureInfo.id);
  device->unlock();
}

/**
 * 用例描述: 测试 GLRestorer 在共享纹理场景下正确保存和恢复 OpenGL 状态
 */
PAG_TEST(PAGSurfaceTest, GLRestorer) {
  auto pagFile = LoadPAGFile("resources/apitest/test.pag");
  int width = pagFile->width();
  int height = pagFile->height();
  auto device = DevicePool::Make();
  ASSERT_TRUE(device != nullptr);
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);

  // Create shared texture as render target
  tgfx::GLTextureInfo textureInfo;
  CreateGLTexture(context, width, height, &textureInfo);
  auto backendTexture = ToBackendTexture(textureInfo, width, height);
  auto pagSurface = PAGSurface::MakeFrom(backendTexture, ImageOrigin::TopLeft);

  // Create FBO for external OpenGL rendering
  GLuint fbo = 0;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureInfo.id, 0);
  ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE));

  // Step 1: Set external GL state before PAG rendering
  int expectedViewport[4] = {10, 20, 200, 150};
  glViewport(expectedViewport[0], expectedViewport[1], expectedViewport[2], expectedViewport[3]);
  glEnable(GL_SCISSOR_TEST);
  int expectedScissorBox[4] = {5, 10, 100, 80};
  glScissor(expectedScissorBox[0], expectedScissorBox[1], expectedScissorBox[2],
            expectedScissorBox[3]);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  int expectedFbo = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &expectedFbo);
  device->unlock();

  // Step 2: PAG rendering (GLRestorer should save state on lockContext, restore on unlockContext)
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setProgress(0);
  pagPlayer->flush();

  // Step 3: Verify GL state is restored after PAG rendering
  // Note: We need to check state without calling lockContext again,
  // because the state should be restored when PAG's unlockContext was called.
  // But since we need context to be current to query GL state, we use the same device.
  context = device->lockContext();
  ASSERT_TRUE(context != nullptr);

  // These checks verify that GLRestorer restored the state when PAG finished rendering
  int actualViewport[4] = {};
  glGetIntegerv(GL_VIEWPORT, actualViewport);
  EXPECT_EQ(actualViewport[0], expectedViewport[0]);
  EXPECT_EQ(actualViewport[1], expectedViewport[1]);
  EXPECT_EQ(actualViewport[2], expectedViewport[2]);
  EXPECT_EQ(actualViewport[3], expectedViewport[3]);

  EXPECT_TRUE(glIsEnabled(GL_SCISSOR_TEST));
  int actualScissorBox[4] = {};
  glGetIntegerv(GL_SCISSOR_BOX, actualScissorBox);
  EXPECT_EQ(actualScissorBox[0], expectedScissorBox[0]);
  EXPECT_EQ(actualScissorBox[1], expectedScissorBox[1]);
  EXPECT_EQ(actualScissorBox[2], expectedScissorBox[2]);
  EXPECT_EQ(actualScissorBox[3], expectedScissorBox[3]);

  EXPECT_TRUE(glIsEnabled(GL_BLEND));

  int actualFbo = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &actualFbo);
  EXPECT_EQ(actualFbo, expectedFbo);

  glDeleteFramebuffers(1, &fbo);
  glDeleteTextures(1, &textureInfo.id);
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
  glDeleteTextures(1, &textureInfo.id);
  device->unlock();
}
}  // namespace pag

#endif  // TGFX_USE_OPENGL

namespace pag {

// GL readback readiness is fence-based: it only flips after the GPU drains previously queued
// work, so tests must poll instead of assuming immediate readiness.
static bool WaitReadbackReady(const std::shared_ptr<PAGSurfaceReadback>& readback) {
  for (int i = 0; i < 5000; i++) {
    if (readback->isReady()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return false;
}

/**
 * 用例描述: 测试 PAGSurface 异步回读会话，公共 API 覆盖全部后端。GL 回读基于 fence，
 * 就绪要等 GPU 排空之前堆积的任务，不能假定创建即就绪，必须与同步 readPixels 的结果
 * 逐字节比对
 */
PAG_TEST(PAGSurfaceTest, AsyncReadPixels) {
  auto pagFile = LoadPAGFile("resources/apitest/test.pag");
  ASSERT_TRUE(pagFile != nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
  ASSERT_TRUE(pagSurface != nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setComposition(pagFile);
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setProgress(0.5);
  ASSERT_TRUE(pagPlayer->flush());

  auto readback = pagSurface->asyncReadPixels();
  ASSERT_TRUE(readback != nullptr);
  EXPECT_EQ(readback->width(), pagFile->width());
  EXPECT_EQ(readback->height(), pagFile->height());
  ASSERT_TRUE(WaitReadbackReady(readback));

  const auto width = readback->width();
  const auto height = readback->height();
  std::vector<uint8_t> asyncPixels(static_cast<size_t>(width * height * 4));
  ASSERT_TRUE(readback->readPixels(ColorType::RGBA_8888, AlphaType::Premultiplied,
                                   asyncPixels.data(), static_cast<size_t>(width * 4)));

  std::vector<uint8_t> syncPixels(static_cast<size_t>(width * height * 4));
  ASSERT_TRUE(pagSurface->readPixels(ColorType::RGBA_8888, AlphaType::Premultiplied,
                                     syncPixels.data(), static_cast<size_t>(width * 4)));
  EXPECT_EQ(asyncPixels, syncPixels);
}

/**
 * 用例描述: 异步回读的非法参数防御与 ALPHA_8 格式转换路径（回读数据为表面原生 RGBA
 * 格式，经 Pixmap 转换到目标格式，结果须与同步 readPixels 一致）
 */
PAG_TEST(PAGSurfaceTest, AsyncReadPixelsInvalidArgsAndFormat) {
  auto pagFile = LoadPAGFile("resources/apitest/test.pag");
  ASSERT_TRUE(pagFile != nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
  ASSERT_TRUE(pagSurface != nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setComposition(pagFile);
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setProgress(0.5);
  ASSERT_TRUE(pagPlayer->flush());

  auto readback = pagSurface->asyncReadPixels();
  ASSERT_TRUE(readback != nullptr);
  const auto width = readback->width();
  const auto height = readback->height();

  // 「未就绪时 readPixels 返回 false」无法在 GL 上确定性构造：isReady() 的探测与
  // readPixels 内部的就绪重查之间存在 fence 置位竞态，GPU 排空进度不受 CPU 控制。
  // 该路径由 §9 浏览器车道覆盖（WebGPU 的 mapAsync 天然有未就绪窗口）。
  ASSERT_TRUE(WaitReadbackReady(readback));

  std::vector<uint8_t> pixels(static_cast<size_t>(width * height * 4));
  EXPECT_FALSE(readback->readPixels(ColorType::RGBA_8888, AlphaType::Premultiplied, nullptr,
                                    static_cast<size_t>(width * 4)));
  EXPECT_FALSE(
      readback->readPixels(ColorType::RGBA_8888, AlphaType::Premultiplied, pixels.data(), 0));

  std::vector<uint8_t> asyncAlpha(static_cast<size_t>(width * height));
  ASSERT_TRUE(readback->readPixels(ColorType::ALPHA_8, AlphaType::Premultiplied, asyncAlpha.data(),
                                   static_cast<size_t>(width)));
  std::vector<uint8_t> syncAlpha(static_cast<size_t>(width * height));
  ASSERT_TRUE(pagSurface->readPixels(ColorType::ALPHA_8, AlphaType::Premultiplied, syncAlpha.data(),
                                     static_cast<size_t>(width)));
  EXPECT_EQ(asyncAlpha, syncAlpha);
}

/**
 * 用例描述: 回读会话持有 PAGSurface 的生命周期，外部释放 surface 与 player 后会话
 * 仍能完成就绪等待与取像素
 */
PAG_TEST(PAGSurfaceTest, AsyncReadPixelsOutlivesSurface) {
  auto pagFile = LoadPAGFile("resources/apitest/test.pag");
  ASSERT_TRUE(pagFile != nullptr);
  std::shared_ptr<PAGSurfaceReadback> readback = nullptr;
  {
    auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
    ASSERT_TRUE(pagSurface != nullptr);
    auto pagPlayer = std::make_shared<PAGPlayer>();
    pagPlayer->setComposition(pagFile);
    pagPlayer->setSurface(pagSurface);
    pagPlayer->setProgress(0.5);
    ASSERT_TRUE(pagPlayer->flush());
    readback = pagSurface->asyncReadPixels();
    ASSERT_TRUE(readback != nullptr);
  }

  ASSERT_TRUE(WaitReadbackReady(readback));
  const auto width = readback->width();
  const auto height = readback->height();
  std::vector<uint8_t> pixels(static_cast<size_t>(width * height * 4));
  EXPECT_TRUE(readback->readPixels(ColorType::RGBA_8888, AlphaType::Premultiplied, pixels.data(),
                                   static_cast<size_t>(width * 4)));
}

}  // namespace pag
