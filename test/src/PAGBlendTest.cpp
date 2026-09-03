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

#include "utils/TestUtils.h"

namespace pag {
using namespace tgfx;

/**
 * 用例描述: 测试基础混合模式
 * Backend-agnostic: only touches OffscreenSurface + Baseline::Compare, no GL/Metal specifics.
 */
PAG_TEST(PAGBlendTest, Blend) {
  auto files = GetAllPAGFiles("resources/blend");
  auto pagSurface = OffscreenSurface::Make(400, 400);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  for (auto& file : files) {
    auto pagFile = PAGFile::Load(file);
    EXPECT_NE(pagFile, nullptr);
    pagPlayer->setComposition(pagFile);
    pagPlayer->setMatrix(Matrix::I());
    pagPlayer->setProgress(0.5);
    pagPlayer->flush();
    auto found = file.find_last_of("/\\");
    auto suffixIndex = file.rfind('.');
    auto fileName = file.substr(found + 1, suffixIndex - found - 1);
    EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGBlendTest/Blend_" + fileName));
  }
}

}  // namespace pag

// The remaining cases construct external GL textures via tgfx::GLTextureInfo and feed them into
// PAGImage::FromTexture / PAGImage::setMatrix workflows. Metal / Vulkan / D3D12 / WebGPU cannot
// consume GLTextureInfo, so the rest of the GL section is gated on TGFX_USE_OPENGL. Metal
// equivalents live in the TGFX_USE_METAL section at the bottom of this file.
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

tgfx::GLTextureInfo GetBottomLeftImage(std::shared_ptr<Device> device, int width, int height) {
  auto context = device->lockContext();
  tgfx::GLTextureInfo textureInfo;
  CreateGLTexture(context, width, height, &textureInfo);
  auto backendTexture = ToBackendTexture(textureInfo, width, height);
  auto surface = PAGSurface::MakeFrom(backendTexture, ImageOrigin::BottomLeft);
  device->unlock();
  auto composition = PAGComposition::Make(1080, 1920);
  auto imageLayer = PAGImageLayer::Make(1080, 1920, 1000000);
  imageLayer->replaceImage(MakePAGImage("assets/mountain.jpg"));
  composition->addLayer(imageLayer);
  auto player = std::make_shared<PAGPlayer>();
  player->setSurface(surface);
  player->setComposition(composition);
  player->flush();
  return textureInfo;
}

/**
 * 用例描述: 使用blend时，如果当前的frameBuffer是BottomLeft的，复制逻辑验证
 */
PAG_TEST(PAGBlendTest, CopyDstTexture) {
  auto width = 400;
  auto height = 400;
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
  auto pagFile = LoadPAGFile("resources/blend/Multiply.pag");
  pagPlayer->setComposition(pagFile);
  pagPlayer->setMatrix(Matrix::I());
  pagPlayer->setProgress(0.5);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGBlendTest/CopyDstTexture"));

  context = device->lockContext();
  glDeleteTextures(1, &textureInfo.id);
  device->unlock();
}

/**
 * 用例描述: 替换的texture是BottomLeft，renderTarget是TopLeft
 */
PAG_TEST(PAGBlendTest, TextureBottomLeft) {
  auto width = 720;
  auto height = 1280;
  auto device = DevicePool::Make();
  auto replaceTextureInfo = GetBottomLeftImage(device, width, height);
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto backendTexture = ToBackendTexture(replaceTextureInfo, width, height);
  auto replaceImage = PAGImage::FromTexture(backendTexture, ImageOrigin::BottomLeft);
  tgfx::GLTextureInfo textureInfo;
  CreateGLTexture(context, width, height, &textureInfo);
  backendTexture = ToBackendTexture(textureInfo, width, height);
  auto pagSurface = PAGSurface::MakeFrom(backendTexture, ImageOrigin::TopLeft);
  device->unlock();

  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  auto pagFile = LoadPAGFile("resources/apitest/texture_bottom_left.pag");
  pagFile->replaceImage(3, replaceImage);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setMatrix(Matrix::I());
  pagPlayer->setProgress(0.34);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGBlendTest/TextureBottomLeft"));

  context = device->lockContext();
  glDeleteTextures(1, &replaceTextureInfo.id);
  glDeleteTextures(1, &textureInfo.id);
  device->unlock();
}

