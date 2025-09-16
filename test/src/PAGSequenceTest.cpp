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

#include "codec/mp4/MP4BoxHelper.h"
#include "pag/pag.h"
#include "platform/swiftshader/NativePlatform.h"
#include "rendering/caches/RenderCache.h"
#include "utils/TestUtils.h"

namespace pag {

/**
 * 用例描述: 测试直接上屏
 */
PAG_TEST(PAGSequenceTest, RenderOnScreen) {
  auto pagFile = LoadPAGFile("resources/apitest/wz_mvp.pag");
  EXPECT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(750, 1334);
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
 * 用例描述: bitmapSequence关键帧不是全屏的时候要清屏
 */
PAG_TEST(PAGSequenceTest, BitmapSequenceReader) {
  auto pagFile = LoadPAGFile("resources/apitest/ZC_mg_seky2_landscape.pag");
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
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
PAG_TEST(PAGSequenceTest, VideoSequenceAsMask) {
  auto pagFile = LoadPAGFile("resources/apitest/video_sequence_as_mask.pag");
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
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
PAG_TEST(PAGSequenceTest, VideoSequenceToMp4WithHeader) {
  auto pagFile = LoadPAGFile("resources/apitest/video_sequence_with_mp4header.pag");
  ASSERT_NE(pagFile, nullptr);
  ASSERT_EQ(pagFile->layerType(), LayerType::PreCompose);

  const PreComposeLayer* preComposeLayer = static_cast<const PreComposeLayer*>(pagFile->getLayer());
  ASSERT_NE(preComposeLayer->composition, nullptr);
  ASSERT_EQ(preComposeLayer->composition->type(), CompositionType::Video);

  VideoComposition* videoComposition = static_cast<VideoComposition*>(preComposeLayer->composition);
  ASSERT_FALSE(videoComposition->sequences.empty());
  const auto* videoSequence = videoComposition->sequences.at(0);
  auto MP4Data = MP4BoxHelper::CovertToMP4(videoSequence);
  ASSERT_NE(MP4Data, nullptr);

  EXPECT_TRUE(
      Baseline::Compare(std::move(MP4Data), "PAGSequenceTest/VideoSequenceToMP4WithHeader"));
}

/**
 * 用例描述: 不带mp4头的视频序列帧导出为mp4
 */
PAG_TEST(PAGSequenceTest, VideoSequenceToMP4WithoutHeader) {
  auto pagFile = LoadPAGFile("resources/apitest/video_sequence_without_mp4header.pag");
  ASSERT_NE(pagFile, nullptr);
  ASSERT_EQ(pagFile->layerType(), LayerType::PreCompose);

  const PreComposeLayer* preComposeLayer = static_cast<const PreComposeLayer*>(pagFile->getLayer());
  ASSERT_NE(preComposeLayer->composition, nullptr);
  ASSERT_EQ(preComposeLayer->composition->type(), CompositionType::Video);

  VideoComposition* videoComposition = static_cast<VideoComposition*>(preComposeLayer->composition);
  ASSERT_FALSE(videoComposition->sequences.empty());
  const auto* videoSequence = videoComposition->sequences.at(0);
  auto MP4Data = MP4BoxHelper::CovertToMP4(videoSequence);
  ASSERT_NE(MP4Data, nullptr);

  EXPECT_TRUE(
      Baseline::Compare(std::move(MP4Data), "PAGSequenceTest/VideoSequenceToMP4WithoutHeader"));
}
/**
 * 用例描述: 同一个序列帧多图层引用且时间轴交错，测试解码器数量是否正确。
 */
PAG_TEST(PAGSequenceTest, Sequence_Multiple_References) {
  auto pagFile = LoadPAGFile("resources/apitest/sequence_mul_ref.pag");
  EXPECT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setMatrix(Matrix::I());
  auto& sequenceCaches = pagPlayer->renderCache->sequenceCaches;
  pagFile->setCurrentTime(0);
  pagPlayer->flush();
  ASSERT_EQ(static_cast<int>(sequenceCaches.size()), 1);
  EXPECT_EQ(static_cast<int>(sequenceCaches.begin()->second.size()), 1);
  pagFile->setCurrentTime(1000000);
  pagPlayer->flush();
  ASSERT_EQ(static_cast<int>(sequenceCaches.size()), 1);
  EXPECT_EQ(static_cast<int>(sequenceCaches.begin()->second.size()), 2);
  pagFile->setCurrentTime(3000000);
  pagPlayer->flush();
  ASSERT_EQ(static_cast<int>(sequenceCaches.size()), 1);
  EXPECT_EQ(static_cast<int>(sequenceCaches.begin()->second.size()), 3);
  pagFile->setCurrentTime(4500000);
  pagPlayer->flush();
  ASSERT_EQ(static_cast<int>(sequenceCaches.size()), 1);
  EXPECT_EQ(static_cast<int>(sequenceCaches.begin()->second.size()), 3);
  pagFile->setCurrentTime(5000000);
  pagPlayer->flush();
  ASSERT_EQ(static_cast<int>(sequenceCaches.size()), 0);
  pagFile->setCurrentTime(8000000);
  pagPlayer->flush();
  ASSERT_EQ(static_cast<int>(sequenceCaches.size()), 1);
  EXPECT_EQ(static_cast<int>(sequenceCaches.begin()->second.size()), 1);
}

}  // namespace pag
