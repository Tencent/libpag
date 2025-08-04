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

#include <iostream>
#include <thread>
#include "base/utils/Log.h"
#include "nlohmann/json.hpp"
#include "utils/TestUtils.h"

namespace pag {
using nlohmann::json;

/**
 * 用例描述: PAGImageLayer 图层基础边缘 Case 测试
 */
PAG_TEST(PAGImageLayerTest, imageBasetTest) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  int target = 0;
  std::shared_ptr<PAGImageLayer> imageLayer =
      std::static_pointer_cast<PAGImageLayer>(GetLayer(TestPAGFile, LayerType::Image, target));
  ASSERT_NE(imageLayer, nullptr);
  auto file = MakePAGImage("resources/font/NotoColorEmoji.ttf");
  // 非法文件都无法被decode成PAGImage，file是nullptr
  EXPECT_TRUE(file == nullptr);
  auto pag = MakePAGImage("resources/apitest/test.pag");
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
PAG_TEST(PAGImageLayerTest, imageMultiThreadReplace) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  int target = 0;
  std::shared_ptr<PAGImageLayer> imageLayer =
      std::static_pointer_cast<PAGImageLayer>(GetLayer(TestPAGFile, LayerType::Image, target));
  ASSERT_NE(imageLayer, nullptr);
  ASSERT_EQ(imageLayer->editableIndex(), 1);
  ASSERT_EQ(imageLayer->contentDuration(), 2 * 1000000);
  auto image = MakePAGImage("resources/apitest/imageReplacement.png");

  TestPAGFile->setCurrentTime(3000000);
  TestPAGPlayer->flush();
  // 多线程同时替换图片
  std::thread thread1([imageLayer, image, TestPAGPlayer]() {
    for (int i = 0; i < 10; ++i) {
      LOGI("线程1 present");
      TestPAGPlayer->flush();
    }
  });
  std::thread thread2([imageLayer, image, TestPAGPlayer]() {
    for (int i = 0; i < 10; ++i) {
      LOGI("线程2 替换图片");
      imageLayer->replaceImage(image);
      TestPAGPlayer->flush();
    }
  });
  std::thread thread3([imageLayer, image, TestPAGFile, TestPAGPlayer]() {
    for (int i = 0; i < 10; ++i) {
      LOGI("线程3 通过图层名字替换图片");
      TestPAGFile->replaceImageByName("PAGImageLayer1", image);
      TestPAGPlayer->flush();
    }
  });
  thread1.join();
  thread2.join();
  thread3.join();

  imageLayer->replaceImage(image);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGImageLayerTest/ImageReplacement"));
}

/**
 * 用例描述: PAGImageLayerContentDuration获取测试
 */
