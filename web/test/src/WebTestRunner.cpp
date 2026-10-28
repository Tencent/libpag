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
#include "base/TestEnvironment.h"
#include "gtest/gtest.h"
#if defined(__EMSCRIPTEN__) && !defined(TGFX_USE_WEBGPU)
#include <emscripten/html5_webgl.h>
#endif

static bool initialized = false;

// The WebGL backend's Devices::MakeDefault() falls back to GLDevice::Current(), which requires a
// WebGL context to already be created and current. tgfx's own web tests set this up through their
// JS bindings module; the test runner has no bindings, so create the context here on the hidden
// #pag-test-canvas element before any test touches a device.
static void initGLContext() {
#if defined(__EMSCRIPTEN__) && !defined(TGFX_USE_WEBGPU)
  static bool glReady = false;
  if (glReady) {
    return;
  }
  glReady = true;
  EmscriptenWebGLContextAttributes attrs = {};
  emscripten_webgl_init_context_attributes(&attrs);
  attrs.majorVersion = 2;
  auto context = emscripten_webgl_create_context("#pag-test-canvas", &attrs);
  if (context == 0) {
    return;
  }
  emscripten_webgl_make_context_current(context);
#endif
}

// Asyncify (WebGPU only) rewrites the whole wasm binary at link time and can corrupt the
// execution state of code that busy-waits on the main thread. Tests incompatible with
// instrumentation are excluded here through the gtest filter, mirroring tgfx's web runner.
// No libpag test is currently known to be incompatible, so the list stays empty.
static const char* excludeFilter = "";

static void initTests() {
  if (initialized) {
    return;
  }
  initialized = true;
  int argc = 1;
  const char* argv[] = {"pag-test"};
  testing::AddGlobalTestEnvironment(new pag::TestEnvironment());
  testing::InitGoogleTest(&argc, const_cast<char**>(argv));
  if (excludeFilter[0] != '\0') {
    testing::GTEST_FLAG(filter) = excludeFilter;
  }
}

extern "C" {

EMSCRIPTEN_KEEPALIVE
int RunAllTests() {
  initGLContext();
  initTests();
  return RUN_ALL_TESTS();
}

EMSCRIPTEN_KEEPALIVE
int RunTest(const char* filter) {
  initGLContext();
  initTests();
  if (filter != nullptr && filter[0] != '\0') {
    if (excludeFilter[0] != '\0') {
      testing::GTEST_FLAG(filter) = std::string(filter) + ":" + excludeFilter;
    } else {
      testing::GTEST_FLAG(filter) = filter;
    }
  } else if (excludeFilter[0] != '\0') {
    testing::GTEST_FLAG(filter) = excludeFilter;
  }
  return RUN_ALL_TESTS();
}
}
