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

#include <filesystem>
#include "utils/TestUtils.h"

namespace pag {
/**
 * 用例描述: 渐变
 */
PAG_TEST(PAGGradientColorTest, GradientColor) {
  PAG_SETUP(TestPAGSurface, TestPAGPlayer, TestPAGFile);
  auto files = GetAllPAGFiles("resources/gradient");
  for (auto& file : files) {
    auto pagFile = PAGFile::Load(file);
    EXPECT_NE(pagFile, nullptr);
    TestPAGPlayer->setComposition(pagFile);
    TestPAGPlayer->setProgress(0.5);
    TestPAGPlayer->flush();
    std::filesystem::path path(file);
    auto key = path.filename().replace_extension();
    EXPECT_TRUE(Baseline::Compare(TestPAGSurface, "PAGGradientColorTest/" + key.string()));
  }
}
}  // namespace pag
