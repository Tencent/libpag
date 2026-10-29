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
#include "rendering/gpu/Devices.h"
#include "tgfx/gpu/Device.h"

namespace pag {
class PAGTest : public testing::Test {
 public:
  static bool HasFailure();

  static bool RegisterPixelComparison(const char* testSuite, const char* testName);

  static bool IsPixelComparison();

  void SetUp() override;

  void TearDown() override;
};

#define PAG_TEST(test_case_name, test_name)            \
  GTEST_TEST_(test_case_name, test_name, pag::PAGTest, \
              ::testing::internal::GetTypeId<pag::PAGTest>())

// Keep the fixture type unchanged: one suite may mix pixel and non-pixel tests.
#define PAG_REGISTER_PIXEL_TEST(test_case_name, test_name)                            \
  [[maybe_unused]] static const bool test_case_name##_##test_name##_pixelComparison = \
      pag::PAGTest::RegisterPixelComparison(#test_case_name, #test_name);

#define PAG_PIXEL_TEST(test_case_name, test_name)    \
  PAG_REGISTER_PIXEL_TEST(test_case_name, test_name) \
  PAG_TEST(test_case_name, test_name)

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
  void SetUp() override {
    PAGTest::SetUp();
    if (HasFatalFailure()) {
      return;
    }
    // Probe that the default device can be created and locked, then release it immediately:
    // the test body must be free to run code that locks the device (directly or through
    // libpag's internal renderers). Since tgfx deduplicates devices per native GPU object
    // (#1581), holding the lock across the whole test would deadlock any such call on the
    // shared device. Tests that need a locked context create a pag::DeviceLockScope locally.
    DeviceLockScope probe;
    ASSERT_TRUE(static_cast<bool>(probe));
  }
};

#define PAGX_TEST(test_case_name, test_name)            \
  GTEST_TEST_(test_case_name, test_name, pag::PAGXTest, \
              ::testing::internal::GetTypeId<pag::PAGXTest>())

#define PAGX_PIXEL_TEST(test_case_name, test_name)   \
  PAG_REGISTER_PIXEL_TEST(test_case_name, test_name) \
  PAGX_TEST(test_case_name, test_name)

class CLITest : public PAGTest {};

#define CLI_TEST(test_case_name, test_name)            \
  GTEST_TEST_(test_case_name, test_name, pag::CLITest, \
              ::testing::internal::GetTypeId<pag::CLITest>())

#define CLI_PIXEL_TEST(test_case_name, test_name)    \
  PAG_REGISTER_PIXEL_TEST(test_case_name, test_name) \
  CLI_TEST(test_case_name, test_name)

}  // namespace pag