/**
 * 用例描述: 替换的texture和renderTarget都是BottomLeft
 */
PAG_TEST(PAGBlendTest, BothBottomLeft) {
  auto width = 720;
  auto height = 1280;
  auto device = DevicePool::Make();
  auto replaceTextureInfo = GetBottomLeftImage(device, width, height);
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto backendTexture = ToBackendTexture(replaceTextureInfo, width / 2, height / 2);
  auto replaceImage = PAGImage::FromTexture(backendTexture, ImageOrigin::BottomLeft);
  replaceImage->setMatrix(
      Matrix::MakeTrans(static_cast<float>(width) * 0.1, static_cast<float>(height) * 0.2));
  tgfx::GLTextureInfo textureInfo = {};
  CreateGLTexture(context, width, height, &textureInfo);
  backendTexture = ToBackendTexture(textureInfo, width, height);
  auto pagSurface = PAGSurface::MakeFrom(backendTexture, ImageOrigin::BottomLeft);
  device->unlock();

  auto composition = PAGComposition::Make(width, height);
  auto imageLayer = PAGImageLayer::Make(width, height, 1000000);
  imageLayer->replaceImage(replaceImage);
  composition->addLayer(imageLayer);

  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(composition);
  pagPlayer->setMatrix(Matrix::I());
  pagPlayer->setProgress(0.3);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGBlendTest/BothBottomLeft"));

  context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  glDeleteTextures(1, &replaceTextureInfo.id);
  glDeleteTextures(1, &textureInfo.id);
  device->unlock();
}
}  // namespace pag

#endif  // TGFX_USE_OPENGL

// Metal-backend equivalents of the GL BottomLeft blend cases above. They source their external
// textures from an id<MTLTexture> instead of a GLuint texture id, exercising the Metal path of
// PAGSurface::MakeFrom(BackendTexture) + PAGImage::FromTexture(BackendTexture) with BottomLeft
// origin handling in the blend pipeline.
#ifdef TGFX_USE_METAL

