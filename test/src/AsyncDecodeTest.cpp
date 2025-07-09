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

#include <unordered_set>
#include "rendering/caches/RenderCache.h"
#include "utils/TestUtils.h"

namespace pag {
/**
 * 用例描述: 异步解码时候同步删除图层
 */
PAG_TEST(AsyncDecode, remove_ID79139135) {
  PAG_SETUP_WITH_PATH(TestPAGSurface, TestPAGPlayer, TestPAGFile,
                      "resources/apitest/AsyncDecodeTest.pag");
  // 触发第一个视频图层初始化
  TestPAGPlayer->flush();
  // 移除所有图层
  TestPAGPlayer->getComposition()->removeAllLayers();
  // 需要再次flush，因为缓存是在flush时候才会删除
  TestPAGPlayer->flush();

  // 图像销毁时候，异步正在初始化的序列帧也应该被销毁
  EXPECT_EQ(static_cast<int>(TestPAGPlayer->renderCache->sequenceCaches.size()), 0);
}

/**
 * 用例描述: 校验异步预测是否能正常命中和回收
 */
PAG_TEST(AsyncDecode, init_ID79139125) {
  PAG_SETUP_WITH_PATH(TestPAGSurface, TestPAGPlayer, TestPAGFile,
                      "resources/apitest/AsyncDecodeTest.pag");

  int currentFrame = 0;
  auto root = TestPAGPlayer->getComposition();
  auto renderCache = TestPAGPlayer->renderCache;
  while (currentFrame < root->stretchedFrameDuration()) {
    switch (currentFrame) {
      case 0:
        //落入第一个视频的预测区间的范围
        TestPAGPlayer->flush();
        EXPECT_EQ(static_cast<int>(renderCache->sequenceCaches.size()), 1);
        break;
      case 1:
        TestPAGPlayer->flush();
        EXPECT_EQ(static_cast<int>(renderCache->sequenceCaches.size()), 1);
      case 58:
        //位于静态区间，并且开始进入下个图层的预测范围
        TestPAGPlayer->flush();
        EXPECT_EQ(static_cast<int>(renderCache->sequenceCaches.size()), 1);
        break;
      case 115:
        //第二个图层开始效果
        TestPAGPlayer->flush();
        EXPECT_EQ(static_cast<int>(renderCache->sequenceCaches.size()), 0);
        break;
      case 145:
        // 开始循环
        TestPAGPlayer->flush();
        EXPECT_EQ(static_cast<int>(renderCache->sequenceCaches.size()), 1);
        // 走到这里就直接return，不需要执行后续帧的测试了
        return;
    }
    TestPAGPlayer->nextFrame();
    currentFrame++;
  }
}

/**
 * 用例描述: 校验当视频图层不用以后是否能立马回收
 */
PAG_TEST(AsyncDecode, destory_ID79139125) {
  PAG_SETUP_WITH_PATH(TestPAGSurface, TestPAGPlayer, TestPAGFile,
                      "resources/apitest/AsyncDecodeTest.pag");

  int currentFrame = 0;
  auto root = TestPAGPlayer->getComposition();
  while (currentFrame < root->stretchedFrameDuration()) {
    switch (currentFrame) {
      case 41:
        //第一个图层开始销毁
        TestPAGPlayer->flush();
        EXPECT_EQ(static_cast<int>(TestPAGPlayer->renderCache->sequenceCaches.size()), 0);
        return;
    }
    TestPAGPlayer->nextFrame();
    currentFrame++;
  }
}

/**
 * 用例描述: 验证当触发异步加载以后，立马销毁资源场景
 */
PAG_TEST(AsyncDecode, release_ID79427343) {
  PAG_SETUP_WITH_PATH(TestPAGSurface, TestPAGPlayer, TestPAGFile,
                      "resources/apitest/AsyncDecodeTest.pag");

  TestPAGPlayer->flush();
  TestPAGPlayer->setProgress(0.5);
  TestPAGPlayer->flush();
  TestPAGPlayer->setComposition(nullptr);
  TestPAGFile = nullptr;
  TestPAGPlayer->flush();
}

/**
 * 用例描述: 位图序列帧异步解码校验
 */
PAG_TEST(AsyncDecode, bitmapCompDecodeTest) {
  PAG_SETUP_WITH_PATH(TestPAGSurface, TestPAGPlayer, TestPAGFile,
                      "resources/apitest/AsyncDecodeTest.pag");

  //          25--------------------------60
  //    5----------------------45
  auto pagFile = LoadPAGFile("resources/apitest/BitmapComp.pag");
  EXPECT_TRUE(pagFile != nullptr);
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0);
  int currentFrame = 0;
  auto renderCache = TestPAGPlayer->renderCache;
  while (currentFrame < pagFile->stretchedFrameDuration()) {
    TestPAGPlayer->flush();
    switch (currentFrame) {
      case 0:
        EXPECT_EQ(static_cast<int>(renderCache->sequenceCaches.size()), 1);
        break;
      case 10:
        EXPECT_EQ(static_cast<int>(renderCache->sequenceCaches.size()), 2);
        break;
      case 44:
        EXPECT_EQ(static_cast<int>(renderCache->sequenceCaches.size()), 2);
        break;
      case 45:
        EXPECT_EQ(static_cast<int>(renderCache->sequenceCaches.size()), 1);
        break;
      case 50:
        // 开始循环预测
        EXPECT_EQ(static_cast<int>(renderCache->sequenceCaches.size()), 2);
        break;
      default:
        break;
    }
    TestPAGPlayer->nextFrame();
    currentFrame++;
  }
}

