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
#include "pag/pag.h"
#include "platform/swiftshader/NativePlatform.h"
#include "rendering/caches/RenderCache.h"

namespace pag {

PAG_TEST_SUIT(PAGSequenceTest)
void pagSequenceTest() {
  auto pagFile = PAGFile::Load("../resources/apitest/wz_mvp.pag");
  EXPECT_NE(pagFile, nullptr);
  auto pagSurface = PAGSurface::MakeOffscreen(750, 1334);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setMatrix(Matrix::I());
  pagPlayer->setProgress(0.5);
  pagPlayer->flush();
  EXPECT_EQ(static_cast<int>(pagPlayer->renderCache->sequenceCaches.size()), 1);
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGSequenceTest/pagSequenceTest"));
}

/**
 * 用例描述: 测试直接上屏
 */
PAG_TEST_F(PAGSequenceTest, RenderOnScreen) {
  pagSequenceTest();
}

/**
 * 用例描述: bitmapSequence关键帧不是全屏的时候要清屏
 */
PAG_TEST_F(PAGSequenceTest, BitmapSequenceReader) {
  auto pagFile = PAGFile::Load("../resources/apitest/ZC_mg_seky2_landscape.pag");
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setProgress(0.5);
  pagPlayer->flush();
  pagPlayer->setProgress(0.75);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGSequenceTest/BitmapSequenceReader"));
}

/**
 * 用例描述: 视频序列帧作为遮罩
 */
PAG_TEST_F(PAGSequenceTest, VideoSequenceAsMask) {
  auto pagFile = PAGFile::Load("../resources/apitest/video_sequence_as_mask.pag");
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setProgress(0.2);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGSequenceTest/VideoSequenceAsMask"));
}

/**
 * 用例描述: 带mp4头的视频序列帧导出为mp4
 */
PAG_TEST_F(PAGSequenceTest, VideoSequenceToMp4WithHeader) {
  auto pagFile = PAGFile::Load("../resources/apitest/video_sequence_with_mp4header.pag");
  ASSERT_NE(pagFile, nullptr);
  ASSERT_EQ(pagFile->layerType(), LayerType::PreCompose);

  const PreComposeLayer* preComposeLayer = static_cast<const PreComposeLayer*>(pagFile->getLayer());
  ASSERT_NE(preComposeLayer->composition, nullptr);
  ASSERT_EQ(preComposeLayer->composition->type(), CompositionType::Video);

  VideoComposition* videoComposition = static_cast<VideoComposition*>(preComposeLayer->composition);
  ASSERT_FALSE(videoComposition->sequences.empty());
  auto videoSequence = videoComposition->sequences.at(0);
  auto mp4Data = videoSequence->getMp4Data();
  ASSERT_NE(mp4Data, nullptr);

  EXPECT_TRUE(
      Baseline::Compare(std::move(mp4Data), "PAGSequenceTest/VideoSequenceToMp4WithHeader"));
}

/**
 * 用例描述: 不带mp4头的视频序列帧导出为mp4
 */
PAG_TEST_F(PAGSequenceTest, VideoSequenceToMp4WithoutHeader) {
  auto pagFile = PAGFile::Load("../resources/apitest/video_sequence_without_mp4header.pag");
  ASSERT_NE(pagFile, nullptr);
  ASSERT_EQ(pagFile->layerType(), LayerType::PreCompose);

  const PreComposeLayer* preComposeLayer = static_cast<const PreComposeLayer*>(pagFile->getLayer());
  ASSERT_NE(preComposeLayer->composition, nullptr);
  ASSERT_EQ(preComposeLayer->composition->type(), CompositionType::Video);

  VideoComposition* videoComposition = static_cast<VideoComposition*>(preComposeLayer->composition);
  ASSERT_FALSE(videoComposition->sequences.empty());
  auto videoSequence = videoComposition->sequences.at(0);
  auto mp4Data = videoSequence->getMp4Data();
  ASSERT_NE(mp4Data, nullptr);

  EXPECT_TRUE(
      Baseline::Compare(std::move(mp4Data), "PAGSequenceTest/VideoSequenceToMp4WithoutHeader"));
}

}  // namespace pag
