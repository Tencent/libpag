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
 * 用例描述: 测试图层对时间的测试是否正确
 */
PAG_TEST_F(PAGTimeUtilsTest, ConvertProgressAndFrame) {
  auto duration = TestPAGFile->duration();
  auto frame = TimeToFrame(duration, TestPAGFile->frameRate());
  for (int i = 0; i <= frame * 2; i++) {
    auto progress = i * 0.5 / frame;
    TestPAGFile->setProgress(progress);
    TestPAGPlayer->flush();
    auto md5 = getMd5FromSnap();
    progress = TestPAGFile->getProgress();
    TestPAGFile->setProgress(progress);
    TestPAGPlayer->flush();
    auto md52 = getMd5FromSnap();
    ASSERT_EQ(md52, md5);
  }
}

}  // namespace pag