PAG_TEST(PAGImageLayerTest, imageLayerContentDuration) {
  auto testFile = LoadPAGFile("resources/apitest/test_TimeRemapInFileRange.pag");
  // 图层 timeremap 没有超过文件显示区域
  int commonTarget = 4;
  std::shared_ptr<PAGImageLayer> imageLayer =
      std::static_pointer_cast<PAGImageLayer>(GetLayer(testFile, LayerType::Image, commonTarget));
  ASSERT_NE(imageLayer, nullptr);
  ASSERT_EQ(imageLayer->contentDuration(), 1160000);

  testFile = LoadPAGFile("resources/apitest/test_TimeRemapBeyondFileRange.pag");
  // 图层 timeremap 都超过了文件显示区域
  imageLayer =
      std::static_pointer_cast<PAGImageLayer>(GetLayer(testFile, LayerType::Image, commonTarget));
  ASSERT_NE(imageLayer, nullptr);
  ASSERT_EQ(imageLayer->contentDuration(), 240000);

  // 变速timeremap测试用例
  testFile = LoadPAGFile("resources/apitest/timeRemap_shift.pag");
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
PAG_TEST(PAGImageLayerTest, getBoundsTest) {
  auto pagFile = LoadPAGFile("resources/apitest/ImageLayerBounds.pag");
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
PAG_TEST(PAGImageLayerTest, setImage) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  auto pagFile = LoadPAGFile("resources/filter/fastblur.pag");
  auto image = MakePAGImage("resources/apitest/imageReplacement.png");
  auto pagImageLayer = pagFile->getLayersByEditableIndex(0, pag::LayerType::Image)[0];
  std::static_pointer_cast<PAGImageLayer>(pagImageLayer)->setImage(image);
  TestPAGPlayer->setComposition(pagFile);
  TestPAGPlayer->setProgress(0.3);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGImageLayerTest/setImage"));
}

/**
 * 用例描述: 测试当 ImageLayer 被裁剪，bounds 有小数时离屏触发重采样导致输出的图片模糊。
 * 对比 image1 和 image2 中间内容基本没有变化。
 */
PAG_TEST(PAGImageLayerTest, mask) {
  auto pagFile = LoadPAGFile("resources/apitest/wumengban.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->flush();
  tgfx::Bitmap bitmap(pagFile->width(), pagFile->height(), false, false);
  tgfx::Pixmap pixmap1(bitmap);
  ASSERT_FALSE(pixmap1.isEmpty());
  auto result = pagSurface->readPixels(ToPAG(pixmap1.colorType()), ToPAG(pixmap1.alphaType()),
                                       pixmap1.writablePixels(), pixmap1.rowBytes());
  ASSERT_TRUE(result);

  pagFile = LoadPAGFile("resources/apitest/mengban.pag");
  ASSERT_NE(pagFile, nullptr);
  pagPlayer->setComposition(pagFile);
  pagPlayer->flush();
  tgfx::Bitmap bitmap2(pagFile->width(), pagFile->height(), false, false);
  tgfx::Pixmap pixmap2(bitmap2);
  ASSERT_FALSE(pixmap2.isEmpty());
  result = pagSurface->readPixels(ToPAG(pixmap2.colorType()), ToPAG(pixmap2.alphaType()),
                                  pixmap2.writablePixels(), pixmap2.rowBytes());
  ASSERT_TRUE(result);

  int left = 130;
  int top = 400;
  int width = 500;
  int height = 700;
  tgfx::Bitmap imageBitmap1(width, height, false, false);
  tgfx::Pixmap image1(imageBitmap1);
  pixmap1.readPixels(image1.info(), image1.writablePixels(), left, top);
  tgfx::Bitmap imageBitmap2(width, height, false, false);
  tgfx::Pixmap image2(imageBitmap2);
  pixmap2.readPixels(image2.info(), image2.writablePixels(), left, top);

  auto pixels1 = reinterpret_cast<const uint8_t*>(image1.pixels());
  auto pixels2 = reinterpret_cast<const uint8_t*>(image2.pixels());
  auto byteSize = image1.byteSize();
  for (size_t index = 0; index < byteSize; index++) {
    auto pixelA = pixels2[index];
    auto pixelB = pixels1[index];
    if (abs(pixelA - pixelB) > 1) {
      result = false;
      break;
    }
  }
  if (result) {
    RemoveImage("PAGImageLayerTest/bitmap1");
    RemoveImage("PAGImageLayerTest/bitmap2");
    RemoveImage("PAGImageLayerTest/image1");
    RemoveImage("PAGImageLayerTest/image2");
  } else {
    SaveImage(pixmap1, "PAGImageLayerTest/bitmap1");
    SaveImage(pixmap2, "PAGImageLayerTest/bitmap2");
    SaveImage(image1, "PAGImageLayerTest/image1");
    SaveImage(image2, "PAGImageLayerTest/image2");
  }
  EXPECT_TRUE(result);
}

/**
 * 用例描述: 测试TrackMatteImageLayer的getVisibleRange是否正确。
 */
PAG_TEST(PAGImageLayerTest, trackmatteImageLayer) {
  auto pagFile = LoadPAGFile("resources/apitest/TrackmatteImageLayer.pag");
  ASSERT_NE(pagFile, nullptr);
  auto imageLayer = std::static_pointer_cast<PAGImageLayer>(
      pagFile->getLayersByEditableIndex(0, LayerType::Image).front());
  auto timeRange = imageLayer->getVisibleRangeInFile();
  EXPECT_EQ(timeRange.start, -13);
  EXPECT_EQ(timeRange.end, 66);
}
}  // namespace pag
