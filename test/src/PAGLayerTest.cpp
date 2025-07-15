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

#include <base/utils/TimeUtil.h>
#include "nlohmann/json.hpp"
#include "utils/TestUtils.h"

namespace pag {
using nlohmann::json;

/**
 * 用例描述: PAGLayerTest 测试获取layerName接口，正确返回PAGSolidLayer
 */
PAG_TEST(PAGLayerTest, layerName) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  int target = 0;
  // from 0.52s-10s
  auto layer = GetLayer(TestPAGFile, LayerType::Solid, target);
  ASSERT_NE(layer, nullptr);
  ASSERT_EQ(layer->layerName(), "PAGSolidLayer");
}

/**
 * 用例描述: PAGLayerTest 测试frameRate准确性，正确为25
 */
PAG_TEST(PAGLayerTest, frameRate) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  int target = 0;
  // from 0.52s-10s
  auto layer = GetLayer(TestPAGFile, LayerType::Solid, target);
  ASSERT_NE(layer, nullptr);
  ASSERT_EQ(layer->frameRate(), 25);
}

/**
 * 用例描述: PAGLayerTest 测试获取startTime接口的准确性，正确为13 * 1000000 / 25
 */
PAG_TEST(PAGLayerTest, startTime) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  int target = 0;
  // from 0.52s-10s
  auto layer = GetLayer(TestPAGFile, LayerType::Solid, target);
  ASSERT_NE(layer, nullptr);
  ASSERT_EQ(layer->startTime(), 13 * 1000000 / 25);
}

/**
 * 用例描述: PAGLayerTest 测试获取pag的duration接口，正确为10 * 1000000 - 13 * 1000000 / 25
 */
PAG_TEST(PAGLayerTest, duration) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  int target = 0;
  // from 0.52s-10s
  auto layer = GetLayer(TestPAGFile, LayerType::Solid, target);
  ASSERT_EQ(layer->duration(), 10 * 1000000 - 13 * 1000000 / 25);
}

/**
 * 用例描述: PAGLayerTest测试trackMatteLayer图层是否正常
 */
PAG_TEST(PAGLayerTest, trackMatteLayer) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  int target = 0;
  // from 0.52s-10s
  auto layer = GetLayer(TestPAGFile, LayerType::Solid, target);
  auto trackMatteLayer = layer->trackMatteLayer();

  ASSERT_NE(trackMatteLayer, nullptr);
  ASSERT_EQ(trackMatteLayer->layerName(), "TrackMatteLayer");
}

/**
 * 用例描述: PAGLayerTest测试parent接口是否正确，正确为RootLayer
 */
PAG_TEST(PAGLayerTest, parent) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  int target = 0;
  // from 0.52s-10s
  auto layer = GetLayer(TestPAGFile, LayerType::Solid, target);
  auto parent = layer->parent();
  ASSERT_NE(parent, nullptr);
  ASSERT_EQ(parent->layerName(), "RootLayer");
}

/**
 * 用例描述: PAGLayerTest测试local global时间转换接口
 */
PAG_TEST(PAGLayerTest, localGlobalConvert) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  int target = 0;
  // from 0.52s-10s
  auto layer = GetLayer(TestPAGFile, LayerType::Solid, target);
  auto parent = layer->parent();
  ASSERT_NE(parent, nullptr);
  ASSERT_EQ(parent->layerName(), "RootLayer");
  ASSERT_EQ(layer->localTimeToGlobal(0), 0);
  ASSERT_EQ(layer->globalToLocalTime(520 * 1000), 520 * 1000);
}

/**
 * 用例描述: PAGLayerTest测试bounds接口，正确返回(0, 0, 720, 1080)
 */
PAG_TEST(PAGLayerTest, bounds) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  int target = 0;
  // from 0.52s-10s
  auto layer = GetLayer(TestPAGFile, LayerType::Solid, target);
  auto bounds = layer->getBounds();
  auto cBounds = Rect::MakeLTRB(0, 0, 720, 1080);
  ASSERT_TRUE(bounds == cBounds);
}

/**
 * 用例描述: PAGLayerTest测试markers接口，确保marker准确
 */
PAG_TEST(PAGLayerTest, markers) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  int target = 0;
  // from 0.52s-10s
  auto layer = GetLayer(TestPAGFile, LayerType::Solid, target);
  auto markers = layer->markers();
  ASSERT_TRUE(markers.size() == 2);
  ASSERT_EQ(markers[0]->startTime, 125);
  ASSERT_EQ(markers[0]->duration, 0);
  ASSERT_EQ(markers[0]->comment, "1");
  ASSERT_EQ(markers[1]->startTime, 150);
  ASSERT_EQ(markers[1]->duration, 0);
  ASSERT_EQ(markers[1]->comment, "2");
}

/**
 * 用例描述: 测试trackMatte的渲染结果。
 */
PAG_TEST(PAGLayerTest, trackMatte) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  // from 0.52s-10s
  TestPAGFile->setCurrentTime(800 * 1000);
  ASSERT_EQ(TestPAGFile->currentTime(), 800 * 1000);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGLayerTest/trackMatte"));
}

/**
 * 用例描述: PAGLayerTest测试visible接口
 */
