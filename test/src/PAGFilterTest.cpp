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

#include <fstream>
#include "base/utils/TimeUtil.h"
#include "nlohmann/json.hpp"
#include "utils/TestUtils.h"

namespace pag {
using nlohmann::json;

/**
 * 用例描述: CornerPin用例
 */
PAG_TEST(PAGFilterTest, CornerPin) {
  auto pagFile = LoadPAGFile("resources/filter/cornerpin.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
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
  auto pagFile = LoadPAGFile("resources/filter/bulge.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
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
  auto pagFile = LoadPAGFile("resources/filter/motiontile.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
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
  auto pagFile = LoadPAGFile("resources/filter/MotionBlur.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
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
  auto pagFile = LoadPAGFile("resources/filter/fastblur.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);

  pagFile->setCurrentTime(1000000);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/GaussBlur_FastBlur"));

  pagFile = LoadPAGFile("resources/filter/fastblur_norepeat.pag");
  ASSERT_NE(pagFile, nullptr);
  pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
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
  auto pagFile = LoadPAGFile("resources/filter/Glow.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
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
  auto pagFile = LoadPAGFile("resources/filter/DropShadow.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
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
  auto pagFile = LoadPAGFile("resources/filter/DisplacementMap.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
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
  auto pagFile = LoadPAGFile("resources/filter/displement_map_video_scale.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
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
  auto pagFile = LoadPAGFile("resources/filter/GaussBlur_Static.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
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
  auto pagFile = LoadPAGFile("resources/filter/GaussianBlur_NoRepeat_Clip.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
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
  auto pagFile = LoadPAGFile("resources/filter/RadialBlur.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
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
  auto pagFile = LoadPAGFile("resources/filter/MosaicChange.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
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
  auto pagFile = LoadPAGFile("resources/filter/cornerpin-bulge.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);

  pagFile->setCurrentTime(1000000);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/MultiFilter_CornerPin_Bulge"));

  pagFile = LoadPAGFile("resources/filter/motiontile_blur.pag");
  ASSERT_NE(pagFile, nullptr);
  pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
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
  auto pagFile = LoadPAGFile("resources/filter/LevelsIndividualFilter.pag");
  ASSERT_NE(pagFile, nullptr);
  pagFile->replaceImage(0, MakePAGImage("assets/rotation.jpg"));
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
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
  auto pagFile = LoadPAGFile("resources/filter/GradientOverlay.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
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
  auto pagFile = LoadPAGFile("resources/filter/GradientOverlayStar.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
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
  auto pagFile = LoadPAGFile("resources/filter/FeatherMask.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
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
  auto pagFile = LoadPAGFile("resources/filter/corner_pin_scale.pag");
  ASSERT_NE(pagFile, nullptr);
  pagFile->replaceImage(0, MakePAGImage("resources/apitest/rotation.jpg"));
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/CornerPinScale"));
}

/**
 * 用例描述: HueSaturation
 */
PAG_TEST(PAGFilterTest, HueSaturation) {
  auto pagFile = LoadPAGFile("resources/filter/HueSaturation.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setProgress(0.7);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/HueSaturation"));
}

/**
 * 用例描述: BrightnessContrast
 */
PAG_TEST(PAGFilterTest, BrightnessContrast) {
  auto pagFile = LoadPAGFile("resources/filter/BrightnessContrast.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setProgress(0.7);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/BrightnessContrast"));
}

/**
 * 用例描述: Logo with mipmap
 */
PAG_TEST(PAGFilterTest, LogoMipmap) {
  auto pagFile = LoadPAGFile("resources/filter/LogoMipmap.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setProgress(0.5);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/LogoMipmap"));
}

/**
 * 用例描述: OuterGlow
 */
PAG_TEST(PAGFilterTest, OuterGlow) {
  auto pagFile = LoadPAGFile("resources/filter/outerglow.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setProgress(0.7);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/OuterGlow"));
}

/**
 * 用例描述: Stroke
 */
PAG_TEST(PAGFilterTest, Stroke) {
  auto pagFile = LoadPAGFile("resources/filter/stroke.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setProgress(0.7);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/Stroke"));
}

/**
 * 用例描述: Default feather mask color
 */
PAG_TEST(PAGFilterTest, DefaultFeatherMask) {
  auto pagFile = LoadPAGFile("resources/filter/DefaultFeatherMask.pag");
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setProgress(0.7);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGFilterTest/DefaultFeatherMask"));
}

}  // namespace pag
