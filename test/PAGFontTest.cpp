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
#include "base/utils/TimeUtil.h"
#include "framework/pag_test.h"
#include "framework/utils/PAGTestUtils.h"
#include "nlohmann/json.hpp"

namespace pag {
using nlohmann::json;

/**
 * 用例描述: 字体相关功能测试
 */
PAG_TEST(PAGFontTest, TestFont) {
  json dumpJson;
  json compareJson;
  std::ifstream inputFile("../test/res/compare_font_md5.json");
  bool needCompare = false;
  if (inputFile) {
    needCompare = true;
    inputFile >> compareJson;
  }

  std::vector<std::string> compareVector;
  std::string fileName = "test_font";
  if (needCompare && compareJson.contains(fileName) && compareJson[fileName].is_array()) {
    compareVector = compareJson[fileName].get<std::vector<std::string>>();
  }

  PAGFont::RegisterFont("../resources/apitest/AdobeHeitiStd.ttc", 0);
  PAGFont::RegisterFont("../resources/apitest/Kai.ttc", 0);
  PAGFont::RegisterFont("../resources/apitest/TTTGBMedium.ttc", 0);

  auto TestPAGFile = PAGFile::Load("../resources/apitest/test_font.pag");
  ASSERT_NE(TestPAGFile, nullptr);
  auto pagSurface = PAGSurface::MakeOffscreen(TestPAGFile->width(), TestPAGFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(TestPAGFile);

  Frame totalFrames = TimeToFrame(TestPAGFile->duration(), TestPAGFile->frameRate());
  Frame currentFrame = 0;

  std::vector<std::string> md5Vector;
  std::string errorMsg;

  bool status = true;
  while (currentFrame < totalFrames) {
    //添加0.1帧目的是保证progress不会由于精度问题帧数计算错误，frame应该使用totalFrames作为总体帧数。因为对于file来说总时长为[0,totalFrames],对应于[0,1]，因此归一化时，分母应该为totalFrames
    pagPlayer->setProgress((currentFrame + 0.1) * 1.0 / totalFrames);
    pagPlayer->flush();

    auto skImage = MakeSnapshot(pagSurface);

    std::string md5 = DumpMD5(skImage);
    md5Vector.push_back(md5);
    if (needCompare && compareVector[currentFrame] != md5) {
      errorMsg += (std::to_string(currentFrame) + ";");
      if (status) {
        std::string imagePath =
            "../test/out/" + fileName + "_" + std::to_string(currentFrame) + ".png";
        Trace(skImage, imagePath);
        status = false;
      }
    }
    currentFrame++;
  }
  EXPECT_EQ(errorMsg, "") << fileName << " frame fail";
  dumpJson[fileName] = md5Vector;
  std::ofstream outFile("../test/out/compare_font_md5.json");
  outFile << std::setw(4) << dumpJson << std::endl;
  outFile.close();
}

}  // namespace pag