PAG_TEST(PAGLayerTest, visible) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  int target = 0;
  TestPAGFile->setCurrentTime(5 * 1000000);
  target = 0;
  auto textLayer1 = GetLayer(TestPAGFile, LayerType::Text, target);
  ASSERT_NE(textLayer1, nullptr);
  textLayer1->setVisible(false);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGLayerTest/visible"));
}

/**
 * 用例描述: PAGLayerTest测试matrix接口
 */
PAG_TEST(PAGLayerTest, matrix) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  TestPAGFile->setCurrentTime(5 * 1000000);
  int target = 0;
  auto textLayer1 = GetLayer(TestPAGFile, LayerType::Text, target);
  ASSERT_NE(textLayer1, nullptr);
  textLayer1->setVisible(false);
  target = 1;
  auto textLayer2 = GetLayer(TestPAGFile, LayerType::Text, target);
  ASSERT_NE(textLayer2, nullptr);
  auto cM = Matrix();
  cM.setAll(1, 0, 728, 0, 1, 871, 0, 0, 1);
  auto mtx = textLayer2->getTotalMatrix();
  ASSERT_TRUE(cM == mtx);
  mtx.preScale(2, 2);
  textLayer2->setMatrix(mtx);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGLayerTest/matrix"));
}

/**
 * 用例描述: PAGLayerTest测试nextFrame接口
 */
PAG_TEST(PAGLayerTest, nextFrame) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  TestPAGFile->setCurrentTime(1960000ll);
  TestPAGFile->nextFrame();
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGLayerTest/nextFrame"));
}

/**
 * 用例描述: PAGLayerTest测试preFrame接口
 */
PAG_TEST(PAGLayerTest, preFrame) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  TestPAGFile->setCurrentTime(1960000ll);
  TestPAGFile->nextFrame();
  TestPAGFile->preFrame();
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGLayerTest/preFrame"));
}

/**
 * 用例描述: goToFrame测试
 */
PAG_TEST(PAGLayerFrameTest, goToFrame) {
  PAG_SETUP_WITH_PATH(TestPAGSurface, TestPAGPlayer, TestPAGFile, "assets/repeater.pag");
  ASSERT_FALSE(TestPAGFile->gotoTime(0));
  ASSERT_TRUE(TestPAGFile->gotoTime(FrameToTime(28, TestPAGFile->frameRateInternal())));
  ASSERT_TRUE(TestPAGFile->gotoTime(FrameToTime(29, TestPAGFile->frameRateInternal())));
  ASSERT_TRUE(TestPAGFile->gotoTime(FrameToTime(30, TestPAGFile->frameRateInternal())));
  ASSERT_FALSE(TestPAGFile->gotoTime(FrameToTime(31, TestPAGFile->frameRateInternal())));
  ASSERT_FALSE(TestPAGFile->gotoTime(FrameToTime(70, TestPAGFile->frameRateInternal())));
  //从区间内切换到区间外会有一次成功
  ASSERT_TRUE(TestPAGFile->gotoTime(FrameToTime(600, TestPAGFile->frameRateInternal())));
  ASSERT_FALSE(TestPAGFile->gotoTime(FrameToTime(601, TestPAGFile->frameRateInternal())));
}

/**
 * 用例描述: alpha 遮罩
 */
PAG_TEST(PAGLayerTest, AlphaMask) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  auto pagFile = LoadPAGFile("resources/apitest/AlphaTrackMatte.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0.78);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGLayerTest/AlphaMask"));
}

/**
 * 用例描述: 文字缩放
 */
PAG_TEST(PAGLayerTest, TextScale) {
  auto pagFile = LoadPAGFile("resources/apitest/TEXT04.pag");
  auto textData = pagFile->getTextData(0);
  textData->text = "耶";
  pagFile->replaceText(0, textData);
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(500, 500);
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  auto centerX = static_cast<float>(pagSurface->width()) * 0.5f;
  auto centerY = static_cast<float>(pagSurface->height()) * 0.5f;
  auto fileCenterX = static_cast<float>(pagFile->width()) * 0.5f;
  auto fileCenterY = static_cast<float>(pagFile->height()) * 0.5f;
  auto matrix = Matrix::I();
  matrix.postTranslate(centerX - fileCenterX, centerY - fileCenterY);
  matrix.postScale(10.187f, 10.187f, centerX, centerY);
  pagPlayer->setMatrix(matrix);
  pagPlayer->setProgress(0.5f);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGLayerTest/TextScale"));
}

/**
 * 用例描述: 透明度设置
 */
PAG_TEST(PAGLayerTest, LayerAlpha) {
  auto pagFile = LoadPAGFile("resources/apitest/AlphaTrackMatte.pag");
  ASSERT_NE(pagFile, nullptr);
  pagFile->setAlpha(0.5f);
  ASSERT_TRUE(fabsf(pagFile->alpha() - 0.5f) < 0.01);
  auto pagSurface = OffscreenSurface::Make(500, 500);
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setProgress(0.5f);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGLayerTest/LayerAlpha"));
}

/**
 * 用例描述: trackMatte-luma
 */
PAG_TEST(PAGLayerTest, trackMatte_luma) {
  auto pagFile = LoadPAGFile("resources/apitest/LumaTrackMatte.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setProgress(0.5f);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGLayerTest/trackMatte_luma"));
}
}  // namespace pag
