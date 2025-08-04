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

#include <vector>
#include "base/utils/TimeUtil.h"
#include "nlohmann/json.hpp"
#include "utils/TestUtils.h"

namespace pag {
using nlohmann::json;

void TimeStretchTest(std::string path, std::string methodName, float scaleFactor) {
  std::vector<int> shortenArray = {0, 6, 30, 59};
  std::vector<int> stretchArray = {0, 12, 120, 239};
  auto startIndex = path.rfind('/') + 1;
  auto suffixIndex = path.rfind('.');
  auto fileName = path.substr(startIndex, suffixIndex - startIndex);
  std::vector<std::string> compareVector;

  auto pagFile = LoadPAGFile(path);
  ASSERT_NE(pagFile, nullptr);
  int64_t duartion = pagFile->duration();
  std::vector<int> array;

  pagFile->setDuration(duartion * scaleFactor);
  if (scaleFactor < 1) {
    fileName += "_shorten";
    array = shortenArray;
  } else {
    fileName += "_stretch";
    array = stretchArray;
  }

  auto pagSurface = OffscreenSurface::Make(pagFile->width() / 4, pagFile->height() / 4);
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);

  Frame totalFrames = TimeToFrame(pagFile->duration(), pagFile->frameRate());

  std::string errorMsg = "";

  auto pagImage = MakePAGImage("resources/apitest/test_timestretch.png");
  pagFile->replaceImage(0, pagImage);

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
  }
  EXPECT_EQ(errorMsg, "") << fileName << " frame fail";
}

/**
 * 用例描述: PAGTimeStrech渲染测试: Repeat模式-缩减
 */
PAG_TEST(PAGTimeStrechTest, Repeat_Shorten) {
  TimeStretchTest("resources/timestretch/repeat.pag", "Repeat_Shorten", 0.5);
}

/**
 * 用例描述: PAGTimeStrech渲染测试: Repeat模式-拉伸
 */
PAG_TEST(PAGTimeStrechTest, Repeat_Stretch) {
  TimeStretchTest("resources/timestretch/repeat.pag", "Repeat_Stretch", 2);
}

/**
 * 用例描述: PAGTimeStrech渲染测试-RepeatInverted-缩减
 */
PAG_TEST(PAGTimeStrechTest, RepeatInverted_Shorten) {
  TimeStretchTest("resources/timestretch/repeatInverted.pag", "RepeatInverted_Shorten", 0.5);
}

/**
 * 用例描述: PAGTimeStrech渲染测试-RepeatInverted-拉伸
 */
PAG_TEST(PAGTimeStrechTest, RepeatInverted_Stretch) {
  TimeStretchTest("resources/timestretch/repeatInverted.pag", "RepeatInverted_Stretch", 2);
}

/**
 * 用例描述: PAGTimeStrech渲染测试-Scale模式-缩减
 */
PAG_TEST(PAGTimeStrechTest, Scale_Shorten) {
  TimeStretchTest("resources/timestretch/scale.pag", "Scale_Shorten", 0.5);
}

/**
 * 用例描述: PAGTimeStrech渲染测试: Scale模式-拉伸
 */
PAG_TEST(PAGTimeStrechTest, Scale_Stretch) {
  TimeStretchTest("resources/timestretch/scale.pag", "Scale_Stretch", 2);
}

}  // namespace pag
