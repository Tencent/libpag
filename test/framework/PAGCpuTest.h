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

#pragma once

#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "pag/file.h"
#include "pag/pag.h"
#include "pag_test.h"
#include "tgfx/core/PixelBuffer.h"

namespace pag {
class PAGCpuTest : public testing::Test {
 public:
  PAGCpuTest();

  ~PAGCpuTest() override;

  /**
   * Sets up the test suite.
   */
  static void SetUpTestSuite();

  /**
   * Sets down the test suite.
   */
  static void TearDownTestSuite();

  /**
   * Sets up the test case.
   */
  void SetUp() override;

  /**
   * Sets down the test case.
   */
  void TearDown() override;

  static void PagSetUp(std::string pagPath);

  static void PagTearDown();

  static std::string GetPagPath();

  static std::shared_ptr<PAGFile> TestPAGFile;

  static std::shared_ptr<PAGSurface> TestPAGSurface;

  static std::shared_ptr<PAGPlayer> TestPAGPlayer;

 private:
  GTEST_DISALLOW_COPY_AND_ASSIGN_(PAGCpuTest);
};

}  // namespace pag
