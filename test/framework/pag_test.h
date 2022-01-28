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

#include "PAGCpuTest.h"
#include "PAGTestEnvironment.h"

namespace pag {
// public macro

#define DEFAULT_PAG_PATH "../resources/apitest/test.pag"

/**
 * Register a test case.
 */
#define PAG_TEST(test_suite_name, test_name) TEST(PAG_CLASS_NAME(test_suite_name), test_name)

/**
 * Define a class, all test that use this test_fixture will execute between PagSetUp and
 * PagTearDown. All test will share a same openGL context. DEFAULT_PAG_PATH will be used to init.
 */
#define PAG_TEST_SUIT(class_name) PAG_TEST_SUIT_WITH_PATH(class_name, DEFAULT_PAG_PATH)

/**
 * Define a class, Every test that use this test_fixture will execute in a new PagSetUp and
 * PagTearDown. Every test will have a independent openGL context.
 */
#define PAG_TEST_CASE(class_name) PAG_TEST_CASE_WITH_PATH(class_name, DEFAULT_PAG_PATH)

/**
 * Register a test case that its execution environment will depend on how to define the
 * test_fixture.
 */
#define PAG_TEST_F(test_fixture, test_name) TEST_F(PAG_CLASS_NAME(test_fixture), test_name)

/**
 * Define a class, all test that use this test_fixture will execute between PagSetUp and
 * PagTearDown. All test will share a same openGL context. Pag_path will be used to init.
 */
#define PAG_TEST_SUIT_WITH_PATH(class_name, pag_path)            \
  class PAG_CLASS_NAME(class_name) : public PAGCpuTest {         \
   public:                                                       \
    PAG_CLASS_NAME(class_name)() {                               \
    }                                                            \
    ~PAG_CLASS_NAME(class_name)() {                              \
    }                                                            \
    static void SetUpTestSuite() {                               \
      PagSetUp(getPagPath());                                    \
    }                                                            \
    static void TearDownTestSuite() {                            \
      PagTearDown();                                             \
    }                                                            \
    static std::string getPagPath();                             \
                                                                 \
   private:                                                      \
    GTEST_DISALLOW_COPY_AND_ASSIGN_(PAG_CLASS_NAME(class_name)); \
  };                                                             \
  std::string PAG_CLASS_NAME(class_name)::getPagPath() {         \
    return pag_path;                                             \
  }

/**
 * Define a class, Every test that use this test_fixture will execute in a new PagSetUp and
 * PagTearDown. Every test will have a independent openGL context. Pag_path will be used to init.
 */
#define PAG_TEST_CASE_WITH_PATH(class_name, pag_path)            \
  class PAG_CLASS_NAME(class_name) : public PAGCpuTest {         \
   public:                                                       \
    PAG_CLASS_NAME(class_name)() {                               \
    }                                                            \
    ~PAG_CLASS_NAME(class_name)() {                              \
    }                                                            \
    void SetUp() {                                               \
      PagSetUp(getPagPath());                                    \
    }                                                            \
    void TearDown() {                                            \
      PagTearDown();                                             \
    }                                                            \
    static std::string getPagPath();                             \
                                                                 \
   private:                                                      \
    GTEST_DISALLOW_COPY_AND_ASSIGN_(PAG_CLASS_NAME(class_name)); \
  };                                                             \
  std::string PAG_CLASS_NAME(class_name)::getPagPath() {         \
    return pag_path;                                             \
  }

// internal macro
#define PAG_CLASS_NAME(class_name) class_name

}  // namespace pag
