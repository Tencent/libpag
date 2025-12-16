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

#pragma once

#include "gtest/gtest.h"

namespace pag {
class PAGTest : public testing::Test {
 public:
  static bool HasFailure();

  void SetUp() override;

  void TearDown() override;
};

#define PAG_TEST(test_case_name, test_name)            \
  GTEST_TEST_(test_case_name, test_name, pag::PAGTest, \
              ::testing::internal::GetTypeId<pag::PAGTest>())

#define PAG_SETUP(pagSurface, pagPlayer, pagFile)                                \
  auto pagFile = LoadPAGFile("resources/apitest/test.pag");                      \
  ASSERT_TRUE(pagFile != nullptr);                                               \
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height()); \
  ASSERT_TRUE(pagSurface != nullptr);                                            \
  auto pagPlayer = std::make_shared<PAGPlayer>();                                \
  pagPlayer->setSurface(pagSurface);                                             \
  pagPlayer->setComposition(pagFile);

#define PAG_SETUP_ISOLATED(pagSurface, pagPlayer, pagFile)                          \
  auto pagFile = LoadPAGFile("resources/apitest/test.pag");                         \
  ASSERT_TRUE(pagFile != nullptr);                                                  \
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height()); \
  ASSERT_TRUE(pagSurface != nullptr);                                               \
  auto pagPlayer = std::make_shared<PAGPlayer>();                                   \
  pagPlayer->setSurface(pagSurface);                                                \
  pagPlayer->setComposition(pagFile);

#define PAG_SETUP_WITH_PATH(pagSurface, pagPlayer, pagFile, pagPath)             \
  auto pagFile = LoadPAGFile(pagPath);                                           \
  ASSERT_TRUE(pagFile != nullptr);                                               \
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height()); \
  ASSERT_TRUE(pagSurface != nullptr);                                            \
  auto pagPlayer = std::make_shared<PAGPlayer>();                                \
  pagPlayer->setSurface(pagSurface);                                             \
  pagPlayer->setComposition(pagFile);
}  // namespace pag
