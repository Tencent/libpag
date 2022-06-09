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

namespace pag {

PAG_TEST_SUIT(PAGShapeLayerTest)

/**
 * 用例描述: 测试 PolyStar-star
 */
PAG_TEST_F(PAGShapeLayerTest, star) {
  auto pagFile = PAGFile::Load("../resources/apitest/poly_star.pag");
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setProgress(0);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGShapeLayerTest/star"));
}

/**
 * 用例描述: 测试 PolyStar-polygon
 */
PAG_TEST_F(PAGShapeLayerTest, polygon) {
  auto pagFile = PAGFile::Load("../resources/apitest/polygon.pag");
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setProgress(0);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGShapeLayerTest/polygon"));
}

/**
 * 用例描述: 测试 PolyStar-polygon + round corner
 */
PAG_TEST_F(PAGShapeLayerTest, polygon_round_corner) {
  auto pagFile = PAGFile::Load("../resources/apitest/polygon_round_corner.pag");
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setProgress(0);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGShapeLayerTest/polygon_round_corner"));
}

/**
 * 用例描述: 测试 PolyStar-star + round corner
 */
PAG_TEST_F(PAGShapeLayerTest, star_round_corner) {
  auto pagFile = PAGFile::Load("../resources/apitest/poly_star_round_corner.pag");
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setProgress(0);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGShapeLayerTest/star_round_corner"));
}

/**
 * 用例描述: trackMatteLayer 的 path 合并时要 union，path 的方向不一致时 append 模式会镂空。
 */
PAG_TEST_F(PAGShapeLayerTest, track_matte_path_union) {
  auto pagFile = PAGFile::Load("../resources/apitest/track_matte_path_union.pag");
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setProgress(0.3);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGShapeLayerTest/track_matte_path_union"));
}
}  // namespace pag
