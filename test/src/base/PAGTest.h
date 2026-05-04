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

#include <memory>
#include "gtest/gtest.h"
#include "tgfx/gpu/opengl/GLDevice.h"

namespace pagx {
class FontConfig;
}

namespace pagx::pag {
class FontProvider;
}

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

class PAGXTest : public PAGTest {
 public:
  std::shared_ptr<tgfx::GLDevice> device = nullptr;
  tgfx::Context* context = nullptr;

  void SetUp() override;
  void TearDown() override;

  /**
   * Shared FontConfig populated by SetUp() with Arial (macOS) + Noto Sans SC +
   * Noto Color Emoji + Noto Sans Hebrew. Every PAGX rendering test can pass
   * this to `PAGXDocument::applyLayout(&fontConfig())` so the layout engine
   * sees the same typefaces that the matched PAG-v2 Inflater sees via
   * `fontProvider()`. This eliminates the historical drift where PathA used
   * pagx::FontConfig (empty in tests) while PathB used pag::FontManager.
   */
  pagx::FontConfig& fontConfig();

  /**
   * FontProvider backed by the same FontConfig above, for PAGLoader /
   * LayerInflater::Inflate Options.
   */
  std::shared_ptr<pagx::pag::FontProvider> fontProvider();

 private:
  std::shared_ptr<pagx::FontConfig> sharedFontConfig = nullptr;
  std::shared_ptr<pagx::pag::FontProvider> sharedFontProvider = nullptr;
};

#define PAGX_TEST(test_case_name, test_name)            \
  GTEST_TEST_(test_case_name, test_name, pag::PAGXTest, \
              ::testing::internal::GetTypeId<pag::PAGXTest>())

class CLITest : public testing::Test {};

#define CLI_TEST(test_case_name, test_name)            \
  GTEST_TEST_(test_case_name, test_name, pag::CLITest, \
              ::testing::internal::GetTypeId<pag::CLITest>())

}  // namespace pag
