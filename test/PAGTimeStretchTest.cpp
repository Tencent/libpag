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

#include <vector>
#include "base/utils/TimeUtil.h"
#include "framework/pag_test.h"
#include "framework/utils/PAGTestUtils.h"
#include "nlohmann/json.hpp"

namespace pag {
using nlohmann::json;

json dumpJson;
json compareJson;
bool needCompare;
bool dumpStatus;

PAG_TEST_SUIT(PAGTimeStrechTest)

void TimeStretchTest(std::string path, std::string methodName, float scaleFactor) {
  std::vector<int> shortenArray = {0, 6, 30, 59};
  std::vector<int> stretchArray = {0, 12, 120, 239};
  int startIndex = path.rfind('/') + 1;
  int suffixIndex = path.rfind('.');
  auto fileName = path.substr(startIndex, suffixIndex - startIndex);
  std::vector<std::string> compareVector;

  auto TestPAGFile = LoadPAGFile(path);
  ASSERT_NE(TestPAGFile, nullptr);
  int64_t duartion = TestPAGFile->duration();
  std::vector<int> array;

  TestPAGFile->setDuration(duartion * scaleFactor);
  if (scaleFactor < 1) {
    fileName += "_shorten";
    array = shortenArray;
  } else {
    fileName += "_stretch";
    array = stretchArray;
  }

  auto pagSurface = PAGSurface::MakeOffscreen(TestPAGFile->width(), TestPAGFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(TestPAGFile);

  Frame totalFrames = TimeToFrame(TestPAGFile->duration(), TestPAGFile->frameRate());

  std::string errorMsg = "";

  auto pagImage = MakePAGImage("apitest/test_timestretch.png");
  TestPAGFile->replaceImage(0, pagImage);

  int index = 0;
  for (const auto& currentFrame : array) {
    pagPlayer->setProgress((currentFrame + 0.1) * 1.0 / totalFrames);
    pagPlayer->getProgress();
    pagPlayer->flush();
    auto compareResult = Baseline::Compare(pagSurface, "PAGTimeStrechTest/" + methodName + "_" +
                                                           fileName + "_" + ToString(currentFrame));
    EXPECT_TRUE(compareResult);
    if (!compareResult) {
      errorMsg += (std::to_string(currentFrame) + ";");
    }
    index++;
  }
  EXPECT_EQ(errorMsg, "") << fileName << " frame fail";
}

/**
 * 用例描述: PAGTimeStrech渲染测试: Repeat模式-缩减
 */
PAG_TEST_F(PAGTimeStrechTest, Repeat_Shorten) {
  TimeStretchTest("timestretch/repeat.pag", "Repeat_Shorten", 0.5);
}

/**
 * 用例描述: PAGTimeStrech渲染测试: Repeat模式-拉伸
 */
PAG_TEST_F(PAGTimeStrechTest, Repeat_Stretch) {
  TimeStretchTest("timestretch/repeat.pag", "Repeat_Stretch", 2);
}

/**
 * 用例描述: PAGTimeStrech渲染测试-RepeatInverted-缩减
 */
PAG_TEST_F(PAGTimeStrechTest, RepeatInverted_Shorten) {
  TimeStretchTest("timestretch/repeatInverted.pag", "RepeatInverted_Shorten", 0.5);
}

/**
 * 用例描述: PAGTimeStrech渲染测试-RepeatInverted-拉伸
 */
PAG_TEST_F(PAGTimeStrechTest, RepeatInverted_Stretch) {
  TimeStretchTest("timestretch/repeatInverted.pag", "RepeatInverted_Stretch", 2);
}

/**
 * 用例描述: PAGTimeStrech渲染测试-Scale模式-缩减
 */
PAG_TEST_F(PAGTimeStrechTest, Scale_Shorten) {
  TimeStretchTest("timestretch/scale.pag", "Scale_Shorten", 0.5);
}

/**
 * 用例描述: PAGTimeStrech渲染测试: Scale模式-拉伸
 */
PAG_TEST_F(PAGTimeStrechTest, Scale_Stretch) {
  TimeStretchTest("timestretch/scale.pag", "Scale_Stretch", 2);
}

}  // namespace pag
