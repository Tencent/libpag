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

#include "PAGCpuTest.h"
#include <fstream>
#include "gtest/gtest.h"
#include "utils/PAGTestUtils.h"


namespace pag {

std::shared_ptr<PAGFile> PAGCpuTest::TestPAGFile = nullptr;
std::shared_ptr<PAGSurface> PAGCpuTest::TestPAGSurface = nullptr;
std::shared_ptr<PAGPlayer> PAGCpuTest::TestPAGPlayer = nullptr;

PAGCpuTest::PAGCpuTest() {
}

PAGCpuTest::~PAGCpuTest() {
}

void PAGCpuTest::SetUpTestSuite() {
}

void PAGCpuTest::TearDownTestSuite() {
}

void PAGCpuTest::SetUp() {
}

void PAGCpuTest::TearDown() {
}

void PAGCpuTest::PagSetUp(std::string pagPath) {
  TestPAGFile = PAGFile::Load(pagPath);
  ASSERT_NE(TestPAGFile, nullptr) << "pag path is:" << GetPagPath() << std::endl;
  TestPAGSurface = PAGSurface::MakeOffscreen(TestPAGFile->width(), TestPAGFile->height());
  TestPAGPlayer = std::make_shared<PAGPlayer>();
  TestPAGPlayer->setSurface(TestPAGSurface);
  TestPAGPlayer->setComposition(TestPAGFile);
}

void PAGCpuTest::PagTearDown() {
  TestPAGFile = nullptr;
  TestPAGSurface = nullptr;
  TestPAGPlayer = nullptr;
}

std::string PAGCpuTest::GetPagPath() {
  return DEFAULT_PAG_PATH;
}

}  // namespace pag
