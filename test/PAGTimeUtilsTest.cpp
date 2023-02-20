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

#include <fstream>
#include "base/utils/TimeUtil.h"
#include "framework/pag_test.h"
#include "framework/utils/PAGTestUtils.h"
#include "nlohmann/json.hpp"

namespace pag {

PAG_TEST_SUIT(PAGTimeUtilsTest)

/**
 * 用例描述: 测试图层对时间的输入输出是否一致
 */
PAG_TEST_F(PAGTimeUtilsTest, ConvertProgressAndFrame) {
  auto pagFile = LoadPAGFile("apitest/ZC_mg_seky2_landscape.pag");
  TestPAGPlayer->setComposition(pagFile);
  auto duration = pagFile->duration();
  auto totalFrames = TimeToFrame(duration, pagFile->frameRate());
  for (int i = 99; i < 120; i++) {
    auto progress = i * 0.5 / totalFrames;
    pagFile->setProgress(progress);
    TestPAGPlayer->flush();
    EXPECT_TRUE(Baseline::Compare(TestPAGSurface,
                                  "PAGTimeUtilsTest/ConvertProgressAndFrame/" + ToString(i)));
    progress = pagFile->getProgress();
    pagFile->setProgress(progress);
    TestPAGPlayer->flush();
    EXPECT_TRUE(Baseline::Compare(TestPAGSurface,
                                  "PAGTimeUtilsTest/ConvertProgressAndFrame/" + ToString(i)));
  }
}

}  // namespace pag
