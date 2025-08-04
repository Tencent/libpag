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
#include "utils/TestUtils.h"

namespace pag {

/**
 * 用例描述: 测试 PolyStar-star
 */
PAG_TEST(PAGShapeLayerTest, star) {
  auto pagFile = LoadPAGFile("resources/apitest/poly_star.pag");
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
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
PAG_TEST(PAGShapeLayerTest, polygon) {
  auto pagFile = LoadPAGFile("resources/apitest/polygon.pag");
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
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
PAG_TEST(PAGShapeLayerTest, polygon_round_corner) {
  auto pagFile = LoadPAGFile("resources/apitest/polygon_round_corner.pag");
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
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
PAG_TEST(PAGShapeLayerTest, star_round_corner) {
  auto pagFile = LoadPAGFile("resources/apitest/poly_star_round_corner.pag");
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
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
PAG_TEST(PAGShapeLayerTest, track_matte_path_union) {
  auto pagFile = LoadPAGFile("resources/apitest/track_matte_path_union.pag");
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setProgress(0.3);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGShapeLayerTest/track_matte_path_union"));
}

/**
 * 用例描述: 测试 shape transform + round corner
 */
PAG_TEST(PAGShapeLayerTest, shape_transform_round_corner) {
  auto pagFile = LoadPAGFile("resources/apitest/shape_transform_round_corner.pag");
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  pagPlayer->setProgress(0.5);
  pagPlayer->flush();
  EXPECT_TRUE(Baseline::Compare(pagSurface, "PAGShapeLayerTest/shape_transform_round_corner"));
}
}  // namespace pag
