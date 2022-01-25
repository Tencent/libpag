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

#include "framework/pag_test.h"
#include "framework/utils/PAGTestUtils.h"
#include "nlohmann/json.hpp"

namespace pag {
using nlohmann::json;

PAG_TEST_SUIT(PAGSimplePathTest)

/**
 * 用例描述: 测试 shader 绘制椭圆
 */
PAG_TEST_F(PAGSimplePathTest, TestRRect_ID86088211) {
    auto pagFile = PAGFile::Load("../resources/apitest/ellipse.pag");
    auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
    auto pagPlayer = std::make_shared<PAGPlayer>();
    pagPlayer->setSurface(pagSurface);
    pagPlayer->setComposition(pagFile);
    pagPlayer->setMatrix(Matrix::I());
    pagPlayer->setProgress(0);
    pagPlayer->flush();
    auto md5 = DumpMD5(pagSurface);
    PAGTestEnvironment::DumpJson["PAGSimplePathTest"]["TestRRect_ID86088211"] = md5;
#ifdef COMPARE_JSON_PATH
    auto compareMD5 = PAGTestEnvironment::CompareJson["PAGSimplePathTest"]["TestRRect_ID86088211"];
    TraceIf(pagSurface, "../test/out/TestRRect_ID86088211.png", compareMD5.get<std::string>() != md5);
    EXPECT_EQ(compareMD5.get<std::string>(), md5);
#endif
}
}  // namespace pag