namespace pag {
using namespace tgfx;

// Metal equivalent of GetBottomLeftImage: creates a BottomLeft-oriented MTLTexture, renders
// mountain.jpg onto it, and returns the texture info. The caller owns the +1 retain on the
// MTLTexture handle and must release it via ReleaseMetalTexture().
tgfx::MetalTextureInfo GetBottomLeftMetalImage(std::shared_ptr<Device> device, int width,
                                               int height) {
  auto context = device->lockContext();
  tgfx::MetalTextureInfo textureInfo = {};
  CreateMetalTexture(context, width, height, &textureInfo);
  auto backendTexture = ToBackendTexture(textureInfo, width, height);
  auto surface = PAGSurface::MakeFrom(backendTexture, ImageOrigin::BottomLeft);
  device->unlock();
  auto composition = PAGComposition::Make(1080, 1920);
  auto imageLayer = PAGImageLayer::Make(1080, 1920, 1000000);
  imageLayer->replaceImage(MakePAGImage("assets/mountain.jpg"));
  composition->addLayer(imageLayer);
  auto player = std::make_shared<PAGPlayer>();
  player->setSurface(surface);
  player->setComposition(composition);
  player->flush();
  return textureInfo;
}

/**
 * 用例描述: Metal backend equivalent of PAGBlendTest.CopyDstTexture. Uses a BottomLeft
 * MTLTexture as the render target to exercise the copy-dst-texture path in the Metal blend
 * pipeline.
 */
PAG_TEST(PAGBlendMetalTest, CopyDstTexture) {
  auto width = 400;
  auto height = 400;
  auto device = DevicePool::Make();
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  tgfx::MetalTextureInfo textureInfo = {};
  ASSERT_TRUE(CreateMetalTexture(context, width, height, &textureInfo));
  auto backendTexture = ToBackendTexture(textureInfo, width, height);
  auto pagSurface = PAGSurface::MakeFrom(backendTexture, ImageOrigin::BottomLeft);
  device->unlock();
  ASSERT_TRUE(pagSurface != nullptr);

  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  auto pagFile = LoadPAGFile("resources/blend/Multiply.pag");
  pagPlayer->setComposition(pagFile);
  pagPlayer->setMatrix(Matrix::I());
  pagPlayer->setProgress(0.5);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGBlendMetalTest/CopyDstTexture"));

  ReleaseMetalTexture(&textureInfo);
}

/**
 * 用例描述: Metal backend equivalent of PAGBlendTest.TextureBottomLeft. Replace-image texture
 * is BottomLeft, render target is TopLeft.
 */
PAG_TEST(PAGBlendMetalTest, TextureBottomLeft) {
  auto width = 720;
  auto height = 1280;
  auto device = DevicePool::Make();
  auto replaceTextureInfo = GetBottomLeftMetalImage(device, width, height);
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto replaceBackendTexture = ToBackendTexture(replaceTextureInfo, width, height);
  auto replaceImage = PAGImage::FromTexture(replaceBackendTexture, ImageOrigin::BottomLeft);
  tgfx::MetalTextureInfo textureInfo = {};
  ASSERT_TRUE(CreateMetalTexture(context, width, height, &textureInfo));
  auto backendTexture = ToBackendTexture(textureInfo, width, height);
  auto pagSurface = PAGSurface::MakeFrom(backendTexture, ImageOrigin::TopLeft);
  device->unlock();
  ASSERT_TRUE(pagSurface != nullptr);

  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  auto pagFile = LoadPAGFile("resources/apitest/texture_bottom_left.pag");
  pagFile->replaceImage(3, replaceImage);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setMatrix(Matrix::I());
  pagPlayer->setProgress(0.34);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGBlendMetalTest/TextureBottomLeft"));

  ReleaseMetalTexture(&replaceTextureInfo);
  ReleaseMetalTexture(&textureInfo);
}

/**
 * 用例描述: Metal backend equivalent of PAGBlendTest.BothBottomLeft. Both the replace-image
 * texture and the render target are BottomLeft-oriented MTLTextures.
 */
PAG_TEST(PAGBlendMetalTest, BothBottomLeft) {
  auto width = 720;
  auto height = 1280;
  auto device = DevicePool::Make();
  auto replaceTextureInfo = GetBottomLeftMetalImage(device, width, height);
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);
  auto replaceBackendTexture = ToBackendTexture(replaceTextureInfo, width / 2, height / 2);
  auto replaceImage = PAGImage::FromTexture(replaceBackendTexture, ImageOrigin::BottomLeft);
  replaceImage->setMatrix(
      Matrix::MakeTrans(static_cast<float>(width) * 0.1f, static_cast<float>(height) * 0.2f));
  tgfx::MetalTextureInfo textureInfo = {};
  ASSERT_TRUE(CreateMetalTexture(context, width, height, &textureInfo));
  auto backendTexture = ToBackendTexture(textureInfo, width, height);
  auto pagSurface = PAGSurface::MakeFrom(backendTexture, ImageOrigin::BottomLeft);
  device->unlock();
  ASSERT_TRUE(pagSurface != nullptr);

  auto composition = PAGComposition::Make(width, height);
  auto imageLayer = PAGImageLayer::Make(width, height, 1000000);
  imageLayer->replaceImage(replaceImage);
  composition->addLayer(imageLayer);

  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(composition);
  pagPlayer->setMatrix(Matrix::I());
  pagPlayer->setProgress(0.3);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGBlendMetalTest/BothBottomLeft"));

  ReleaseMetalTexture(&replaceTextureInfo);
  ReleaseMetalTexture(&textureInfo);
}

}  // namespace pag

#endif  // TGFX_USE_METAL
