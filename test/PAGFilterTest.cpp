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

#include <fstream>
#include "framework/pag_test.h"
#include "framework/utils/PAGTestUtils.h"
#include "nlohmann/json.hpp"

namespace pag {
using nlohmann::json;

PAG_TEST_CASE(PAGFilterTest)

/**
 * 用例描述: CornerPin用例
 */
PAG_TEST(PAGFilterTest, CornerPin) {
  auto pagFile = PAGFile::Load(TestConstants::RESOURCES_ROOT + "filter/cornerpin.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);

  pagFile->setCurrentTime(1000000);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/CornerPin"));
}

/**
 * 用例描述: Bulge效果测试
 */
PAG_TEST(PAGFilterTest, Bulge) {
  auto pagFile = PAGFile::Load(TestConstants::RESOURCES_ROOT + "filter/bulge.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);

  pagFile->setCurrentTime(300000);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/Bulge"));
}

/**
 * 用例描述: MotionTile效果测试
 */
PAG_TEST(PAGFilterTest, MotionTile) {
  auto pagFile = PAGFile::Load(TestConstants::RESOURCES_ROOT + "filter/motiontile.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);

  pagFile->setCurrentTime(1000000);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/MotionTile"));
}

/**
 * 用例描述: MotionBlur效果测试
 */
PAG_TEST(PAGFilterTest, MotionBlur) {
  auto pagFile = PAGFile::Load(TestConstants::RESOURCES_ROOT + "filter/MotionBlur.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);

  pagFile->setCurrentTime(200000);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/MotionBlur_200000"));

  pagFile->setCurrentTime(600000);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/MotionBlur_600000"));
}

/**
 * 用例描述: GaussBlur效果测试
 */
PAG_TEST(PAGFilterTest, GaussBlur) {
  auto pagFile = PAGFile::Load(TestConstants::RESOURCES_ROOT + "filter/fastblur.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);

  pagFile->setCurrentTime(1000000);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/GaussBlur_FastBlur"));

  pagFile = PAGFile::Load(TestConstants::RESOURCES_ROOT + "filter/fastblur_norepeat.pag");
  ASSERT_NE(pagFile, nullptr);
  pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);

  pagFile->setCurrentTime(1000000);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/GaussBlur_FastBlur_NoRepeat"));
}

/**
 * 用例描述: Glow效果测试
 */
PAG_TEST(PAGFilterTest, Glow) {
  auto pagFile = PAGFile::Load(TestConstants::RESOURCES_ROOT + "filter/Glow.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);

  pagFile->setCurrentTime(200000);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/Glow"));
}

/**
 * 用例描述: DropShadow效果测试
 */

PAG_TEST(PAGFilterTest, DropShadow) {
  auto pagFile = PAGFile::Load(TestConstants::RESOURCES_ROOT + "filter/DropShadow.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);

  pagFile->setCurrentTime(1000000);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/DropShadow"));
}

/**
 * 用例描述: DisplacementMap
 */
PAG_TEST(PAGFilterTest, DisplacementMap) {
  auto pagFile = PAGFile::Load(TestConstants::RESOURCES_ROOT + "filter/DisplacementMap.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);

  pagFile->setCurrentTime(600000);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/DisplacementMap"));
}

/**
 * 用例描述: DisplacementMap特殊用例测试，包含：缩放，模糊
 */
PAG_TEST(PAGFilterTest, DisplacementMap_Scale) {
  auto pagFile =
      PAGFile::Load(TestConstants::RESOURCES_ROOT + "filter/displement_map_video_scale.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);

  pagFile->setCurrentTime(3632520);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/DisplacementMap_Scale"));
}

/**
 * 用例描述: GaussBlur_Static
 */
PAG_TEST(PAGFilterTest, GaussBlur_Static) {
  auto pagFile = PAGFile::Load(TestConstants::RESOURCES_ROOT + "filter/GaussBlur_Static.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);

  pagFile->setCurrentTime(0);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/GaussBlur_Static"));
}

/**
 * 用例描述: GaussianBlur_NoRepeat_Clip
 */
PAG_TEST(PAGFilterTest, GaussianBlur_NoRepeat_Clip) {
  auto pagFile =
      PAGFile::Load(TestConstants::RESOURCES_ROOT + "filter/GaussianBlur_NoRepeat_Clip.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);

  pagFile->setCurrentTime(0);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/GaussianBlur_NoRepeat_Clip"));
}

/**
 * 用例描述: RadialBlur
 */
PAG_TEST(PAGFilterTest, RadialBlur) {
  auto pagFile = PAGFile::Load(TestConstants::RESOURCES_ROOT + "filter/RadialBlur.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);

  pagFile->setCurrentTime(1000000);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/RadialBlur"));
}

/**
 * 用例描述: Mosaic
 */
PAG_TEST(PAGFilterTest, Mosaic) {
  auto pagFile = PAGFile::Load(TestConstants::RESOURCES_ROOT + "filter/MosaicChange.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);

  pagFile->setCurrentTime(0);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/Mosaic"));
}

/**
 * 用例描述: 多滤镜混合效果测试
 */
PAG_TEST(PAGFilterTest, MultiFilter) {
  auto pagFile = PAGFile::Load(TestConstants::RESOURCES_ROOT + "filter/cornerpin-bulge.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);

  pagFile->setCurrentTime(1000000);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/MultiFilter_CornerPin_Bulge"));

  pagFile = PAGFile::Load(TestConstants::RESOURCES_ROOT + "filter/motiontile_blur.pag");
  ASSERT_NE(pagFile, nullptr);
  pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);

  pagFile->setCurrentTime(400000);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/MultiFilter_Motiontile_Blur"));
}

/**
 * 用例描述: LevelsIndividualFilter
 */
PAG_TEST(PAGFilterTest, LevelsIndividualFilter) {
  auto pagFile = PAGFile::Load(TestConstants::RESOURCES_ROOT + "filter/LevelsIndividualFilter.pag");
  ASSERT_NE(pagFile, nullptr);
  pagFile->replaceImage(0, PAGImage::FromPath(TestConstants::ASSETS_ROOT + "rotation.jpg"));
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setProgress(0.7);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/LevelsIndividualFilter"));
}

/**
 * 用例描述: GradientOverlayFilter
 */
PAG_TEST(PAGFilterTest, GradientOverlayFilter) {
  auto pagFile = PAGFile::Load(TestConstants::RESOURCES_ROOT + "filter/GradientOverlay.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setProgress(0.7);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/GradientOverlayFilter"));
}

/**
 * 用例描述: GradientOverlayFilter_Star
 */
PAG_TEST(PAGFilterTest, GradientOverlayFilter_Star) {
  auto pagFile = PAGFile::Load(TestConstants::RESOURCES_ROOT + "filter/GradientOverlayStar.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setProgress(0.0);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/GradientOverlayFilter_Star"));
}

/**
 * 用例描述: FeatherMask
 */
PAG_TEST(PAGFilterTest, FeatherMask) {
  auto pagFile = PAGFile::Load(TestConstants::RESOURCES_ROOT + "filter/FeatherMask.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setProgress(0.7);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/FeatherMask"));
}

/**
 * 用例描述: Corner Pin 缩放
 */
PAG_TEST(PAGFilterTest, CornerPinScale) {
  auto pagFile = PAGFile::Load(TestConstants::RESOURCES_ROOT + "filter/corner_pin_scale.pag");
  ASSERT_NE(pagFile, nullptr);
  pagFile->replaceImage(0,
                        PAGImage::FromPath(TestConstants::RESOURCES_ROOT + "apitest/rotation.jpg"));
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/CornerPinScale"));
}
}  // namespace pag
