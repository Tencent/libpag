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

#include <base/utils/TimeUtil.h>
#include <iostream>
#include "framework/pag_test.h"
#include "framework/utils/PAGTestUtils.h"
#include "nlohmann/json.hpp"

namespace pag {
using nlohmann::json;

PAG_TEST_CASE(PAGLayerTest)

/**
 * 用例描述: PAGLayerTest 测试获取layerName接口，正确返回PAGSolidLayer
 */
PAG_TEST_F(PAGLayerTest, layerName) {
  int target = 0;
  // from 0.52s-10s
  auto layer = GetLayer(TestPAGFile, LayerType::Solid, target);
  ASSERT_NE(layer, nullptr);
  ASSERT_EQ(layer->layerName(), "PAGSolidLayer");
}

/**
 * 用例描述: PAGLayerTest 测试frameRate准确性，正确为25
 */
PAG_TEST_F(PAGLayerTest, frameRate) {
  int target = 0;
  // from 0.52s-10s
  auto layer = GetLayer(TestPAGFile, LayerType::Solid, target);
  ASSERT_NE(layer, nullptr);
  ASSERT_EQ(layer->frameRate(), 25);
}

/**
 * 用例描述: PAGLayerTest 测试获取startTime接口的准确性，正确为13 * 1000000 / 25
 */
PAG_TEST_F(PAGLayerTest, startTime) {
  int target = 0;
  // from 0.52s-10s
  auto layer = GetLayer(TestPAGFile, LayerType::Solid, target);
  ASSERT_NE(layer, nullptr);
  ASSERT_EQ(layer->startTime(), 13 * 1000000 / 25);
}

/**
 * 用例描述: PAGLayerTest 测试获取pag的duration接口，正确为10 * 1000000 - 13 * 1000000 / 25
 */
PAG_TEST_F(PAGLayerTest, duration) {
  int target = 0;
  // from 0.52s-10s
  auto layer = GetLayer(TestPAGFile, LayerType::Solid, target);
  ASSERT_EQ(layer->duration(), 10 * 1000000 - 13 * 1000000 / 25);
}

/**
 * 用例描述: PAGLayerTest测试trackMatteLayer图层是否正常
 */
PAG_TEST_F(PAGLayerTest, trackMatteLayer) {
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
PAG_TEST_F(PAGLayerTest, parent) {
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
PAG_TEST_F(PAGLayerTest, localGlobalConvert) {
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
PAG_TEST_F(PAGLayerTest, bounds) {
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
PAG_TEST_F(PAGLayerTest, markers) {
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
 * 用例描述: PAGLayerTest测试trackMatte接口，对比获取到的MD5
 */
PAG_TEST_F(PAGLayerTest, trackMatte) {
  // from 0.52s-10s
  TestPAGFile->setCurrentTime(800 * 1000);
  ASSERT_EQ(TestPAGFile->currentTime(), 800 * 1000);
  TestPAGPlayer->flush();
  auto tmMd5 = getMd5FromSnap();
  ASSERT_NE(tmMd5, "");
  PAGTestEnvironment::DumpJson["PAGLayerTest"]["layer"]["trackMatte"] = tmMd5;
#ifdef COMPARE_JSON_PATH
  auto cTrackMatte = PAGTestEnvironment::CompareJson["PAGLayerTest"]["layer"]["trackMatte"];
  TraceIf(TestPAGSurface, "../test/out/trackMatte.png", cTrackMatte.get<std::string>() != tmMd5);
  EXPECT_EQ(cTrackMatte.get<std::string>(), tmMd5);
#endif
}

/**
 * 用例描述: PAGLayerTest测试visible接口
 */
PAG_TEST_F(PAGLayerTest, visible) {
  int target = 0;
  TestPAGFile->setCurrentTime(5 * 1000000);
  target = 0;
  auto textLayer1 = GetLayer(TestPAGFile, LayerType::Text, target);
  ASSERT_NE(textLayer1, nullptr);
  textLayer1->setVisible(false);
  TestPAGPlayer->flush();
  auto visibleMd5 = getMd5FromSnap();
  PAGTestEnvironment::DumpJson["PAGLayerTest"]["layer"]["visible"] = visibleMd5;
#ifdef COMPARE_JSON_PATH
  auto cTrackMatte = PAGTestEnvironment::CompareJson["PAGLayerTest"]["layer"]["visible"];
  TraceIf(TestPAGSurface, "../test/out/pag_layer_visible.png",
          cTrackMatte.get<std::string>() != visibleMd5);
  EXPECT_EQ(cTrackMatte.get<std::string>(), visibleMd5);
#endif
}

/**
 * 用例描述: PAGLayerTest测试matrix接口
 */
PAG_TEST_F(PAGLayerTest, matrix) {
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
  auto matrixMd5 = getMd5FromSnap();
  ASSERT_NE(matrixMd5, "");
  PAGTestEnvironment::DumpJson["PAGLayerTest"]["layer"]["matrix"] = matrixMd5;
#ifdef COMPARE_JSON_PATH
  auto cTrackMatte = PAGTestEnvironment::CompareJson["PAGLayerTest"]["layer"]["matrix"];
  EXPECT_EQ(cTrackMatte.get<std::string>(), matrixMd5);
#endif
}

/**
 * 用例描述: PAGLayerTest测试nextFrame接口
 */
PAG_TEST_F(PAGLayerTest, nextFrame) {
  TestPAGFile->setCurrentTime(1960000ll);
  TestPAGFile->nextFrame();
  TestPAGPlayer->flush();
  auto nextFrameMd5 = getMd5FromSnap();
  ASSERT_NE(nextFrameMd5, "");
  PAGTestEnvironment::DumpJson["PAGLayerTest"]["layer"]["nextFrame"] = nextFrameMd5;
#ifdef COMPARE_JSON_PATH
  auto cTrackMatte = PAGTestEnvironment::CompareJson["PAGLayerTest"]["layer"]["nextFrame"];
  EXPECT_EQ(cTrackMatte.get<std::string>(), nextFrameMd5);
#endif
}

/**
 * 用例描述: PAGLayerTest测试preFrame接口
 */
PAG_TEST_F(PAGLayerTest, preFrame) {
  TestPAGFile->setCurrentTime(1960000ll);
  TestPAGFile->nextFrame();
  TestPAGFile->preFrame();
  TestPAGPlayer->flush();
  auto preFrameMd5 = getMd5FromSnap();
  PAGTestEnvironment::DumpJson["PAGLayerTest"]["layer"]["preFrame"] = preFrameMd5;
#ifdef COMPARE_JSON_PATH
  auto cTrackMatte = PAGTestEnvironment::CompareJson["PAGLayerTest"]["layer"]["preFrame"];
  EXPECT_EQ(cTrackMatte.get<std::string>(), preFrameMd5);
#endif
}

PAG_TEST_CASE_WITH_PATH(PAGLayerFrameTest, "../assets/repeater.pag")

/**
 * 用例描述: goToFrame测试
 */
PAG_TEST_F(PAGLayerFrameTest, goToFrame) {
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
 * 用例描述: PAGLayer外部滤镜接口测试
 */
// TODO(liamcli) 这里带后续外部滤镜注入接口优化完，需要修改本用例
PAG_TEST_F(PAGLayerTest, external_filter_ID79286937) {
  //  auto target = 1;
  //  std::shared_ptr<PAGImageLayer> imageLayer1 = std::static_pointer_cast<PAGImageLayer>(
  //      GetLayer(TestPAGFile, LayerType::Image, target));
  //  Marker marker;
  //  marker.comment = "{\"filter\":[\"ShaderToy_BoDian\"]}";
  //
  //  auto filters = smart::SmartFilter::getShaderToyFilterParams({&marker},
  //  imageLayer1->duration(),
  //                                                              imageLayer1->frameRate());
  //  auto pagFilter = PAGFilter::FromExternal(filters.front(), 500000);
  //  imageLayer1->addFilter(pagFilter);
  //
  //  int targetIndex = 0;
  //  auto solidLayer = std::static_pointer_cast<PAGSolidLayer>(
  //      GetLayer(TestPAGFile, LayerType::Solid, targetIndex));
  //  solidLayer->setSolidColor(Black);
  //
  //  TestPAGFile->setCurrentTime(2200000);
  //  TestPAGPlayer->flush();
  //  auto md54 = DumpMD5(TestPAGSurface);
  ////        Trace(MakeSnapshot(TestPAGSurface), "../test/out/md54.png");
  //
  //  TestPAGFile->setCurrentTime(2600000);
  //  TestPAGPlayer->flush();
  //  auto md55 = DumpMD5(TestPAGSurface);
  ////        Trace(MakeSnapshot(TestPAGSurface), "../test/out/md55.png");
  //
  //  PAGTestEnvironment::DumpJson["PAGLayerFilterTest"]["filter"]["md54"] = md54;
  //  PAGTestEnvironment::DumpJson["PAGLayerFilterTest"]["filter"]["md55"] = md55;
  //#ifdef COMPARE_JSON_PATH
  //  auto cmd54 = PAGTestEnvironment::CompareJson["PAGLayerFilterTest"]["filter"]["md54"];
  //  if (cmd54 != nullptr) {
  //    EXPECT_EQ(cmd54.get<std::string>(), md54);
  //  }
  //  auto cmd55 = PAGTestEnvironment::CompareJson["PAGLayerFilterTest"]["filter"]["md55"];
  //  if (cmd55 != nullptr) {
  //    EXPECT_EQ(cmd55.get<std::string>(), md55);
  //  }
  //#endif
}

/**
 * 用例描述: alpha 遮罩
 */
PAG_TEST_F(PAGLayerTest, AlphaMask_ID80514761) {
  auto pagFile = PAGFile::Load("../resources/apitest/AlphaTrackMatte.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0.78);
  TestPAGPlayer->flush();
  auto image = MakeSnapshot(TestPAGSurface);
  auto preFrameMd5 = DumpMD5(image);
  PAGTestEnvironment::DumpJson["PAGLayerTest"]["layer"]["alphaMask"] = preFrameMd5;
#ifdef COMPARE_JSON_PATH
  auto cTrackMatte = PAGTestEnvironment::CompareJson["PAGLayerTest"]["layer"]["alphaMask"];
  TraceIf(image, "../test/out/alphaMask.png", cTrackMatte.get<std::string>() != preFrameMd5);
  EXPECT_EQ(cTrackMatte.get<std::string>(), preFrameMd5);
#endif
}

/**
 * 用例描述: 文字缩放
 */
PAG_TEST_F(PAGLayerTest, TextScale_81527319) {
  auto pagFile = PAGFile::Load("../resources/apitest/TEXT04.pag");
  auto textData = pagFile->getTextData(0);
  textData->text = "耶";
  pagFile->replaceText(0, textData);
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = PAGSurface::MakeOffscreen(500, 500);
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
  auto skImage = MakeSnapshot(pagSurface);
  auto dumpMd5 = DumpMD5(skImage);
  PAGTestEnvironment::DumpJson["PAGLayerTest"]["layer"]["textScale"] = dumpMd5;
#ifdef COMPARE_JSON_PATH
  auto textScaleMD5 = PAGTestEnvironment::CompareJson["PAGLayerTest"]["layer"]["textScale"];
  TraceIf(skImage, "../test/out/textScale.png", textScaleMD5.get<std::string>() != dumpMd5);
  EXPECT_EQ(textScaleMD5.get<std::string>(), dumpMd5);
#endif
}

/**
 * 用例描述: 透明度设置
 */
PAG_TEST_F(PAGLayerTest, Opacity) {
  auto pagFile = PAGFile::Load("../resources/apitest/AlphaTrackMatte.pag");
  ASSERT_NE(pagFile, nullptr);
  pagFile->setOpacity(128);
  ASSERT_EQ(pagFile->opacity(), 128);
  auto pagSurface = PAGSurface::MakeOffscreen(500, 500);
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setProgress(0.5f);
  pagPlayer->flush();
  auto skImage = MakeSnapshot(pagSurface);
  auto dumpMd5 = DumpMD5(skImage);
  PAGTestEnvironment::DumpJson["PAGLayerTest"]["layer"]["opacity"] = dumpMd5;
#ifdef COMPARE_JSON_PATH
  auto opacityMD5 = PAGTestEnvironment::CompareJson["PAGLayerTest"]["layer"]["opacity"];
  TraceIf(skImage, "../test/out/opacity.png", opacityMD5.get<std::string>() != dumpMd5);
  EXPECT_EQ(opacityMD5.get<std::string>(), dumpMd5);
#endif
}
}  // namespace pag
