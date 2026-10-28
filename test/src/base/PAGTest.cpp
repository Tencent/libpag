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

#include "PAGTest.h"
#include <set>
#include <string>
#include <utility>
#include "utils/DevicePool.h"

namespace pag {
static bool hasFailure = false;

static std::set<std::pair<std::string, std::string>>& PixelComparisons() {
  static std::set<std::pair<std::string, std::string>> tests;
  return tests;
}

bool PAGTest::RegisterPixelComparison(const char* testSuite, const char* testName) {
  return PixelComparisons().emplace(testSuite, testName).second;
}

bool PAGTest::HasFailure() {
  return hasFailure;
}

void PAGTest::SetUp() {
  const auto* test = testing::UnitTest::GetInstance()->current_test_info();
  if (test == nullptr || PixelComparisons().count({test->test_suite_name(), test->name()}) == 0) {
    return;
  }
  // Pixel tests opt into tgfx's atlas-isolation workaround before rendering. This intentionally
  // excludes cross-test atlas reuse from baseline coverage; unmarked behavior tests retain it.
  ASSERT_TRUE(DevicePool::ResetAtlasForComparison())
      << "Cannot prepare pixel comparison: failed to create or lock the Metal device.";
}

void PAGTest::TearDown() {
  hasFailure = hasFailure || ::testing::Test::HasFailure();
}
}  // namespace pag
