/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <emscripten/emscripten.h>
#include <string>
#include "WebTestBuildInfo.h"
#include "base/TestEnvironment.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "utils/Baseline.h"
#if !defined(TGFX_USE_WEBGPU)
#include <emscripten/html5_webgl.h>
#endif

static bool Initialized = false;
static std::string RunResult = "null";

// Devices::MakeDefault uses the current Emscripten WebGL context.
static bool InitGLContext() {
#if !defined(TGFX_USE_WEBGPU)
  EmscriptenWebGLContextAttributes attrs = {};
  emscripten_webgl_init_context_attributes(&attrs);
  attrs.majorVersion = 2;
  auto context = emscripten_webgl_create_context("#pag-test-canvas", &attrs);
  if (context <= 0) {
    return false;
  }
  if (emscripten_webgl_make_context_current(context) != EMSCRIPTEN_RESULT_SUCCESS) {
    emscripten_webgl_destroy_context(context);
    return false;
  }
#endif
  return true;
}

static int ExecuteTests(const char* filter) {
  if (Initialized || !InitGLContext()) {
    return 1;
  }
  Initialized = true;
  int argc = 1;
  char name[] = "pag-test";
  char* argv[] = {name, nullptr};
  testing::AddGlobalTestEnvironment(new pag::TestEnvironment());
  testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(filter) = filter && filter[0] ? filter : "*";
  testing::GTEST_FLAG(output) = "json:/test/report.json";
  auto exitCode = RUN_ALL_TESTS();
  const auto* tests = testing::UnitTest::GetInstance();
  auto stats = pag::Baseline::GetComparisonStats();
  // A wholesale stale comparison cache sends every key down the accepted-version-change path,
  // which must not pass without a single real comparison.
  if (tests->test_to_run_count() == 0 || stats.missingBaseline != 0 ||
      (stats.compared == 0 && stats.versionSkipped != 0)) {
    exitCode = 1;
  }
  nlohmann::json result = {{"exitCode", exitCode},
                           {"tests", tests->test_to_run_count()},
                           {"failed", tests->failed_test_count()},
                           {"skipped", tests->skipped_test_count()},
                           {"comparisons",
                            {{"compared", stats.compared},
                             {"versionSkipped", stats.versionSkipped},
                             {"missingBaseline", stats.missingBaseline}}}};
  RunResult = result.dump();
  return exitCode;
}

extern "C" {

EMSCRIPTEN_KEEPALIVE
const char* GetBuildInfo() {
  return BuildInfoJson;
}

EMSCRIPTEN_KEEPALIVE
const char* GetRunResult() {
  return RunResult.c_str();
}

EMSCRIPTEN_KEEPALIVE
int RunAllTests() {
  return ExecuteTests(nullptr);
}

EMSCRIPTEN_KEEPALIVE
int RunTest(const char* filter) {
  return ExecuteTests(filter);
}
}