/**
 * 用例描述: 图片异步解码校验
 */
PAG_TEST(AsyncDecode, imageDecodeTest) {
  PAG_SETUP_WITH_PATH(TestPAGSurface, TestPAGPlayer, TestPAGFile,
                      "resources/apitest/AsyncDecodeTest.pag");

  // 0----------15
  //         10-----------35
  //                           40---------60
  auto pagFile = LoadPAGFile("resources/apitest/ImageDecodeTest.pag");
  EXPECT_TRUE(pagFile != nullptr);
  auto pagImage = MakePAGImage("resources/apitest/imageReplacement.png");
  pagFile->replaceImage(1, pagImage);
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0);
  int currentFrame = 0;
  auto renderCache = TestPAGPlayer->renderCache;
  while (currentFrame < pagFile->stretchedFrameDuration()) {
    TestPAGPlayer->flush();
    switch (currentFrame) {
      case 0: {
        ASSERT_EQ(static_cast<int>(renderCache->decodedAssetImages.size()), 1);
        auto bitmapID = renderCache->decodedAssetImages.begin()->first;
        EXPECT_EQ(bitmapID, pagImage->uniqueID());
      } break;
      case 9:
        EXPECT_EQ(static_cast<int>(renderCache->decodedAssetImages.size()), 1);
        break;
      case 10:
        ASSERT_EQ(static_cast<int>(renderCache->decodedAssetImages.size()), 0);
        break;
      case 25:
        // 进入第三个图片的预测
        EXPECT_EQ(static_cast<int>(renderCache->decodedAssetImages.size()), 1);
        break;
      case 39:
        EXPECT_EQ(static_cast<int>(renderCache->decodedAssetImages.size()), 1);
        break;
      case 40:
        EXPECT_EQ(static_cast<int>(renderCache->decodedAssetImages.size()), 0);
        break;
      case 45:
        // 开始循环预测, 本应该是 1，但是第一张图的缓存还存在，所以这里是 0.
        EXPECT_EQ(static_cast<int>(renderCache->decodedAssetImages.size()), 0);
        break;
      default:
        break;
    }
    TestPAGPlayer->nextFrame();
    currentFrame++;
  }
}
}  // namespace pag
