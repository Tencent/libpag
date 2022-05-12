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

#include <iostream>
#include <thread>
#include "framework/pag_test.h"
#include "framework/utils/PAGTestUtils.h"
#include "nlohmann/json.hpp"

namespace pag {
using nlohmann::json;

PAG_TEST_SUIT(PAGImageLayerTest)

/**
 * 用例描述: PAGImageLayer 图层基础边缘 Case 测试
 */
PAG_TEST_F(PAGImageLayerTest, imageBasetTest) {
  int target = 0;
  std::shared_ptr<PAGImageLayer> imageLayer =
      std::static_pointer_cast<PAGImageLayer>(GetLayer(TestPAGFile, LayerType::Image, target));
  ASSERT_NE(imageLayer, nullptr);
  auto file = PAGImage::FromPath("../resources/font/NotoColorEmoji.ttf");
  // 非法文件都无法被decode成PAGImage，file是nullptr
  EXPECT_TRUE(file == nullptr);
  auto pag = PAGImage::FromPath("../resources/apitest/test.pag");
  EXPECT_TRUE(pag == nullptr);
  imageLayer->replaceImage(file);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGImageLayerTest/ImageReplacement_Empty"));
  imageLayer->replaceImage(pag);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGImageLayerTest/ImageReplacement_Empty"));
  imageLayer->replaceImage(nullptr);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGImageLayerTest/ImageReplacement_Empty"));
}

/**
 * 用例描述: PAGImageLayer多线程替换测试
 */
PAG_TEST_F(PAGImageLayerTest, imageMultiThreadReplace) {
  int target = 0;
  std::shared_ptr<PAGImageLayer> imageLayer =
      std::static_pointer_cast<PAGImageLayer>(GetLayer(TestPAGFile, LayerType::Image, target));
  ASSERT_NE(imageLayer, nullptr);
  ASSERT_EQ(imageLayer->editableIndex(), 1);
  ASSERT_EQ(imageLayer->contentDuration(), 2 * 1000000);
  auto image = PAGImage::FromPath("../resources/apitest/imageReplacement.png");

  TestPAGFile->setCurrentTime(3000000);
  TestPAGPlayer->flush();
  // 多线程同时替换图片
  std::thread thread1([imageLayer, image]() {
    for (int i = 0; i < 10; ++i) {
      std::cout << "线程1 present" << std::endl;
      TestPAGPlayer->flush();
    }
  });
  std::thread thread2([imageLayer, image]() {
    for (int i = 0; i < 10; ++i) {
      std::cout << "线程2 替换图片" << std::endl;
      imageLayer->replaceImage(image);
      TestPAGPlayer->flush();
    }
  });
  thread1.join();
  thread2.join();

  imageLayer->replaceImage(image);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGImageLayerTest/ImageReplacement"));
}

/**
 * 用例描述: PAGImageLayerContentDuration获取测试
 */
PAG_TEST_F(PAGImageLayerTest, imageLayerContentDuration) {
  auto testFile = PAGFile::Load("../resources/apitest/test_TimeRemapInFileRange.pag");
  // 图层 timeremap 没有超过文件显示区域
  int commonTarget = 4;
  std::shared_ptr<PAGImageLayer> imageLayer =
      std::static_pointer_cast<PAGImageLayer>(GetLayer(testFile, LayerType::Image, commonTarget));
  ASSERT_NE(imageLayer, nullptr);
  ASSERT_EQ(imageLayer->contentDuration(), 1160000);

  testFile = PAGFile::Load("../resources/apitest/test_TimeRemapBeyondFileRange.pag");
  // 图层 timeremap 都超过了文件显示区域
  imageLayer =
      std::static_pointer_cast<PAGImageLayer>(GetLayer(testFile, LayerType::Image, commonTarget));
  ASSERT_NE(imageLayer, nullptr);
  ASSERT_EQ(imageLayer->contentDuration(), 240000);

  // 变速timeremap测试用例
  testFile = PAGFile::Load("../resources/apitest/timeRemap_shift.pag");
  auto pagComposition = PAGComposition::Make(testFile->width(), testFile->height());
  auto pagImageLayer = std::static_pointer_cast<PAGImageLayer>(testFile->getLayerAt(0));
  pagComposition->addLayer(pagImageLayer);
  imageLayer = std::static_pointer_cast<pag::PAGImageLayer>(pagComposition->getLayerAt(0));
  ASSERT_NE(imageLayer, nullptr);
  ASSERT_EQ(imageLayer->contentDuration(), 4000000);
}

/**
 * 用例描述: PAGImageLayer getBounds接口 测试
 */
PAG_TEST_F(PAGImageLayerTest, getBoundsTest) {
  auto pagFile = pag::PAGFile::Load("../resources/apitest/ImageLayerBounds.pag");
  Rect rect1 = Rect::MakeXYWH(0, 0, 720, 1280);
  Rect rect2 = Rect::MakeXYWH(0, 0, 720, 720);
  Rect rect3 = Rect::MakeXYWH(0, 0, 720, 1280);
  Rect bounds[] = {rect1, rect2, rect3};
  for (int i = 0; i < pagFile->numImages(); i++) {
    auto pagImageLayer = pagFile->getLayersByEditableIndex(i, pag::LayerType::Image)[0];
    auto rect = pagImageLayer->getBounds();
    ASSERT_EQ(rect, bounds[i]);
  }
}

/**
 * 用例描述: PAGImageLayer setImage接口 测试
 */
PAG_TEST_F(PAGImageLayerTest, setImage) {
  auto pagFile = pag::PAGFile::Load("../resources/filter/fastblur.pag");

  auto image = PAGImage::FromPath("../resources/apitest/imageReplacement.png");
  auto pagImageLayer = pagFile->getLayersByEditableIndex(0, pag::LayerType::Image)[0];
  std::static_pointer_cast<PAGImageLayer>(pagImageLayer)->setImage(image);
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0.3);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGImageLayerTest/setImage"));
}
}  // namespace pag
