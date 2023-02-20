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

#include "HitTestCase.h"
#include "framework/pag_test.h"
#include "framework/utils/PAGTestUtils.h"

namespace pag {
void HitTestCase::HitTestPoint(std::shared_ptr<PAGPlayer> TestPAGPlayer,
                               std::shared_ptr<PAGFile> TestPAGFile) {
  auto testComposition = std::static_pointer_cast<PAGComposition>(TestPAGFile->getLayerAt(0));
  // 覆盖异常输入
  EXPECT_FALSE(TestPAGPlayer->hitTestPoint(TestPAGFile, -1, -1, false));
  EXPECT_FALSE(TestPAGPlayer->hitTestPoint(TestPAGFile, 0, 0, false));
  EXPECT_FALSE(TestPAGPlayer->hitTestPoint(TestPAGFile, TestPAGFile->width() * 2,
                                           TestPAGFile->height() * 2, false));

  // 时间轴移动到TrackMatte上
  testComposition->setCurrentTime(0.8 * 1000000);
  TestPAGPlayer->flush();
  EXPECT_FALSE(TestPAGPlayer->hitTestPoint(testComposition, 250, 400, true));
  EXPECT_TRUE(TestPAGPlayer->hitTestPoint(testComposition, 250, 600, true));

  int target = 0;

  // 测试SolidContent
  target = 0;
  // 大小 720*1080
  auto solidLayer = GetLayer(testComposition, LayerType::Solid, target);
  ASSERT_NE(solidLayer, nullptr);
  EXPECT_FALSE(TestPAGPlayer->hitTestPoint(solidLayer, -1, -1, true));
  // Path.contains() 是闭区间，包含边界点的。而 Rect.contains() 不包含 right 和 bottom。
  EXPECT_FALSE(TestPAGPlayer->hitTestPoint(solidLayer, 721, 1081, true));
  EXPECT_TRUE(TestPAGPlayer->hitTestPoint(solidLayer, 0, 0, true));
  EXPECT_TRUE(TestPAGPlayer->hitTestPoint(solidLayer, 720, 1080, true));

  // 测试 TextContent
  testComposition->setCurrentTime(5 * 1000000);
  TestPAGPlayer->flush();
  target = 0;
  auto textLayer = GetLayer(testComposition, LayerType::Text, target);
  ASSERT_NE(textLayer, nullptr);
  EXPECT_FALSE(TestPAGPlayer->hitTestPoint(textLayer, -1, -1, false));
  EXPECT_FALSE(TestPAGPlayer->hitTestPoint(textLayer, 720, 1080, false));
  // 在TextLayer上，但是不在字上
  EXPECT_FALSE(TestPAGPlayer->hitTestPoint(textLayer, 70, 864, true));
  EXPECT_TRUE(TestPAGPlayer->hitTestPoint(textLayer, 70, 864, false));
  // 在TextLayer上，并且在字上
  EXPECT_TRUE(TestPAGPlayer->hitTestPoint(textLayer, 100, 852, true));
  EXPECT_TRUE(TestPAGPlayer->hitTestPoint(textLayer, 100, 852, false));

  // 测试ImageContent
  // 坐标会被matrix remap，四舍五入时候会导致有差异，有可能接近边缘的某些像素会判断异常
  testComposition->setCurrentTime(3 * 1000000);
  TestPAGPlayer->flush();
  // 大小 500*654
  target = 0;
  auto imageLayer =
      std::static_pointer_cast<PAGImageLayer>(GetLayer(testComposition, LayerType::Image, target));
  ASSERT_NE(imageLayer, nullptr);
  EXPECT_FALSE(TestPAGPlayer->hitTestPoint(imageLayer, -1, -1, false));
  EXPECT_FALSE(TestPAGPlayer->hitTestPoint(imageLayer, 0, 0, false));
  EXPECT_FALSE(TestPAGPlayer->hitTestPoint(imageLayer, 721, 1081, true));
  EXPECT_FALSE(TestPAGPlayer->hitTestPoint(imageLayer, 721, 1081, false));

  EXPECT_TRUE(TestPAGPlayer->hitTestPoint(imageLayer, 366, 174, true));
  EXPECT_TRUE(TestPAGPlayer->hitTestPoint(imageLayer, 366, 174, false));

  EXPECT_TRUE(TestPAGPlayer->hitTestPoint(imageLayer, 424, 160, true));
  EXPECT_TRUE(TestPAGPlayer->hitTestPoint(imageLayer, 424, 160, true));

  // 测试ImageReplacement
  imageLayer->replaceImage(MakePAGImage("resources/apitest/imageReplacement.png"));
  TestPAGPlayer->flush();
  EXPECT_FALSE(TestPAGPlayer->hitTestPoint(imageLayer, -1, -1, false));
  EXPECT_FALSE(TestPAGPlayer->hitTestPoint(imageLayer, 0, 0, false));
  EXPECT_FALSE(TestPAGPlayer->hitTestPoint(imageLayer, 721, 1081, true));
  EXPECT_FALSE(TestPAGPlayer->hitTestPoint(imageLayer, 721, 1081, false));

  // 两张图片叠加的部分，同时imageLayer无像素
  EXPECT_FALSE(TestPAGPlayer->hitTestPoint(imageLayer, 292, 50, true));
  EXPECT_TRUE(TestPAGPlayer->hitTestPoint(imageLayer, 292, 50, false));
  // 两张图片叠加的部分，同时imageLayer有像素
  EXPECT_TRUE(TestPAGPlayer->hitTestPoint(imageLayer, 366, 174, true));
  EXPECT_TRUE(TestPAGPlayer->hitTestPoint(imageLayer, 366, 174, false));

  // imageLayer无像素
  EXPECT_FALSE(TestPAGPlayer->hitTestPoint(imageLayer, 516, 40, true));
  EXPECT_TRUE(TestPAGPlayer->hitTestPoint(imageLayer, 516, 40, false));
  // imageLayer有像素
  EXPECT_TRUE(TestPAGPlayer->hitTestPoint(imageLayer, 424, 160, true));
  EXPECT_TRUE(TestPAGPlayer->hitTestPoint(imageLayer, 424, 160, true));

  // TODO 每种LayerContent都要写测试用例
  // TODO BitmapSequenceContent
  // TODO VideoSequenceContent
}

void HitTestCase::VideoSequenceHitTestPoint(std::shared_ptr<PAGPlayer> TestPAGPlayer,
                                            std::shared_ptr<PAGFile> TestPAGFile) {
  TestPAGFile->setCurrentTime(0.8 * 1000000);
  TestPAGPlayer->flush();
  EXPECT_FALSE(TestPAGPlayer->hitTestPoint(TestPAGFile, 250, 400, true));
  EXPECT_TRUE(TestPAGPlayer->hitTestPoint(TestPAGFile, 250, 400, false));
  EXPECT_TRUE(TestPAGPlayer->hitTestPoint(TestPAGFile, 250, 600, true));
}

void HitTestCase::BitmapSequenceHitTestPoint(std::shared_ptr<PAGPlayer> TestPAGPlayer,
                                             std::shared_ptr<PAGFile> TestPAGFile) {
  TestPAGFile->setCurrentTime(0.8 * 1000000);
  TestPAGPlayer->flush();
  EXPECT_FALSE(TestPAGPlayer->hitTestPoint(TestPAGFile, 250, 400, true));
  EXPECT_TRUE(TestPAGPlayer->hitTestPoint(TestPAGFile, 250, 400, false));
  EXPECT_TRUE(TestPAGPlayer->hitTestPoint(TestPAGFile, 250, 600, true));
}

void HitTestCase::GetLayersUnderPointEdgeCase(std::shared_ptr<PAGPlayer>,
                                              std::shared_ptr<PAGFile> TestPAGFile) {
  auto testComposition = std::static_pointer_cast<PAGComposition>(TestPAGFile->getLayerAt(0));
  // 覆盖异常输入
  std::vector<std::shared_ptr<PAGLayer>> results = {};
  // TODO 这里会崩溃，需要check是否这里要检查空指针
  //        EXPECT_FALSE(TestPAGFile->getLayersUnderPoint(2, 2, nullptr));
  results = testComposition->getLayersUnderPoint(-1, -1);
  EXPECT_EQ(static_cast<int>(results.size()), 0);
  results = testComposition->getLayersUnderPoint(0, 0);
  EXPECT_EQ(static_cast<int>(results.size()), 0);
  results =
      testComposition->getLayersUnderPoint(TestPAGFile->width() * 2, TestPAGFile->height() * 2);
  EXPECT_EQ(static_cast<int>(results.size()), 0);
}

void HitTestCase::GetLayersUnderPointTrackMatte(std::shared_ptr<pag::PAGPlayer>,
                                                std::shared_ptr<pag::PAGFile> TestPAGFile) {
  auto testComposition = std::static_pointer_cast<PAGComposition>(TestPAGFile->getLayerAt(0));
  std::vector<std::shared_ptr<PAGLayer>> results = {};
  // 时间轴移动到TrackMatte上
  testComposition->setCurrentTime(0.8 * 1000000);
  // 点在TrackMatte区域
  results = testComposition->getLayersUnderPoint(250, 400);
  EXPECT_EQ(static_cast<int>(results.size()), 1);
  // 点在非TrackMatte区域
  results = testComposition->getLayersUnderPoint(250, 600);
  EXPECT_EQ(static_cast<int>(results.size()), 1);
}

void HitTestCase::GetLayersUnderPointImage(std::shared_ptr<pag::PAGPlayer> TestPAGPlayer,
                                           std::shared_ptr<pag::PAGFile> TestPAGFile) {
  auto testComposition = std::static_pointer_cast<PAGComposition>(TestPAGFile->getLayerAt(0));
  std::vector<std::shared_ptr<PAGLayer>> results = {};
  // 时间轴移动到图片上
  testComposition->setCurrentTime(3 * 1000000);
  TestPAGPlayer->flush();
  results = testComposition->getLayersUnderPoint(360, 200);
  EXPECT_EQ(static_cast<int>(results.size()), 3);

  results = testComposition->getLayersUnderPoint(100, 200);
  EXPECT_EQ(static_cast<int>(results.size()), 2);

  results = testComposition->getLayersUnderPoint(360, 500);
  EXPECT_EQ(static_cast<int>(results.size()), 1);
}

void HitTestCase::GetLayersUnderPointText(std::shared_ptr<pag::PAGPlayer>,
                                          std::shared_ptr<pag::PAGFile> TestPAGFile) {
  auto testComposition = std::static_pointer_cast<PAGComposition>(TestPAGFile->getLayerAt(0));
  std::vector<std::shared_ptr<PAGLayer>> results = {};

  // 时间轴移动到非文字上
  testComposition->setCurrentTime(5 * 1000000);
  results = testComposition->getLayersUnderPoint(500, 500);
  EXPECT_EQ(static_cast<int>(results.size()), 1);
  // 在TextLayer上，但是不在字上
  results = testComposition->getLayersUnderPoint(70, 864);
  EXPECT_EQ(static_cast<int>(results.size()), 2);
  // 在TextLayer上，并且在字上
  results = testComposition->getLayersUnderPoint(100, 852);
  EXPECT_EQ(static_cast<int>(results.size()), 2);
}

void HitTestCase::GetLayersUnderPoint(std::shared_ptr<PAGPlayer> TestPAGPlayer,
                                      std::shared_ptr<PAGFile> TestPAGFile) {
  auto testComposition = std::static_pointer_cast<PAGComposition>(TestPAGFile->getLayerAt(0));
  // 覆盖异常输入
  std::vector<std::shared_ptr<PAGLayer>> results = {};
  // TODO 这里会崩溃，需要check是否这里要检查空指针
  //        EXPECT_FALSE(TestPAGFile->getLayersUnderPoint(2, 2, nullptr));
  results = testComposition->getLayersUnderPoint(-1, -1);
  EXPECT_EQ(static_cast<int>(results.size()), 0);
  results = testComposition->getLayersUnderPoint(0, 0);
  EXPECT_EQ(static_cast<int>(results.size()), 0);
  results =
      testComposition->getLayersUnderPoint(TestPAGFile->width() * 2, TestPAGFile->height() * 2);
  EXPECT_EQ(static_cast<int>(results.size()), 0);

  // 时间轴移动到TrackMatte上
  testComposition->setCurrentTime(0.8 * 1000000);
  // 点在TrackMatte区域
  results = testComposition->getLayersUnderPoint(250, 400);
  EXPECT_EQ(static_cast<int>(results.size()), 1);
  // 点在非TrackMatte区域
  results = testComposition->getLayersUnderPoint(250, 600);
  EXPECT_EQ(static_cast<int>(results.size()), 1);

  // 时间轴移动到图片上
  testComposition->setCurrentTime(3 * 1000000);
  TestPAGPlayer->flush();
  results = testComposition->getLayersUnderPoint(360, 200);
  EXPECT_EQ(static_cast<int>(results.size()), 3);

  results = testComposition->getLayersUnderPoint(100, 200);
  EXPECT_EQ(static_cast<int>(results.size()), 2);

  results = testComposition->getLayersUnderPoint(360, 500);
  EXPECT_EQ(static_cast<int>(results.size()), 1);

  // 时间轴移动到非文字上
  testComposition->setCurrentTime(5 * 1000000);
  TestPAGPlayer->flush();
  results = testComposition->getLayersUnderPoint(500, 500);
  EXPECT_EQ(static_cast<int>(results.size()), 1);
  // 在TextLayer上，但是不在字上
  results = testComposition->getLayersUnderPoint(70, 864);
  EXPECT_EQ(static_cast<int>(results.size()), 2);
  // 在TextLayer上，并且在字上
  results = testComposition->getLayersUnderPoint(100, 852);
  EXPECT_EQ(static_cast<int>(results.size()), 2);
}
}  // namespace pag