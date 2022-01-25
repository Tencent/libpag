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
#include <vector>
#include "TestUtils.h"
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

void initData() {
  std::ifstream inputFile("../test/res/compare_timestretch_md5.json");
  if (inputFile) {
    needCompare = true;
    inputFile >> compareJson;
    dumpJson = compareJson;
  }
}

void saveFile() {
  if (dumpStatus) {
    std::ofstream outFile("../test/out/compare_timestretch_md5.json");
    outFile << std::setw(4) << dumpJson << std::endl;
    outFile.close();
  }
}

void TimeStretchTest(std::string path, float scaleFactor) {
  std::vector<int> shortenArray = {0, 6, 30, 59};
  std::vector<int> stretchArray = {0, 12, 120, 239};

  auto fileName = path.substr(path.rfind("/") + 1, path.size());
  std::vector<std::string> compareVector;

  auto TestPAGFile = PAGFile::Load(path);
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

  bool needCompareThis = false;
  if (needCompare && compareJson.contains(fileName) && compareJson[fileName].is_array()) {
    compareVector = compareJson[fileName].get<std::vector<std::string>>();
    needCompareThis = true;
  }

  auto pagSurface = PAGSurface::MakeOffscreen(TestPAGFile->width(), TestPAGFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(TestPAGFile);

  Frame totalFrames = TimeToFrame(TestPAGFile->duration(), TestPAGFile->frameRate());

  std::vector<std::string> md5Vector;
  std::string errorMsg = "";

  auto pagImage = PAGImage::FromPath("../resources/apitest/test_timestretch.png");
  TestPAGFile->replaceImage(0, pagImage);

  bool status = true;
  int index = 0;
  for (const auto& currentFrame : array) {
    pagPlayer->setProgress((currentFrame + 0.1) * 1.0 / totalFrames);
    pagPlayer->getProgress();
    pagPlayer->flush();

    auto skImage = MakeSnapshot(pagSurface);

    std::string md5 = DumpMD5(skImage);
    md5Vector.push_back(md5);
    if (needCompareThis && compareVector[index] != md5) {
      errorMsg += (std::to_string(currentFrame) + ";");
      if (status) {
        std::string imagePath =
            "../test/out/" + fileName + "_" + std::to_string(currentFrame) + ".png";
        Trace(skImage, imagePath);
        status = false;
        dumpStatus = true;
      }
    }
    index++;
  }
  EXPECT_EQ(errorMsg, "") << fileName << " frame fail";
  dumpJson[fileName] = md5Vector;
}

/**
 * 用例描述: PAGTimeStrech渲染测试: Repeat模式-缩减
 */
PAG_TEST_F(PAGTimeStrechTest, Repeat_Shorten_TestMD5) {
  initData();
  TimeStretchTest("../resources/timestretch/repeat.pag", 0.5);
}

/**
 * 用例描述: PAGTimeStrech渲染测试: Repeat模式-拉伸
 */
PAG_TEST_F(PAGTimeStrechTest, Repeat_Stretch_TestMD5) {
  TimeStretchTest("../resources/timestretch/repeat.pag", 2);
}

/**
 * 用例描述: PAGTimeStrech渲染测试-RepeatInverted-缩减
 */
PAG_TEST_F(PAGTimeStrechTest, RepeatInverted_Shorten_TestMD5) {
  TimeStretchTest("../resources/timestretch/repeatInverted.pag", 0.5);
}

/**
 * 用例描述: PAGTimeStrech渲染测试-RepeatInverted-拉伸
 */
PAG_TEST_F(PAGTimeStrechTest, RepeatInverted_Stretch_TestMD5) {
  TimeStretchTest("../resources/timestretch/repeatInverted.pag", 2);
}

/**
 * 用例描述: PAGTimeStrech渲染测试-Scale模式-缩减
 */
PAG_TEST_F(PAGTimeStrechTest, Scale_Shorten_TestMD5) {
  TimeStretchTest("../resources/timestretch/scale.pag", 0.5);
}

/**
 * 用例描述: PAGTimeStrech渲染测试: Scale模式-拉伸
 */
PAG_TEST_F(PAGTimeStrechTest, Scale_Stretch_TestMD5) {
  TimeStretchTest("../resources/timestretch/scale.pag", 2);
  saveFile();
}

}  // namespace pag
