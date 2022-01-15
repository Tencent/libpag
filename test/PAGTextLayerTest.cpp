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

#include <rendering/renderers/TextAnimatorRenderer.h>
#include <iostream>
#include <thread>
#include "framework/pag_test.h"
#include "framework/utils/PAGTestUtils.h"
#include "nlohmann/json.hpp"
#include "pag/file.h"
#include "rendering/renderers/TextRenderer.h"

namespace pag {
using nlohmann::json;

PAG_TEST_SUIT(PAGTextLayerTest)

/**
 * 用例描述: PAGTextLayerTest fillColor 接口测试
 */
PAG_TEST_F(PAGTextLayerTest, fillColor) {
  ASSERT_NE(TestPAGFile, nullptr);
  TestPAGFile->setCurrentTime(5 * 1000000);
  int target = 0;
  auto layer = GetLayer(TestPAGFile, LayerType::Text, target);
  ASSERT_NE(layer, nullptr) << "not get a textLayer" << std::endl;
  auto textLayer = std::static_pointer_cast<pag::PAGTextLayer>(layer);
  auto fillColor = textLayer->fillColor();
  EXPECT_TRUE(fillColor == Blue);
  TestPAGPlayer->flush();
  auto fillColorMd5 = getMd5FromSnap();
  ASSERT_NE(fillColorMd5, "");
  // 色值限定uint_8输入，天然合法
  textLayer->setFillColor(Red);
  EXPECT_TRUE((textLayer->fillColor() == Red));
  PAGTestEnvironment::DumpJson["PAGTextLayerTester"]["TextLayer"]["fillColorMd5"] = fillColorMd5;
#ifdef COMPARE_JSON_PATH
  auto cFillColor = PAGTestEnvironment::CompareJson["PAGTextLayerTester"]["TextLayer"]["fillColorMd5"];
  if (cFillColor != nullptr) {
    EXPECT_EQ(cFillColor.get<std::string>(), fillColorMd5);
  }
#endif
}

/**
 * 用例描述: PAGTextLayerTest strokeColor 接口测试
 */
PAG_TEST_F(PAGTextLayerTest, strokeColor) {
  ASSERT_NE(TestPAGFile, nullptr);
  TestPAGFile->setCurrentTime(5 * 1000000);
  int target = 0;
  auto layer = GetLayer(TestPAGFile, LayerType::Text, target);
  ASSERT_NE(layer, nullptr) << "not get a textLayer" << std::endl;
  auto textLayer = std::static_pointer_cast<pag::PAGTextLayer>(layer);
  auto strokeColor = textLayer->strokeColor();
  EXPECT_TRUE((strokeColor == Red));
  TestPAGPlayer->flush();
  auto strokeColorMd5 = getMd5FromSnap();
  ASSERT_NE(strokeColorMd5, "");
  textLayer->setStrokeColor(Blue);
  EXPECT_TRUE((textLayer->strokeColor() == Blue));
  PAGTestEnvironment::DumpJson["PAGTextLayerTester"]["TextLayer"]["strokeColorMd5"] =
      strokeColorMd5;
#ifdef COMPARE_JSON_PATH
  auto cStrokeColor = PAGTestEnvironment::CompareJson["PAGTextLayerTester"]["TextLayer"]["strokeColorMd5"];
  if (cStrokeColor != nullptr) {
    EXPECT_EQ(cStrokeColor.get<std::string>(), strokeColorMd5);
  }
#endif
}

/**
 * 用例描述: PAGTextLayerTest fontSize 接口测试
 */
PAG_TEST_F(PAGTextLayerTest, fontSize) {
  ASSERT_NE(TestPAGFile, nullptr);
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
PAG_TEST_F(PAGTextLayerTest, text) {
  ASSERT_NE(TestPAGFile, nullptr);
  TestPAGFile->setCurrentTime(5 * 1000000);
  int target = 0;
  auto layer = GetLayer(TestPAGFile, LayerType::Text, target);
  ASSERT_NE(layer, nullptr) << "not get a textLayer" << std::endl;
  auto textLayer = std::static_pointer_cast<pag::PAGTextLayer>(layer);
  auto text = textLayer->text();
  EXPECT_EQ(text, "文本图层1");
}

/**
 * 用例描述: PAGTextLayerTest textDocumentMd5 接口测试
 */
PAG_TEST_F(PAGTextLayerTest, textDocumentMd5) {
  ASSERT_NE(TestPAGFile, nullptr);
  TestPAGFile->setCurrentTime(5 * 1000000);
  TestPAGPlayer->flush();
  auto textDocumentMd5 = getMd5FromSnap();
  ASSERT_NE(textDocumentMd5, "");
  PAGTestEnvironment::DumpJson["PAGTextLayerTester"]["TextLayer"]["textDocumentMd5"] =
      textDocumentMd5;
#ifdef COMPARE_JSON_PATH
  auto cText = PAGTestEnvironment::CompareJson["PAGTextLayerTester"]["TextLayer"]["textDocumentMd5"];
  if (cText != nullptr) {
    EXPECT_EQ(cText.get<std::string>(), textDocumentMd5);
  }
#endif
}

/**
 * 用例描述: PAGTextLayerTest多线程修改测试
 */
PAG_TEST_F(PAGTextLayerTest, multiThreadModify) {
  ASSERT_NE(TestPAGFile, nullptr);
  TestPAGFile->setCurrentTime(5 * 1000000);
  int target = 0;
  auto layer = GetLayer(TestPAGFile, LayerType::Text, target);
  ASSERT_NE(layer, nullptr) << "not get a textLayer" << std::endl;
  auto textLayer = std::static_pointer_cast<pag::PAGTextLayer>(layer);

  // 多线程替换文本
  std::thread thread1([textLayer]() {
    for (int i = 0; i < 10; ++i) {
      std::cout << "线程1 present" << std::endl;
      TestPAGPlayer->flush();
    }
  });

  std::thread thread2([textLayer]() {
    for (int i = 0; i < 10; ++i) {
      std::cout << "线程2替换文本" << std::endl;
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
PAG_TEST_F(PAGTextLayerTest, Make) {
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
PAG_TEST_F(PAGTextLayerTest, Emoji_ID79762747) {
  auto pagFile = PAGFile::Load("../assets/zongyi2.pag");
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
  auto md5 = getMd5FromSnap();
  PAGTestEnvironment::DumpJson["PAGTextLayerTester"]["emoji"] = md5;
#ifdef COMPARE_JSON_PATH
  auto emojiMd5 = PAGTestEnvironment::CompareJson["PAGTextLayerTester"]["emoji"];
  if (emojiMd5 != nullptr) {
    TraceIf(TestPAGSurface, "../test/out/pag_textlayer_emoji.png",
            emojiMd5.get<std::string>() != md5);
    EXPECT_EQ(emojiMd5.get<std::string>(), md5);
  }
#endif
}

/**
 * 用例描述: PAGTextLayer Emoji 功能测试
 */
PAG_TEST_F(PAGTextLayerTest, NormalEmoji_ID79762751) {
  auto pagFile = PAGFile::Load("../assets/test2.pag");
  auto textData = pagFile->getTextData(0);
  textData->text = "ha ha哈哈\n哈😆哈哈哈";
  pagFile->replaceText(0, textData);
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0);
  TestPAGPlayer->flush();
  auto md5 = getMd5FromSnap();
  PAGTestEnvironment::DumpJson["PAGTextLayerTester"]["normalEmoji"] = md5;
#ifdef COMPARE_JSON_PATH
  auto emojiMd5 = PAGTestEnvironment::CompareJson["PAGTextLayerTester"]["normalEmoji"];
  if (emojiMd5 != nullptr) {
    TraceIf(TestPAGSurface, "../test/out/pag_textlayer_normalEmoji.png",
            emojiMd5.get<std::string>() != md5);
    EXPECT_EQ(emojiMd5.get<std::string>(), md5);
  }
#endif
}

/**
 * 用例描述: 测试文字替换
 */
PAG_TEST_F(PAGTextLayerTest, text_ID83312235) {
  auto pagFile = PAGFile::Load("../assets/test2.pag");
  auto textData = pagFile->getTextData(0);
  textData->text = "ha ha哈哈\n哈哈哈哈";
  textData->justification = ParagraphJustification::LeftJustify;
  pagFile->replaceText(0, textData);
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0);
  TestPAGPlayer->flush();
  auto md5 = getMd5FromSnap();
  PAGTestEnvironment::DumpJson["PAGTextLayerTester"]["text_ID83312235"] = md5;
#ifdef COMPARE_JSON_PATH
  auto emojiMd5 = PAGTestEnvironment::CompareJson["PAGTextLayerTester"]["text_ID83312235"];
  if (emojiMd5 != nullptr) {
    TraceIf(TestPAGSurface, "../test/out/text_ID83312235.png", emojiMd5.get<std::string>() != md5);
    EXPECT_EQ(emojiMd5.get<std::string>(), md5);
  }
#endif
}

/**
 * 用例描述: PAGTextLayer 竖排文本 功能测试
 */
PAG_TEST_F(PAGTextLayerTest, VerticalText_ID80511765) {
  auto pagFile = PAGFile::Load("../assets/TextDirection.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0);
  TestPAGPlayer->flush();
  auto md5 = getMd5FromSnap();
  PAGTestEnvironment::DumpJson["PAGTextLayerTester"]["textDirection"] = md5;
#ifdef COMPARE_JSON_PATH
  auto textMd5 = PAGTestEnvironment::CompareJson["PAGTextLayerTester"]["textDirection"];
  if (textMd5 != nullptr) {
    TraceIf(TestPAGSurface, "../test/out/pag_textlayer_textDirection.png",
            textMd5.get<std::string>() != md5);
    EXPECT_EQ(textMd5.get<std::string>(), md5);
  }
#endif
}

/**
 * 用例描述: PAGTextLayer 字间距动画 横排文本 背景框 功能测试
 */
PAG_TEST_F(PAGTextLayerTest, TrackingAnimator_ID859317799) {
  auto pagFile = PAGFile::Load("../assets/TrackingAnimator.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0);
  TestPAGPlayer->flush();
  auto md5 = getMd5FromSnap();
  PAGTestEnvironment::DumpJson["PAGTextLayerTester"]["trackingAnimator"] = md5;
#ifdef COMPARE_JSON_PATH
  auto textMd5 = PAGTestEnvironment::CompareJson["PAGTextLayerTester"]["trackingAnimator"];
  if (textMd5 != nullptr) {
    TraceIf(TestPAGSurface, "../test/out/pag_textlayer_trackingAnimator.png",
            textMd5.get<std::string>() != md5);
    EXPECT_EQ(textMd5.get<std::string>(), md5);
  }
#endif
}

/**
 * 用例描述: PAGTextLayer 字间距动画 竖排文本 功能测试
 */
PAG_TEST_F(PAGTextLayerTest, TrackingAnimatorVertical_ID859317799) {
  auto pagFile = PAGFile::Load("../assets/TrackingAnimatorVertical.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0);
  TestPAGPlayer->flush();
  auto md5 = getMd5FromSnap();
  PAGTestEnvironment::DumpJson["PAGTextLayerTester"]["trackingAnimatorVertical"] = md5;
#ifdef COMPARE_JSON_PATH
  auto textMd5 = PAGTestEnvironment::CompareJson["PAGTextLayerTester"]["trackingAnimatorVertical"];
  if (textMd5 != nullptr) {
    TraceIf(TestPAGSurface, "../test/out/pag_textlayer_trackingAnimatorVertical.png",
            textMd5.get<std::string>() != md5);
    EXPECT_EQ(textMd5.get<std::string>(), md5);
  }
#endif
}

/**
 * 用例描述: PAGTextLayer 位置动画
 */
PAG_TEST_F(PAGTextLayerTest, PositionAnimator_ID859317799) {
  auto pagFile = PAGFile::Load("../assets/TextPositionAnimator.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0);
  TestPAGPlayer->flush();
  auto md5 = getMd5FromSnap();
  PAGTestEnvironment::DumpJson["PAGTextLayerTester"]["positionAnimator"] = md5;
#ifdef COMPARE_JSON_PATH
  auto textMd5 = PAGTestEnvironment::CompareJson["PAGTextLayerTester"]["positionAnimator"];
  if (textMd5 != nullptr) {
    TraceIf(TestPAGSurface, "../test/out/pag_textlayer_positionAnimator.png",
            textMd5.get<std::string>() != md5);
    EXPECT_EQ(textMd5.get<std::string>(), md5);
  }
#endif
}

/**
 * 用例描述: PAGTextLayer 文本动画
 */
PAG_TEST_F(PAGTextLayerTest, TextAnimators_ID863204853) {
  auto pagFile = PAGFile::Load("../assets/TextAnimators.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0);
  TestPAGPlayer->flush();
  auto md5 = getMd5FromSnap();
  PAGTestEnvironment::DumpJson["PAGTextLayerTester"]["textAnimators"] = md5;
#ifdef COMPARE_JSON_PATH
  auto textMd5 = PAGTestEnvironment::CompareJson["PAGTextLayerTester"]["textAnimators"];
  if (textMd5 != nullptr) {
    TraceIf(TestPAGSurface, "../test/out/pag_textlayer_textAnimators.png",
            textMd5.get<std::string>() != md5);
    EXPECT_EQ(textMd5.get<std::string>(), md5);
  }
#endif
}

/**
 * 用例描述: PAGTextLayer 文本动画
 */
PAG_TEST_F(PAGTextLayerTest, TextAnimatorsMode_ID863204817) {
  auto pagFile = PAGFile::Load("../assets/TextAnimatorMode.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0);
  TestPAGPlayer->flush();
  auto md5 = getMd5FromSnap();
  PAGTestEnvironment::DumpJson["PAGTextLayerTester"]["textAnimatorMode"] = md5;
#ifdef COMPARE_JSON_PATH
  auto textMd5 = PAGTestEnvironment::CompareJson["PAGTextLayerTester"]["textAnimatorMode"];
  TraceIf(TestPAGSurface, "../test/out/textAnimatorMode.png", textMd5.get<std::string>() != md5);
  EXPECT_EQ(textMd5.get<std::string>(), md5);
#endif
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
  pag::CalculateTextAscentAndDescent(text->getTextDocument(), &minAscent, &maxDescent);
  EXPECT_LE(fabs(minAscent - (-55.68f)), 0.01f);
  EXPECT_LE(fabs(maxDescent - 13.824f), 0.01f);
}

/**
 * 用例描述: PAGTextLayer 文本动画
 */
PAG_TEST_F(PAGTextLayerTest, TextAnimatorsX7_ID863204817) {
  auto pagFile = PAGFile::Load("../assets/TextAnimatorX7.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0);
  TestPAGPlayer->flush();
  auto md5 = getMd5FromSnap();
  PAGTestEnvironment::DumpJson["PAGTextLayerTester"]["textAnimatorX7"] = md5;
#ifdef COMPARE_JSON_PATH
  auto textMd5 = PAGTestEnvironment::CompareJson["PAGTextLayerTester"]["textAnimatorX7"];
  EXPECT_EQ(textMd5.get<std::string>(), md5);
#endif
  auto bytes = Codec::Encode(pagFile->getFile());
  ASSERT_NE(bytes->data(), nullptr);
  ASSERT_GT(static_cast<int>(bytes->length()), 0);
  auto newPagFile = PAGFile::Load(bytes->data(), bytes->length());
  TestPAGPlayer->setComposition(newPagFile);
  TestPAGPlayer->setProgress(0);
  TestPAGPlayer->flush();
  auto newMd5 = getMd5FromSnap();
  EXPECT_EQ(newMd5, md5);
}

/**
 * 用例描述: PAGTextLayer 文本动画 平滑
 */
PAG_TEST_F(PAGTextLayerTest, TextAnimatorSmooth_ID863204817) {
  auto pagFile = PAGFile::Load("../assets/TextAnimatorSmooth.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0);
  TestPAGPlayer->flush();
  auto md5 = getMd5FromSnap();
  PAGTestEnvironment::DumpJson["PAGTextLayerTester"]["textAnimatorSmooth"] = md5;
#ifdef COMPARE_JSON_PATH
  auto textMd5 = PAGTestEnvironment::CompareJson["PAGTextLayerTester"]["textAnimatorSmooth"];
  TraceIf(TestPAGSurface, "../test/out/textAnimatorSmooth.png", textMd5.get<std::string>() != md5);
  EXPECT_EQ(textMd5.get<std::string>(), md5);
#endif
}

/**
 * 用例描述: PAGTextLayer 文本Bounds获取-换行
 */
PAG_TEST_F(PAGTextLayerTest, TextBounds) {
  auto pagFile = PAGFile::Load("../assets/test2.pag");
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  auto pagPlayer = new PAGPlayer();
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
  auto defaultBounds = Rect::MakeXYWH(361, 1465, 371, 329);
  EXPECT_TRUE(bounds == defaultBounds);

  textLayer->setText(
      "测试文本\n"
      "\n"
      "  ");
  pagPlayer->flush();
  bounds = pagPlayer->getBounds(textLayer);
  bounds.round();
  defaultBounds = Rect::MakeXYWH(361, 1465, 371, 310);
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
  defaultBounds = Rect::MakeXYWH(475, 1556, 134, 141);
  EXPECT_TRUE(bounds == defaultBounds);

  delete pagPlayer;
}

/**
 * 用例描述: PAGTextLayer 文本Bounds获取-含有遮罩
 */
PAG_TEST_F(PAGTextLayerTest, TrackMatteTextBounds) {
  auto pagFile = PAGFile::Load("../assets/text_matte.pag");
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0.5f);
  TestPAGPlayer->flush();
  auto defaultBounds = Rect::MakeXYWH(219, 484, 445, 112);
  for (int i = 0; i < pagFile->numTexts(); i++) {
    auto textLayers = pagFile->getLayersByEditableIndex(i, LayerType::Text);
    for (const auto& textLayer : textLayers) {
      auto bounds = TestPAGPlayer->getBounds(textLayer);
      bounds.round();
      EXPECT_EQ(bounds == defaultBounds, true);
    }
  }
}

}  // namespace pag
