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
#include "nlohmann/json.hpp"

namespace pag {
using nlohmann::json;

PAG_TEST_CASE(PAGPlayerTest)

/**
 * 用例描述: PAGPlayer setComposition基础功能
 */
PAG_TEST_F(PAGPlayerTest, setComposition) {
  TestPAGPlayer->setComposition(nullptr);
  ASSERT_EQ(TestPAGPlayer->getComposition(), nullptr);
  auto container = PAGComposition::Make(720, 1080);
  TestPAGPlayer->setComposition(container);
  ASSERT_NE(TestPAGPlayer->getComposition(), nullptr);
}

/**
 * 用例描述: PAGPlayerTest基础功能
 */
PAG_TEST_F(PAGPlayerTest, pagPlayer) {
  auto container = PAGComposition::Make(720, 1080);
  TestPAGPlayer->setComposition(container);
  ASSERT_NE(TestPAGPlayer->getComposition(), nullptr);
  auto rootLayer = PAGFile::Load(DEFAULT_PAG_PATH);
  auto pagCom = std::static_pointer_cast<PAGComposition>(rootLayer->getLayerAt(0));
  int size = pagCom->numChildren();
  for (int i = 0; i < size; i++) {
    auto layer = pagCom->getLayerAt(0);
    layer->setCurrentTime(3 * 1000000);
    container->addLayer(pagCom->getLayerAt(0));
  }

  ASSERT_EQ(TestPAGSurface->width(), 720);
  ASSERT_EQ(TestPAGSurface->height(), 1080);
  ASSERT_EQ(container->numChildren(), 6);

  auto pagFile2 = PAGFile::Load(DEFAULT_PAG_PATH);
  auto pagComposition2 = std::static_pointer_cast<PAGComposition>(pagFile2->getLayerAt(0));
  TestPAGPlayer->setComposition(pagComposition2);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGPlayerTest/pagPlayer_setComposition"));
  TestPAGPlayer->setComposition(container);
  TestPAGPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGPlayerTest/pagPlayer_setComposition2"));
}

/**
 * 用例描述: PAGPlayerTest 切换PAGSurface
 */
PAG_TEST_F(PAGPlayerTest, switchPAGSurface) {
  auto pagFile1 = PAGFile::Load("../resources/apitest/test.pag");
  auto pagSurface1 = PAGSurface::MakeOffscreen(pagFile1->width(), pagFile1->height());
  auto pagPlayer1 = new PAGPlayer();
  pagPlayer1->setSurface(pagSurface1);
  pagPlayer1->setComposition(pagFile1);
  pagPlayer1->setProgress(0);
  pagPlayer1->flush();
  pagPlayer1->setSurface(nullptr);

  auto pagPlayer2 = new PAGPlayer();
  auto pagFile2 = PAGFile::Load("../resources/apitest/ZC2.pag");
  pagPlayer2->setComposition(pagFile2);
  pagPlayer2->setSurface(pagSurface1);
  pagPlayer2->setProgress(0.5f);
  auto status = pagPlayer2->flush();
  ASSERT_EQ(status, true);

  delete pagPlayer1;
  delete pagPlayer2;
  EXPECT_TRUE(Baseline::Compare(pagSurface1, "PAGPlayerTest/switchPAGSurface"));
}

/**
 * 用例描述: PAGPlayerTest基础功能
 */
PAG_TEST_F(PAGPlayerTest, autoClear) {
  auto pagFile = PAGFile::Load("../resources/apitest/AlphaTrackMatte.pag");
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  auto pagPlayer = new PAGPlayer();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->flush();

  auto pagFile2 = PAGFile::Load("../resources/gradient/grad_alpha.pag");
  pagPlayer->setComposition(pagFile2);
  pagPlayer->setAutoClear(false);
  pagPlayer->flush();

  auto pixelBuffer = PixelBuffer::Make(pagSurface->width(), pagSurface->height());
  Bitmap bitmap(pixelBuffer);
  ASSERT_FALSE(bitmap.isEmpty());
  auto result = pagSurface->readPixels(bitmap.colorType(), bitmap.alphaType(),
                                       bitmap.writablePixels(), bitmap.rowBytes());
  ASSERT_TRUE(result);
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGPlayerTest/autoClear_autoClear_false_flush0"));

  pagPlayer->flush();
  result = pagSurface->readPixels(bitmap.colorType(), bitmap.alphaType(), bitmap.writablePixels(),
                                  bitmap.rowBytes());
  ASSERT_TRUE(result);
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGPlayerTest/autoClear_autoClear_false_flush1"));

  pagPlayer->setAutoClear(true);
  pagPlayer->flush();
  result = pagSurface->readPixels(bitmap.colorType(), bitmap.alphaType(), bitmap.writablePixels(),
                                  bitmap.rowBytes());
  ASSERT_TRUE(result);
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGPlayerTest/autoClear_autoClear_true"));
}

}  // namespace pag