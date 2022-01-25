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

#ifdef SMOKE_TEST
#include <fstream>
#include <vector>
#include "TestUtils.h"
#include "base/utils/TimeUtil.h"
#include "framework/pag_test.h"
#include "framework/utils/PAGTestUtils.h"
#include "nlohmann/json.hpp"

namespace pag {
using nlohmann::json;

/**
 * 用例描述: smoke文件夹下文件是否渲染正常
 */
PAG_TEST(PAGSmokeTest, TestMD5) {
  json dumpJson;
  json compareJson;
  //这里需要生成冒烟测试的md5
  std::ifstream inputFile("../test/res/smoke_md5_dump.json");
  bool needCompare;
  if (inputFile) {
    needCompare = true;
    inputFile >> compareJson;
  }

  std::vector<std::string> files;
  GetAllPAGFiles("../resources/smoke", files);

  int size = static_cast<int>(files.size());
  for (int i = 0; i < size; i++) {
    auto fileName = files[i].substr(files[i].rfind("/") + 1, files[i].size());
    std::vector<std::string> compareVector;

    bool needCompareThis = false;
    if (needCompare && compareJson.contains(fileName) && compareJson[fileName].is_array()) {
      compareVector = compareJson[fileName].get<std::vector<std::string>>();
      needCompareThis = true;
    }

    auto TestPAGFile = PAGFile::Load(files[i]);
    ASSERT_NE(TestPAGFile, nullptr);
    auto pagSurface = PAGSurface::MakeOffscreen(TestPAGFile->width(), TestPAGFile->height());
    ASSERT_NE(pagSurface, nullptr);
    auto pagPlayer = std::make_shared<PAGPlayer>();
    pagPlayer->setSurface(pagSurface);
    pagPlayer->setComposition(TestPAGFile);

    Frame totalFrames = TimeToFrame(TestPAGFile->duration(), TestPAGFile->frameRate());
    Frame currentFrame = 0;

    std::vector<std::string> md5Vector;
    std::string errorMsg = "";

    bool status = true;
    while (currentFrame < totalFrames) {
      //添加0.1帧目的是保证progress不会由于精度问题帧数计算错误，frame应该使用totalFrames作为总体帧数。因为对于file来说总时长为[0,totalFrames],对应于[0,1]，因此归一化时，分母应该为totalFrames
      pagPlayer->setProgress((currentFrame + 0.1) * 1.0 / totalFrames);
      pagPlayer->flush();

      auto skImage = MakeSnapshot(pagSurface);
      std::string md5 = DumpMD5(skImage);
      md5Vector.push_back(md5);
      if (needCompareThis && compareVector[currentFrame] != md5) {
        errorMsg += (std::to_string(currentFrame) + ";");
        if (status) {
          std::string imagePath =
              "../test/out/smoke/" + fileName + "_" + std::to_string(currentFrame) + ".png";
          Trace(skImage, imagePath);
          status = false;
        }
      }
      currentFrame++;
    }
    EXPECT_EQ(errorMsg, "") << fileName << " frame fail";
    dumpJson[fileName] = md5Vector;
    EXPECT_EQ(errorMsg, "") << dumpJson;
  }
  std::ofstream outFile("../test/out/smoke/smoke_md5_dump.json");
  outFile << std::setw(4) << dumpJson << std::endl;
  outFile.close();
}

}  // namespace pag
#endif
