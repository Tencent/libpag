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

#include <rendering/renderers/TextAnimatorRenderer.h>
#include <iostream>
#include <thread>
#include "base/utils/Log.h"
#include "nlohmann/json.hpp"
#include "pag/file.h"
#include "rendering/renderers/TextRenderer.h"
#include "utils/TestUtils.h"

namespace pag {
using nlohmann::json;

/**
 * 用例描述: PAGTextLayerTest fillColor 接口测试
 */
PAG_TEST(PAGTextLayerTest, fillColor) {
  PAG_SETUP_ISOLATED(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  TestPAGFile->setCurrentTime(5 * 1000000);
  int target = 0;
  auto layer = GetLayer(TestPAGFile, LayerType::Text, target);
  ASSERT_NE(layer, nullptr) << "not get a textLayer" << std::endl;
  auto textLayer = std::static_pointer_cast<pag::PAGTextLayer>(layer);
  auto fillColor = textLayer->fillColor();
  EXPECT_TRUE(fillColor == Blue);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGTextLayerTest/fillColor"));
  // 色值限定uint_8输入，天然合法
  textLayer->setFillColor(Red);
  EXPECT_TRUE((textLayer->fillColor() == Red));
}

/**
 * 用例描述: PAGTextLayerTest strokeColor 接口测试
 */
PAG_TEST(PAGTextLayerTest, strokeColor) {
  PAG_SETUP_ISOLATED(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  TestPAGFile->setCurrentTime(5 * 1000000);
  int target = 0;
  auto layer = GetLayer(TestPAGFile, LayerType::Text, target);
  ASSERT_NE(layer, nullptr) << "not get a textLayer" << std::endl;
  auto textLayer = std::static_pointer_cast<pag::PAGTextLayer>(layer);
  auto strokeColor = textLayer->strokeColor();
  EXPECT_TRUE((strokeColor == Red));
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGTextLayerTest/strokeColor"));
  textLayer->setStrokeColor(Blue);
  EXPECT_TRUE((textLayer->strokeColor() == Blue));
}

/**
 * 用例描述: PAGTextLayerTest fontSize 接口测试
 */
PAG_TEST(PAGTextLayerTest, fontSize) {
  PAG_SETUP_ISOLATED(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  TestPAGFile->setCurrentTime(5 * 1000000);
  int target = 0;
  auto layer = GetLayer(TestPAGFile, LayerType::Text, target);
  ASSERT_NE(layer, nullptr) << "not get a textLayer" << std::endl;
  auto textLayer = std::static_pointer_cast<pag::PAGTextLayer>(layer);
  auto fontSize = textLayer->fontSize();
  EXPECT_EQ(fontSize, 60);
  // 异常fontSize
  textLayer->setFontSize(-30);
  TestPAGPlayer->flush();
  textLayer->setFontSize(0);
  TestPAGPlayer->flush();
  // 正常fontSize setter and getter
  textLayer->setFontSize(80);
  EXPECT_EQ(textLayer->fontSize(), 80);
}

/**
 * 用例描述: PAGTextLayerTest text 接口测试
 */
PAG_TEST(PAGTextLayerTest, text) {
  PAG_SETUP_ISOLATED(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  TestPAGFile->setCurrentTime(5 * 1000000);
  int target = 0;
  auto layer = GetLayer(TestPAGFile, LayerType::Text, target);
  ASSERT_NE(layer, nullptr) << "not get a textLayer" << std::endl;
  auto textLayer = std::static_pointer_cast<pag::PAGTextLayer>(layer);
  auto text = textLayer->text();
  EXPECT_EQ(text, "文本图层1");
}

/**
 * 用例描述: PAGTextLayerTest textDocument 接口测试
 */
PAG_TEST(PAGTextLayerTest, textDocument) {
  PAG_SETUP_ISOLATED(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  TestPAGFile->setCurrentTime(5 * 1000000);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGTextLayerTest/textDocument"));
}

/**
 * 用例描述: PAGTextLayerTest多线程修改测试
 */
PAG_TEST(PAGTextLayerTest, multiThreadModify) {
  PAG_SETUP_ISOLATED(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  TestPAGFile->setCurrentTime(5 * 1000000);
  int target = 0;
  auto layer = GetLayer(TestPAGFile, LayerType::Text, target);
  ASSERT_NE(layer, nullptr) << "not get a textLayer" << std::endl;
  auto textLayer = std::static_pointer_cast<pag::PAGTextLayer>(layer);

  // 多线程替换文本
  std::thread thread1([textLayer, TestPAGPlayer]() {
    for (int i = 0; i < 10; ++i) {
      LOGI("线程1 present");
      TestPAGPlayer->flush();
    }
  });

  std::thread thread2([textLayer, TestPAGPlayer]() {
    for (int i = 0; i < 10; ++i) {
      LOGI("线程2 替换文本");
      textLayer->setFontSize(20);
      textLayer->setFillColor(Blue);
      textLayer->setText("线程222替换文本");
      TestPAGPlayer->flush();
    }
  });

  thread1.join();
  thread2.join();
  textLayer->setText("替换文本666666");
  TestPAGPlayer->flush();
}

/**
 * 用例描述: PAGTextLayer Make 功能测试
 */
PAG_TEST(PAGTextLayerTest, Make) {
  auto textLayer = PAGTextLayer::Make(0, "");
  EXPECT_TRUE(textLayer == nullptr);
  textLayer = PAGTextLayer::Make(10, std::shared_ptr<TextDocument>{});
  EXPECT_TRUE(textLayer == nullptr);
  textLayer = PAGTextLayer::Make(6000000, "PAGTextLayerMakeTest", 30);
  EXPECT_TRUE(textLayer != nullptr);
  EXPECT_TRUE(textLayer->getLayer()->transform->position->value.y == 30);
  EXPECT_TRUE(textLayer->duration() == 6000000);
  EXPECT_TRUE(textLayer->text() == "PAGTextLayerMakeTest");
  EXPECT_TRUE(textLayer->fontSize() == 30);
}

/**
 * 用例描述: PAGTextLayer Emoji 功能测试
 */
PAG_TEST(PAGTextLayerTest, Emoji_ID79762747) {
  PAG_SETUP_ISOLATED(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  auto pagFile = LoadPAGFile("assets/zongyi2.pag");
  auto textData = pagFile->getTextData(0);
  // 自动化测试里屏蔽了 mac 端原生的字体库改用 freetype，防止自动化测试过程总是弹出
  // 字体下载的窗口阻塞测试。 但是 freetype 用于测量 mac 端的字体，比如 Emoji
  // 时，会出现位置偏上的情况。但是只影响到 自动化测试， Android 和 iOS
  // 端以及其他平台都是正常的。可以作为已知问题忽略。
  textData->text = "ha ha哈哈\n哈😆哈哈哈";
  pagFile->replaceText(0, textData);
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGTextLayerTest/Emoji"));
}

/**
 * 用例描述: PAGTextLayer Emoji 功能测试
 */
PAG_TEST(PAGTextLayerTest, NormalEmoji) {
  PAG_SETUP_ISOLATED(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  auto pagFile = LoadPAGFile("assets/test2.pag");
  auto textData = pagFile->getTextData(0);
  textData->text = "ha ha哈哈\n哈😆哈哈哈";
  pagFile->replaceText(0, textData);
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGTextLayerTest/NormalEmoji"));
}

/**
 * 用例描述: 测试文字替换
 */
PAG_TEST(PAGTextLayerTest, TextReplacement) {
  PAG_SETUP_ISOLATED(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  auto pagFile = LoadPAGFile("assets/test2.pag");
  auto textData = pagFile->getTextData(0);
  textData->text = "ha ha哈哈\n哈哈哈哈";
  textData->justification = ParagraphJustification::LeftJustify;
  pagFile->replaceText(0, textData);
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGTextLayerTest/TextReplacement"));
}

/**
 * 用例描述: PAGTextLayer 竖排文本 功能测试
 */
PAG_TEST(PAGTextLayerTest, VerticalText_ID80511765) {
  PAG_SETUP_ISOLATED(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  auto pagFile = LoadPAGFile("assets/TextDirection.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGTextLayerTest/textDirection"));
}

/**
 * 用例描述: PAGTextLayer 字间距动画 横排文本 背景框 功能测试
 */
PAG_TEST(PAGTextLayerTest, TrackingAnimator_ID859317799) {
  PAG_SETUP_ISOLATED(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  auto pagFile = LoadPAGFile("assets/TrackingAnimator.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGTextLayerTest/trackingAnimator"));
}

/**
 * 用例描述: PAGTextLayer 字间距动画 竖排文本 功能测试
 */
PAG_TEST(PAGTextLayerTest, TrackingAnimatorVertical_ID859317799) {
  PAG_SETUP_ISOLATED(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  auto pagFile = LoadPAGFile("assets/TrackingAnimatorVertical.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGTextLayerTest/trackingAnimatorVertical"));
}

/**
 * 用例描述: PAGTextLayer 位置动画
 */
PAG_TEST(PAGTextLayerTest, PositionAnimator) {
  PAG_SETUP_ISOLATED(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  auto pagFile = LoadPAGFile("assets/TextPositionAnimator.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGTextLayerTest/PositionAnimator"));
}

/**
 * 用例描述: PAGTextLayer 文本动画
 */
PAG_TEST(PAGTextLayerTest, TextAnimators_ID863204853) {
  PAG_SETUP_ISOLATED(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  auto pagFile = LoadPAGFile("assets/TextAnimators.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGTextLayerTest/textAnimators"));
}

/**
 * 用例描述: PAGTextLayer 文本动画
 */
PAG_TEST(PAGTextLayerTest, TextAnimatorsMode_ID863204817) {
  PAG_SETUP_ISOLATED(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  auto pagFile = LoadPAGFile("assets/TextAnimatorMode.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGTextLayerTest/textAnimatorMode"));

  auto file = pagFile->getFile();
  auto text = file->getTextAt(0);
  bool hasBias = false;
  auto position = TextAnimatorRenderer::GetPositionFromAnimators(
      &text->animators, text->getTextDocument().get(), 0, 0, &hasBias);
  EXPECT_EQ(hasBias, false);
  EXPECT_EQ(position.x, 0.0f);
  EXPECT_EQ(position.y, -180.0f);
  float minAscent = 0.0f;
  float maxDescent = 0.0f;
  pag::CalculateTextAscentAndDescent(text->getTextDocument().get(), &minAscent, &maxDescent);
  EXPECT_LE(fabs(minAscent - (-55.68f)), 0.01f);
  EXPECT_LE(fabs(maxDescent - 13.824f), 0.01f);
}

/**
 * 用例描述: PAGTextLayer 文本动画
 */
PAG_TEST(PAGTextLayerTest, TextAnimatorsX7_ID863204817) {
  PAG_SETUP_ISOLATED(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  auto pagFile = LoadPAGFile("assets/TextAnimatorX7.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGTextLayerTest/textAnimatorX7"));

  auto bytes = Codec::Encode(pagFile->getFile());
  ASSERT_NE(bytes->data(), nullptr);
  ASSERT_GT(static_cast<int>(bytes->length()), 0);
  auto newPagFile = PAGFile::Load(bytes->data(), bytes->length());
  TestPAGPlayer->setComposition(newPagFile);
  TestPAGPlayer->setProgress(0);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGTextLayerTest/textAnimatorX7"));
}

/**
 * 用例描述: PAGTextLayer 文本动画 平滑
 */
PAG_TEST(PAGTextLayerTest, TextAnimatorSmooth_ID863204817) {
  PAG_SETUP_ISOLATED(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  auto pagFile = LoadPAGFile("assets/TextAnimatorSmooth.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGTextLayerTest/textAnimatorSmooth"));
}

/**
 * 用例描述: PAGTextLayer 文本Bounds获取-换行
 */
PAG_TEST(PAGTextLayerTest, TextBounds) {
  auto pagFile = LoadPAGFile("assets/test2.pag");
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  auto pagPlayer = std::make_unique<PAGPlayer>();
  pagPlayer->setComposition(pagFile);
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setProgress(0.5f);
  pagPlayer->flush();
  auto textLayer = std::static_pointer_cast<PAGTextLayer>(
      pagFile->getLayersByEditableIndex(0, LayerType::Text)[0]);
  textLayer->setText(
      "测试文本\n"
      "\n"
      "\n");
  pagPlayer->setProgress(0.5);
  pagPlayer->flush();
  auto bounds = pagPlayer->getBounds(textLayer);
  bounds.round();
  auto defaultBounds = Rect::MakeLTRB(361, 1465, 732, 1794);
  EXPECT_TRUE(bounds == defaultBounds);

  textLayer->setText(
      "测试文本\n"
      "\n"
      "  ");
  pagPlayer->flush();
  bounds = pagPlayer->getBounds(textLayer);
  bounds.round();
  defaultBounds = Rect::MakeLTRB(361, 1465, 732, 1775);
  EXPECT_TRUE(bounds == defaultBounds);

  textLayer->setText(
      "测试文本\n"
      "\n"
      "T\n");
  pagPlayer->flush();
  bounds = pagPlayer->getBounds(textLayer);
  bounds.round();
  EXPECT_TRUE(bounds == defaultBounds);

  textLayer->setText("   ");
  pagPlayer->flush();
  bounds = pagPlayer->getBounds(textLayer);
  bounds.round();
  defaultBounds = Rect::MakeLTRB(475, 1556, 609, 1697);
  EXPECT_TRUE(bounds == defaultBounds);
}

/**
 * 用例描述: PAGTextLayer 文本Bounds获取-含有遮罩
 */
PAG_TEST(PAGTextLayerTest, TrackMatteTextBounds) {
  PAG_SETUP_ISOLATED(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  auto pagFile = LoadPAGFile("assets/text_matte.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0.5f);
  TestPAGPlayer->flush();
  auto defaultBounds = Rect::MakeXYWH(219, 485, 445, 112);
  for (int i = 0; i < pagFile->numTexts(); i++) {
    auto textLayers = pagFile->getLayersByEditableIndex(i, LayerType::Text);
    for (const auto& textLayer : textLayers) {
      auto bounds = TestPAGPlayer->getBounds(textLayer);
      bounds.round();
      EXPECT_EQ(bounds == defaultBounds, true);
    }
  }
}

PAG_TEST(PAGTextLayerTest, SmallFontSizeScale) {
  auto pagSurface = PAGSurface::MakeOffscreen(720, 1080);
  auto pagFile = LoadPAGFile("assets/tougao.pag");
  auto pagPlayer = std::make_unique<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGTextLayerTest/SmallFontSizeScale"));

  pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  pagPlayer->setSurface(pagSurface);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGTextLayerTest/SmallFontSizeScale_LowResolution"));
}

/**
 * 测试点文本的路径绘制
 */
PAG_TEST(PAGTextLayerTest, TextPathCommon) {
  PAG_SETUP_ISOLATED(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  auto pagFile = LoadPAGFile("assets/TextPathCommon.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGTextLayerTest/TextPathCommon"));
}

/**
 * 测试点文本的路径绘制,路径反转功能
 */
PAG_TEST(PAGTextLayerTest, TextPathReversePath) {
  PAG_SETUP_ISOLATED(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  auto pagFile = LoadPAGFile("assets/TextPathReversed.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGTextLayerTest/TextPathReversed"));
}

/**
 * 测试点文本的路径绘制,垂直于路径功能关闭
 */
PAG_TEST(PAGTextLayerTest, TextPathNotPerpendicular) {
  PAG_SETUP_ISOLATED(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  auto pagFile = LoadPAGFile("assets/TextPathNotPerpendicular.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGTextLayerTest/TextPathNotPerpendicular"));
}

/**
 * 测试点文本的路径绘制,强制对齐功能,路径起点大于终点
 */
PAG_TEST(PAGTextLayerTest, TextPathForceAlignment) {
  PAG_SETUP_ISOLATED(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  auto pagFile = LoadPAGFile("assets/TextPathForceAlignment.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGTextLayerTest/TextPathForceAlignment"));
}

/**
 * 测试点文本的路径绘制,强制对齐功能,路径终点大于起点
 */
PAG_TEST(PAGTextLayerTest, TextPathForceAlignment2) {
  PAG_SETUP_ISOLATED(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  auto pagFile = LoadPAGFile("assets/TextPathForceAlignment2.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGTextLayerTest/TextPathForceAlignment2"));
}

/**
 * 测试点文本的路径绘制,闭合路径情况绘制
 */
PAG_TEST(PAGTextLayerTest, TextPathClosedPath) {
  PAG_SETUP_ISOLATED(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  auto pagFile = LoadPAGFile("assets/TextPathClosedPath.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGTextLayerTest/TextPathClosedPath"));
}

/**
 * 测试框文本的路径绘制
 */
PAG_TEST(PAGTextLayerTest, TextPathBox) {
  PAG_SETUP_ISOLATED(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  auto pagFile = LoadPAGFile("assets/TextPathBox.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGTextLayerTest/TextPathBox"));
}

/**
 * 测试框文本的路径绘制
 */
PAG_TEST(PAGTextLayerTest, TextPathBox2) {
  PAG_SETUP_ISOLATED(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  auto pagFile = LoadPAGFile("assets/TextPathBox2.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGTextLayerTest/TextPathBox2"));
}

/**
 * 测试框文本的路径绘制,反转
 */
PAG_TEST(PAGTextLayerTest, TextPathBoxReversed) {
  PAG_SETUP_ISOLATED(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  auto pagFile = LoadPAGFile("assets/TextPathBoxReversed.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGTextLayerTest/TextPathBoxReversed"));
}

/**
 * 测试框文本的范围选择器-三角形,缓和度高低
 */
PAG_TEST(PAGTextLayerTest, TextRangeSelectorTriangleEarseHighAndLow) {
  PAG_SETUP_ISOLATED(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  auto pagFile = LoadPAGFile("assets/RangeSelectorTriangleEaseHighAndLow.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGTextLayerTest/RangeSelectorTriangleHighLow"));
}

/**
 * 测试文本 layer 做缩放动画时，使用 mipmap 的情况
 */
PAG_TEST(PAGTextLayerTest, TextLayerScaleAnimationWithMipmap) {
  PAG_SETUP_ISOLATED(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  auto pagFile = LoadPAGFile("resources/apitest/text_layer_scale_mipmap.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0.5f);
  TestPAGPlayer->flush();
  EXPECT_TRUE(
      Baseline::Compare(TestPAGSurface, "PAGTextLayerTest/TextLayerScaleAnimationWithMipmap"));
}
}  // namespace pag
