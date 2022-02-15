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

#ifdef PERFORMANCE_TEST

#include <filesystem>
#include <fstream>
#include <vector>
#include "TestUtils.h"
#include "base/utils/GetTimer.h"
#include "base/utils/TimeUtil.h"
#include "framework/pag_test.h"
#include "framework/utils/PAGTestUtils.h"
#include "nlohmann/json.hpp"

namespace pag {
using nlohmann::json;

double NumberGap(int64_t num1, int64_t num2) {
  return (num2 - num1) * 1.0 / num1;
}

bool Accept(double result) {
  return result <= 0.05;
}

/**
 * 用例描述: 测试渲染性能
 */
PAG_TEST(PerformanceTest, TestRender) {
  json renderJson;
  json graphicsJson;

  json compareRenderJson;
  std::ifstream inputRenderFile("../test/baseline/PerformanceTest/compare_performance_render.json");
  bool needCompareRender = false;
  if (inputRenderFile) {
    needCompareRender = true;
    inputRenderFile >> compareRenderJson;
  }

  json compareGraphicsJson;
  std::ifstream inputGraphicsFile(
      "../test/baseline/PerformanceTest/compare_performance_graphics.json");
  bool needCompareGraphics = false;
  if (inputGraphicsFile) {
    needCompareGraphics = true;
    inputGraphicsFile >> compareGraphicsJson;
  }

  std::vector<std::string> files;
  GetAllPAGFiles("../resources/smoke", files);

  for (size_t i = 0; i < files.size(); i++) {
    auto fileName = files[i].substr(files[i].rfind('/') + 1, files[i].size());
    std::vector<std::string> compareRenderVector;
    std::vector<std::string> compareGraphicsVector;

    bool needCompareRenderThis = false;
    bool needCompareGraphicsThis = false;
    if (needCompareRender && compareRenderJson.contains(fileName) &&
        compareRenderJson[fileName].is_array()) {
      compareRenderVector = compareRenderJson[fileName].get<std::vector<std::string>>();
      needCompareRenderThis = true;
    }
    if (needCompareGraphics && compareGraphicsJson.contains(fileName) &&
        compareGraphicsJson[fileName].is_array()) {
      compareGraphicsVector = compareGraphicsJson[fileName].get<std::vector<std::string>>();
      needCompareGraphicsThis = true;
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

    std::vector<std::string> renderVector;
    std::vector<std::string> graphicsVector;
    std::string errorMsg;

    int64_t totalTime = 0;
    int64_t totalGraphics = 0;

    while (currentFrame < totalFrames) {
      pagPlayer->setProgress((currentFrame + 0.1) * 1.0 / totalFrames);
      int64_t frameTime = GetTimer();
      pagPlayer->flush();
      frameTime = GetTimer() - frameTime;

      int64_t graphicsMemory = pagPlayer->graphicsMemory();
      if (currentFrame > 0) {
        // 先不统计第0帧
        totalTime += frameTime;
        totalGraphics += graphicsMemory;
      }

      currentFrame++;
    }
    std::cout << "\n frameTime: " << totalTime / (totalFrames - 1)
              << " graphicsMemory: " << totalGraphics / (totalFrames - 1) << std::endl;

    int64_t avgTotalTime = totalTime / (totalFrames - 1);
    renderVector.push_back(std::to_string(avgTotalTime));
    int64_t avgTotalGraphics = totalGraphics / (totalFrames - 1);
    graphicsVector.push_back(std::to_string(avgTotalGraphics));

    double renderResult = NumberGap(std::stoll(compareRenderVector[i]), avgTotalTime);
    if (needCompareRenderThis && !Accept(renderResult)) {
      errorMsg += "frame" + std::to_string(currentFrame) +
                  " render out of time:" + std::to_string(renderResult);
      std::string imagePath =
          "../test/out/" + fileName + "_render_" + std::to_string(currentFrame) + ".png";
      Trace(MakeSnapshot(pagSurface), imagePath);
    }
    double graphicsResult = NumberGap(std::stoll(compareGraphicsVector[i]), avgTotalGraphics);
    if (needCompareGraphicsThis && !Accept(graphicsResult)) {
      errorMsg += "frame" + std::to_string(currentFrame) +
                  " graphics to much:" + std::to_string(graphicsResult);
      std::string imagePath =
          "../test/out/" + fileName + "_graphics_" + std::to_string(currentFrame) + ".png";
      Trace(MakeSnapshot(pagSurface), imagePath);
    }
    EXPECT_EQ(errorMsg, "") << fileName << " frame fail";
    renderJson[fileName] = renderVector;
    graphicsJson[fileName] = graphicsVector;
  }
  std::filesystem::path renderConfig("../test/out/PerformanceTest/performance_render.json");
  std::filesystem::create_directories(renderConfig.parent_path());
  std::ofstream outRenderFile(renderConfig);
  outRenderFile << std::setw(4) << renderJson << std::endl;
  outRenderFile.close();
  std::filesystem::path graphicsConfig("../test/out/PerformanceTest/performance_graphics.json");
  std::filesystem::create_directories(graphicsConfig.parent_path());
  std::ofstream outGraphicsFile(graphicsConfig);
  outGraphicsFile << std::setw(4) << graphicsJson << std::endl;
  outGraphicsFile.close();
}
}  // namespace pag
#endif