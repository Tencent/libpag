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
#include "platform/NativeGLDevice.h"
#include "rendering/Drawable.h"

namespace pag {
PAG_TEST_SUIT(PAGSurfaceTest)

/**
 * 用例描述: 测试 PAGSurface 数据同步
 */
PAG_TEST(PAGSurfaceTest, FromTexture_ID82382683) {
    auto pagFile = PAGFile::Load("../resources/apitest/test.pag");
    int width = pagFile->width();
    int height = pagFile->height();
    auto device = NativeGLDevice::Make();
    auto context = device->lockContext();
    ASSERT_TRUE(context != nullptr);
    auto gl = GLContext::Unwrap(context);
    auto glVersion = gl->caps->version;
    GLTextureInfo textureInfo;
    CreateTexture(gl, width, height, &textureInfo);
    auto backendTexture = BackendTexture(textureInfo, width, height);
    auto pagSurface = PAGSurface::MakeFrom(backendTexture, ImageOrigin::TopLeft);
    auto nativeHandle = NativeGLDevice::GetCurrentNativeHandle();
    device->unlock();
    auto glDevice = std::static_pointer_cast<GLDevice>(pagSurface->drawable->getDevice());
    EXPECT_TRUE(glDevice->sharableWith(nativeHandle));

    auto drawable = std::make_shared<TextureDrawable>(device, backendTexture, ImageOrigin::TopLeft);
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
        auto md5 = DumpMD5(pagSurface2);
        PAGTestEnvironment::DumpJson["PAGSurfaceTest"]["FromTexture_ID82382683"] = md5;
#ifdef COMPARE_JSON_PATH
        auto compareMD5 = PAGTestEnvironment::CompareJson["PAGSurfaceTest"]["FromTexture_ID82382683"];
        TraceIf(pagSurface2, "../test/out/FromTexture_ID82382683.png", md5 != compareMD5);
        EXPECT_EQ(compareMD5.get<std::string>(), md5);
#endif
    }
}

/**
 * 用例描述: 遮罩使用屏幕坐标系时origin不一致的情况
 */
PAG_TEST(PAGSurfaceTest, mask_ID85767971) {
    auto pagFile = PAGFile::Load("../assets/test2.pag");
    auto width = pagFile->width();
    auto height = pagFile->height();
    auto device = NativeGLDevice::Make();
    auto context = device->lockContext();
    ASSERT_TRUE(context != nullptr);
    auto gl = GLContext::Unwrap(context);
    GLTextureInfo textureInfo;
    CreateTexture(gl, width, height, &textureInfo);
    auto backendTexture = BackendTexture(textureInfo, width, height);
    auto pagSurface = PAGSurface::MakeFrom(backendTexture, ImageOrigin::BottomLeft);
    device->unlock();

    auto pagPlayer = std::make_shared<PAGPlayer>();
    pagPlayer->setSurface(pagSurface);
    pagPlayer->setComposition(pagFile);
    pagPlayer->setMatrix(Matrix::I());
    pagPlayer->setProgress(0.9);
    pagPlayer->flush();
    auto md5 = DumpMD5(pagSurface);
    PAGTestEnvironment::DumpJson["PAGSurfaceTest"]["mask_ID85767971"] = md5;
#ifdef COMPARE_JSON_PATH
    auto compareMD5 = PAGTestEnvironment::CompareJson["PAGSurfaceTest"]["mask_ID85767971"];
    auto path = "../test/out/mask_ID85767971.png";
    TraceIf(pagSurface, path, compareMD5.get<std::string>() != md5);
    EXPECT_EQ(compareMD5.get<std::string>(), md5);
#endif

    context = device->lockContext();
    ASSERT_TRUE(context != nullptr);
    gl = GLContext::Unwrap(context);
    gl->deleteTextures(1, &textureInfo.id);
    device->unlock();
}
}  // namespace pag
