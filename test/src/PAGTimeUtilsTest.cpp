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

#include <fstream>
#include "base/utils/TimeUtil.h"
#include "nlohmann/json.hpp"
#include "utils/TestUtils.h"

namespace pag {
/**
 * 用例描述: 测试图层对时间的输入输出是否一致
 */
PAG_TEST(PAGTimeUtilsTest, ConvertProgressAndFrame) {
  auto pagSurface = OffscreenSurface::Make(320, 180);
  ASSERT_TRUE(pagSurface != nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  auto pagFile = LoadPAGFile("resources/apitest/ZC_mg_seky2_landscape.pag");
  pagPlayer->setComposition(pagFile);
  auto duration = pagFile->duration();
  auto totalFrames = TimeToFrame(duration, pagFile->frameRate());
  for (int i = 99; i < 120; i++) {
    auto progress = i * 0.5 / totalFrames;
    pagFile->setProgress(progress);
    pagPlayer->flush();
    EXPECT_TRUE(
        Baseline::Compare(pagSurface, "PAGTimeUtilsTest/ConvertProgressAndFrame/" + ToString(i)));
    progress = pagFile->getProgress();
    pagFile->setProgress(progress);
    pagPlayer->flush();
    EXPECT_TRUE(
        Baseline::Compare(pagSurface, "PAGTimeUtilsTest/ConvertProgressAndFrame/" + ToString(i)));
  }
}

/**
 * 用例描述：测试工具类中，时间和进度的转换是否正确
 */
PAG_TEST(PAGTimeUtilsTest, EdgeTest) {
  EXPECT_EQ(ProgressToFrame(FrameToProgress(40, 41), 41), 40);
  EXPECT_EQ(ProgressToFrame(FrameToProgress(0, 41), 41), 0);

  EXPECT_EQ(ProgressToTime(1.0, 41), 40);
  EXPECT_EQ(ProgressToTime(0.0, 41), 0);
}

}  // namespace pag
