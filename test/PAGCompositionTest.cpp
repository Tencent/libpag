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
#include "nlohmann/json.hpp"

namespace pag {
using nlohmann::json;

PAG_TEST_SUIT(PAGCompositionTest)

/**
 * 用例描述: PAGCompositionLayer基础功能
 */
PAG_TEST_F(PAGCompositionTest, composition) {
  auto pagComposition = std::static_pointer_cast<PAGComposition>(TestPAGFile->getLayerAt(0));
  ASSERT_EQ(pagComposition->width(), 720);
  ASSERT_EQ(pagComposition->height(), 1080);
  ASSERT_EQ(pagComposition->numChildren(), 6);
  pagComposition->setCurrentTime(3 * 1000000);
  pagComposition->setContentSize(540, 960);
  TestPAGPlayer->flush();

  auto imageLayer1 = std::static_pointer_cast<PAGTextLayer>(pagComposition->getLayerAt(2));
  ASSERT_NE(imageLayer1, nullptr);
  ASSERT_EQ(imageLayer1->layerName(), "PAGImageLayer1");
  ASSERT_EQ(pagComposition->getLayerIndex(imageLayer1), 2);
  ASSERT_TRUE(pagComposition->contains(imageLayer1));

  auto imageLayer2 = std::static_pointer_cast<PAGTextLayer>(pagComposition->getLayerAt(3));
  ASSERT_NE(imageLayer2, nullptr);
  ASSERT_EQ(imageLayer2->layerName(), "PAGImageLayer2");

  pagComposition->swapLayer(imageLayer1, imageLayer2);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGCompositionTest/composition_swapLayer"));

  pagComposition->swapLayerAt(2, 3);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGCompositionTest/composition_swapLayerAt"));

  pagComposition->setLayerIndex(imageLayer1, 3);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGCompositionTest/composition_setLayerIndex"));

  pagComposition->removeLayer(imageLayer1);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGCompositionTest/composition_removeLayer"));

  pagComposition->removeLayerAt(2);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGCompositionTest/composition_removeLayerAt"));

  pagComposition->removeAllLayers();
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGCompositionTest/composition_empty"));

  auto pagFile2 = PAGFile::Load(TestConstants::DEFAULT_PAG_PATH);
  auto pagComposition2 = std::static_pointer_cast<PAGComposition>(pagFile2->getLayerAt(0));
  auto imageLayer = pagComposition2->getLayerAt(2);
  pagComposition->addLayer(imageLayer);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGCompositionTest/composition_empty"));

  pagComposition->addLayerAt(pagComposition2->getLayerAt(3), 0);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGCompositionTest/composition_empty"));

  pagComposition->setContentSize(300, 300);
  ASSERT_EQ(pagComposition->width(), 300);
  ASSERT_EQ(pagComposition->height(), 300);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGCompositionTest/composition_empty"));
}

/**
 * 用例描述: VideoSequence的大小和Composition不一致
 */
PAG_TEST_F(PAGCompositionTest, VideoSequence) {
  auto pagFile = PAGFile::Load(TestConstants::RESOURCES_ROOT + "apitest/video_sequence_size.pag");
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  auto pagPlayer = std::make_unique<PAGPlayer>();
  pagPlayer->setComposition(pagFile);
  pagPlayer->setSurface(pagSurface);
  pagFile->setMatrix(Matrix::MakeScale(0.8625));
  pagPlayer->setProgress(0.5);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGCompositionTest/VideoSequence"));
}

// ContainerTest 中会操作容器，所以此处需要声明为case，不能声明为suit
PAG_TEST_CASE(ContainerTest)

/**
 * 用例描述: PAGCompositionLayer getLayerAt
 */
PAG_TEST_F(ContainerTest, getLayerAt) {
  auto pagComposition = std::static_pointer_cast<PAGComposition>(TestPAGFile->getLayerAt(0));

  // case 0: 合法索引
  auto pagLayer = pagComposition->getLayerAt(0);
  auto pagLayer2 = pagLayer->parent()->getLayerAt(0);
  EXPECT_EQ(pagLayer, pagLayer2);

  // Case 1: index < 0
  pagLayer = pagComposition->getLayerAt(-1);
  EXPECT_EQ(pagLayer, nullptr);

  // Case 2: index >= layers总数
  pagLayer = pagComposition->getLayerAt(pagComposition->numChildren());
  EXPECT_EQ(pagLayer, nullptr);

  // case 3: 当前PAGComposition是空/等同于索引越界
  auto emptyPAGComposition = PAGComposition::Make(100, 100);
  auto emptyPagLayer = emptyPAGComposition->getLayerAt(0);
  EXPECT_EQ(emptyPagLayer, nullptr);

  // case 4: 通过Make创建空容器
  emptyPAGComposition = PAGComposition::Make(100, 100);
  emptyPagLayer = emptyPAGComposition->getLayerAt(0);
  EXPECT_EQ(emptyPagLayer, nullptr);
}

/**
 * 用例描述: PAGCompositionLayer getLayerIndex
 */
PAG_TEST_F(ContainerTest, getLayerIndex) {
  auto pagComposition = std::static_pointer_cast<PAGComposition>(TestPAGFile->getLayerAt(0));

  // case 0: pagLayer 是当前PAGComposition的子Layer
  auto pagLayer = pagComposition->getLayerAt(1);
  auto index = pagComposition->getLayerIndex(pagLayer);
  EXPECT_EQ(index, 1);

  // case 1: pagLayer = nullptr
  index = pagComposition->getLayerIndex(nullptr);
  EXPECT_LT(index, 0);

  // case 2: pagLayer指向另外一个PAGComposition的Layer
  auto pagFile = PAGFile::Load(GetPagPath());
  auto pagLayer2 = pagFile->getLayerAt(0);

  index = pagComposition->getLayerIndex(pagLayer2);
  EXPECT_LT(index, 0);

  // case 3: 当前PAGComposition是空
  auto emptyPAGComposition = PAGComposition::Make(100, 100);
  index = emptyPAGComposition->getLayerIndex(pagLayer);
  EXPECT_LT(index, 0);
}

/**
 * 用例描述: ContainerTest setLayerIndex
 */
PAG_TEST_F(ContainerTest, setLayerIndex) {
  auto pagComposition = std::static_pointer_cast<PAGComposition>(TestPAGFile->getLayerAt(0));
  // case 0: 正常情况
  auto pagLayer = pagComposition->getLayerAt(1);
  pagComposition->setLayerIndex(pagLayer, 0);
  auto layerIndex = pagComposition->getLayerIndex(pagLayer);
  EXPECT_EQ(layerIndex, 0);

  // case 1: 当前PAGComposition是空
  auto emptyPAGComposition = PAGComposition::Make(100, 100);
  emptyPAGComposition->setLayerIndex(pagLayer, 0);
  EXPECT_EQ(pagLayer->parent() == emptyPAGComposition, false);

  // case 2: 当前pagLayer不是当前PAGComposition的子Layer
  auto emptyPAGLayer = PAGImageLayer::Make(100, 100, 100);
  pagComposition->setLayerIndex(emptyPAGLayer, 0);
  auto index = pagComposition->getLayerIndex(emptyPAGLayer);
  EXPECT_LT(index, 0);

  auto count = pagComposition->numChildren();
  // case 3: index <0
  pagComposition->setLayerIndex(pagLayer, -1);
  index = pagComposition->getLayerIndex(pagLayer);
  EXPECT_EQ(index, count - 1);

  // case 4: index 超出layers数组范围
  count = pagComposition->numChildren();
  pagComposition->setLayerIndex(pagLayer, count);
  index = pagComposition->getLayerIndex(pagLayer);
  EXPECT_EQ(index, count - 1);
}

/**
 * 用例描述: ContainerTest addLayer
 */
PAG_TEST_F(ContainerTest, addLayer) {
  auto pagComposition = std::static_pointer_cast<PAGComposition>(TestPAGFile->getLayerAt(0));
  // case 0: 正常情况 参数pagLayer是另外一个PAGComposition的子Layer
  auto pagFile = PAGFile::Load(getPagPath());
  auto addLayer = pagFile->getLayerAt(0);
  pagComposition->addLayer(addLayer);
  EXPECT_EQ(addLayer->parent() == pagComposition, true);
  EXPECT_EQ(pagComposition->contains(addLayer), true);

  // case 1: 参数pagLayer = nullptr
  auto result = pagComposition->addLayer(nullptr);
  EXPECT_EQ(result, false);

  // case 2: 参数pagLayer是当前PAGComposition的子layer,相当于调整顺序
  auto count = pagComposition->numChildren();
  auto pagLayer = pagComposition->getLayerAt(0);
  pagComposition->addLayer(pagLayer);
  auto index = pagComposition->getLayerIndex(pagLayer);
  EXPECT_EQ(index, count - 1);

  // case 3: 参数pagLayer是当前PAGComposition或者是当前PAGComposition的父亲节点
  EXPECT_FALSE(pagComposition->addLayer(pagComposition));
}

/**
 * 用例描述: ContainerTest addLayerAt
 */
PAG_TEST_F(ContainerTest, addLayerAt) {
  auto pagComposition = std::static_pointer_cast<PAGComposition>(TestPAGFile->getLayerAt(0));

  auto pagFile = PAGFile::Load(getPagPath());
  auto targetLayer = pagFile->getLayerAt(0);
  pagComposition->addLayer(targetLayer);

  // case 0: 正常情况
  auto result = pagComposition->addLayerAt(targetLayer, 1);
  EXPECT_EQ(result, true);
  EXPECT_EQ(targetLayer->parent() == pagComposition, true);
  EXPECT_EQ(pagComposition->contains(targetLayer), true);
  EXPECT_EQ(pagComposition->getLayerIndex(targetLayer), 1);

  pagComposition->removeLayer(targetLayer);
  // case 1: index <0
  auto count = pagComposition->numChildren();
  EXPECT_TRUE(pagComposition->addLayerAt(targetLayer, -1));
  EXPECT_EQ(pagComposition->getLayerIndex(targetLayer), count);

  // case 2: index超出范围，添加当前Composition下的layer会导致位置重排
  count = pagComposition->numChildren();
  EXPECT_TRUE(pagComposition->addLayerAt(targetLayer, count));
  EXPECT_EQ(pagComposition->getLayerIndex(targetLayer), pagComposition->numChildren() - 1);
  //剩余测试和 addLayer一样
}

/**
 * 用例描述: ContainerTest contains
 */
PAG_TEST_F(ContainerTest, contains) {
  auto pagComposition = std::static_pointer_cast<PAGComposition>(TestPAGFile->getLayerAt(0));
  // case 0: 正常情况
  auto pagLayer = pagComposition->getLayerAt(0);
  auto result = pagComposition->contains(pagLayer);
  EXPECT_EQ(result, true);

  // case 1: 参数pagLayer 为空
  EXPECT_FALSE(pagComposition->contains(nullptr));

  // case 2: 当前PAGComposition为空
  auto emptyPAGComposition = PAGComposition::Make(100, 100);
  result = emptyPAGComposition->contains(pagLayer);
  EXPECT_EQ(result, false);

  // case 3: PAGLayer是另外一个PAGComposition的
  auto emptyPAGLayer = PAGImageLayer::Make(100, 100, 100);
  result = pagComposition->contains(emptyPAGLayer);
  EXPECT_EQ(result, false);
}

/**
 * 用例描述: ContainerTest removeLayer
 */
PAG_TEST_F(ContainerTest, removeLayer) {
  auto pagComposition = std::static_pointer_cast<PAGComposition>(TestPAGFile->getLayerAt(0));
  // case 0: 正常情况
  auto pagLayer = pagComposition->getLayerAt(0);
  EXPECT_EQ(pagLayer->parent(), pagComposition);
  auto result = pagComposition->removeLayer(pagLayer);
  EXPECT_EQ(pagLayer->parent(), nullptr);
  EXPECT_EQ(result == pagLayer, true);
  auto check = pagComposition->contains(pagLayer);
  EXPECT_EQ(check, false);

  // case 1: 传入nullptr参数
  result = pagComposition->removeLayer(nullptr);
  EXPECT_EQ(result == nullptr, true);

  // case 2: pagLayer不属于当前PAGComposition
  auto emptyPAGLayer = PAGImageLayer::Make(100, 100, 100);
  result = pagComposition->removeLayer(emptyPAGLayer);
  EXPECT_EQ(result == nullptr, true);

  // case 3: PAGLayer是当前PAGComposition
  result = pagComposition->removeLayer(pagComposition);
  EXPECT_EQ(result == nullptr, true);
}

/**
 * 用例描述: ContainerTest removeLayerAt
 */
PAG_TEST_F(ContainerTest, removeLayerAt) {
  auto pagComposition = std::static_pointer_cast<PAGComposition>(TestPAGFile->getLayerAt(0));
  // case 0: index <0
  auto result = pagComposition->removeLayerAt(-1);
  EXPECT_EQ(result, nullptr);

  // case 1: index 超出范围
  result = pagComposition->removeLayerAt(pagComposition->numChildren());
  EXPECT_EQ(result, nullptr);

  // case 2: 正常情况
  auto pagLayer = pagComposition->getLayerAt(0);
  EXPECT_EQ(pagLayer->parent() == pagComposition, true);
  result = pagComposition->removeLayerAt(0);
  EXPECT_EQ(pagLayer->parent(), nullptr);
  auto check = pagComposition->contains(pagLayer);
  EXPECT_EQ(check, false);
}

/**
 * 用例描述: ContainerTest removeAllLayers
 */
PAG_TEST_F(ContainerTest, removeAllLayers) {
  auto pagComposition = std::static_pointer_cast<PAGComposition>(TestPAGFile->getLayerAt(0));
  auto layerCount = pagComposition->numChildren();
  EXPECT_LE(layerCount, 6);
  auto pagLayer = pagComposition->getLayerAt(0);
  EXPECT_TRUE(pagLayer->parent() == pagComposition);
  pagComposition->removeAllLayers();
  layerCount = pagComposition->numChildren();
  EXPECT_EQ(layerCount, 0);
  EXPECT_EQ(pagLayer->parent(), nullptr);
}

/**
 * 用例描述: ContainerTest swapLayerAt
 */
PAG_TEST_F(ContainerTest, swapLayerAt) {
  auto pagComposition = std::static_pointer_cast<PAGComposition>(TestPAGFile->getLayerAt(0));
  // case 0: 正常情况
  auto pagLayer0 = pagComposition->getLayerAt(0);
  auto pagLayer1 = pagComposition->getLayerAt(1);
  pagComposition->swapLayerAt(0, 1);
  auto pagLayer00 = pagComposition->getLayerAt(0);
  auto pagLayer01 = pagComposition->getLayerAt(1);
  EXPECT_TRUE(pagLayer0 == pagLayer01);
  EXPECT_TRUE(pagLayer1 == pagLayer00);

  // case 1: index1 范围内，index2 范围外
  auto pagLayer = pagComposition->getLayerAt(0);
  pagComposition->swapLayerAt(0, -1);
  auto index = pagComposition->getLayerIndex(pagLayer);
  EXPECT_EQ(index, 0);

  pagComposition->swapLayerAt(0, pagComposition->numChildren());
  index = pagComposition->getLayerIndex(pagLayer);
  EXPECT_EQ(index, 0);

  // case 2: index1 范围外，index2 范围内
  pagLayer = pagComposition->getLayerAt(0);
  pagComposition->swapLayerAt(-1, 0);
  index = pagComposition->getLayerIndex(pagLayer);
  EXPECT_EQ(index, 0);

  pagComposition->swapLayerAt(pagComposition->numChildren(), 0);
  index = pagComposition->getLayerIndex(pagLayer);
  EXPECT_EQ(index, 0);
}

/**
 * 用例描述: ContainerTest swapLayer
 */
PAG_TEST_F(ContainerTest, swapLayer) {
  auto pagComposition = std::static_pointer_cast<PAGComposition>(TestPAGFile->getLayerAt(0));
  // case 0: 正常情况
  auto pagLayer0 = pagComposition->getLayerAt(0);
  auto pagLayer1 = pagComposition->getLayerAt(1);
  pagComposition->swapLayer(pagLayer0, pagLayer1);
  auto index0 = pagComposition->getLayerIndex(pagLayer0);
  EXPECT_EQ(index0, 1);
  auto index1 = pagComposition->getLayerIndex(pagLayer1);
  EXPECT_EQ(index1, 0);

  // case 2: pagLayer1是nullptr 或者 pagLayer2是nullptr,另外一个是当前PAGComposition的子Layer
  auto pagLayer = pagComposition->getLayerAt(0);
  pagComposition->swapLayer(nullptr, pagLayer);
  auto index = pagComposition->getLayerIndex(pagLayer);
  EXPECT_EQ(index, 0);

  pagLayer = pagComposition->getLayerAt(0);
  pagComposition->swapLayer(pagLayer, nullptr);
  index = pagComposition->getLayerIndex(pagLayer);
  EXPECT_EQ(index, 0);

  // case 3: pagLayer1指向另外一个PAGComposition的子Layer , pagLayer2是当前PAGComposition的子Layer
  pagLayer = pagComposition->getLayerAt(0);
  auto emptyLayer = PAGImageLayer::Make(100, 100, 100);
  pagComposition->swapLayer(pagLayer, emptyLayer);
  index = pagComposition->getLayerIndex(pagLayer);
  EXPECT_EQ(index, 0);

  pagLayer = pagComposition->getLayerAt(0);
  pagComposition->swapLayer(emptyLayer, pagLayer);
  index = pagComposition->getLayerIndex(pagLayer);
  EXPECT_EQ(index, 0);
}

/**
 * 用例描述: ContainerTest HitTestPoint
 */
PAG_TEST_F(ContainerTest, HitTestPoint) {
  HitTestCase::HitTestPoint(TestPAGPlayer, TestPAGFile);
}

PAG_TEST_SUIT_WITH_PATH(VideoSequenceHitTestPoint,
                        pag::TestConstants::RESOURCES_ROOT + "apitest/video_sequence_test.pag")

/**
 * 用例描述: 视频序列帧HitTest
 */
PAG_TEST_F(VideoSequenceHitTestPoint, VideoSequenceHitTestPoint) {
  HitTestCase::VideoSequenceHitTestPoint(TestPAGPlayer, TestPAGFile);
}

PAG_TEST_SUIT_WITH_PATH(BitmapSequenceHitTestPoint,
                        pag::TestConstants::RESOURCES_ROOT + "apitest/bitmap_sequence_test.pag")

/**
 * 用例描述: 图片序列帧HitTest
 */
PAG_TEST_F(BitmapSequenceHitTestPoint, BitmapSequenceHitTestPoint) {
  HitTestCase::BitmapSequenceHitTestPoint(TestPAGPlayer, TestPAGFile);
}

/**
 * 用例描述: GetLayersUnderPoint 边缘Case测试
 */
PAG_TEST_F(ContainerTest, GetLayersUnderPointEdgeCase) {
  HitTestCase::GetLayersUnderPointEdgeCase(TestPAGPlayer, TestPAGFile);
}

/**
 * 用例描述: TrackMatte图层 GetLayersUnderPoint 测试
 */
PAG_TEST_F(ContainerTest, GetLayersUnderPointTrackMatte) {
  HitTestCase::GetLayersUnderPointTrackMatte(TestPAGPlayer, TestPAGFile);
}

/**
 * 用例描述: Text图层 GetLayersUnderPoint 测试
 */
PAG_TEST_F(ContainerTest, GetLayersUnderPointText) {
  HitTestCase::GetLayersUnderPointText(TestPAGPlayer, TestPAGFile);
}

/**
 * 用例描述: Image图层 GetLayersUnderPoint 测试
 */
PAG_TEST_F(ContainerTest, GetLayersUnderPointImage) {
  HitTestCase::GetLayersUnderPointImage(TestPAGPlayer, TestPAGFile);
}
}  // namespace pag
