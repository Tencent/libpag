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

#include "TestUtils.h"
#include "framework/pag_test.h"
#include "framework/utils/PAGTestUtils.h"

namespace pag {
PAG_TEST_CASE(PAGGradientColorTest)

/**
 * 用例描述: 渐变
 */
PAG_TEST_F(PAGGradientColorTest, GradientColor_ID84028439) {
  std::vector<std::string> files;
  GetAllPAGFiles("../resources/gradient", files);
  for (auto& file : files) {
    auto pagFile = PAGFile::Load(file);
    EXPECT_NE(pagFile, nullptr);
    TestPAGPlayer->setComposition(pagFile);
    TestPAGPlayer->setProgress(0.5);
    TestPAGPlayer->flush();
    auto md5 = DumpMD5(TestPAGSurface);
    auto found = file.find_last_of("/\\");
    auto fileName = file.substr(found + 1);
    PAGTestEnvironment::DumpJson["PAGGradientColorTest"][fileName] = md5;
#ifdef COMPARE_JSON_PATH
    auto compareMD5 = PAGTestEnvironment::CompareJson["PAGGradientColorTest"][fileName];
    auto path = "../test/out/gradient_" + fileName + ".png";
    TraceIf(TestPAGSurface, path, compareMD5.get<std::string>() != md5);
    EXPECT_EQ(compareMD5.get<std::string>(), md5);
#endif
  }
}
}  // namespace pag
