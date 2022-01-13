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

#ifndef SKIP_FILE_MD5

#include <fstream>
#include <map>
#include <thread>
#include <vector>
#include "TestUtils.h"
#include "base/utils/GetTimer.h"
#include "base/utils/Task.h"
#include "base/utils/TimeUtil.h"
#include "framework/pag_test.h"
#include "framework/utils/PAGTestUtils.h"
#include "framework/utils/Semaphore.h"
#include "nlohmann/json.hpp"
#include "rendering/caches/RenderCache.h"

namespace pag {
using nlohmann::json;

static constexpr bool PrintPerformance = false;

struct RenderCost {
  int64_t totalTime = 0;
  int64_t readPixelsCost = 0;
  int64_t md5Cost = 0;
  std::string performance;
};

class MD5Task : public Executor {
 public:
  MD5Task(Frame currentFrame, Bitmap bitmap)
      : _currentFrame(currentFrame), _bitmap(std::move(bitmap)) {
  }

  Frame currentFrame() const {
    return _currentFrame;
  }

  Bitmap bitmap() const {
    return _bitmap;
  }

  std::string md5() {
    return _md5;
  }

 private:
  Frame _currentFrame = 0;
  Bitmap _bitmap = {};
  std::string _md5;

  void execute() override {
    _md5 = DumpMD5(_bitmap);
  }
};

void DumpFileMD5(Semaphore* semaphore, std::string pagPath, json* compareJson, json* dumpJson) {
  auto timer = GetTimer();
  auto fileName = pagPath.substr(pagPath.rfind('/') + 1, pagPath.size());
  std::vector<std::string> compare = {};
  std::vector<std::string> output = {};
  if (compareJson->contains(fileName) && (*compareJson)[fileName].is_array()) {
    compare = (*compareJson)[fileName].get<std::vector<std::string>>();
  }
  auto pagFile = PAGFile::Load(pagPath);
  ASSERT_NE(pagFile, nullptr);
  auto pagSurface = PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);

  Frame totalFrames = TimeToFrame(pagFile->duration(), pagFile->frameRate());
  Frame currentFrame = 0;
  std::string errorMsg;
  std::map<Frame, RenderCost> performanceMap = {};

  bool status = true;
  Bitmap currentBitmap = {};
  std::shared_ptr<Task> lastMD5 = nullptr;

  auto CompareMD5 = [&](Frame currentFrame) {
    auto startTime = GetTimer();
    auto md5Task = static_cast<MD5Task*>(lastMD5->wait());
    auto md5 = md5Task->md5();
    auto md5Cost = GetTimer() - startTime;
    if (currentFrame == md5Task->currentFrame()) {
      auto& cost = performanceMap[currentFrame];
      cost.md5Cost = md5Cost;
      cost.totalTime += md5Cost;
    }
    output.push_back(md5);
    if (!compare.empty() && compare[currentFrame] != md5) {
      errorMsg += (std::to_string(currentFrame) + ";");
      if (status) {
        auto imagePath = "../test/out/" + fileName + "_" + std::to_string(currentFrame) + ".png";
        Trace(md5Task->bitmap(), imagePath);
        status = false;
      }
    }
  };

  while (currentFrame < totalFrames) {
    auto changed = pagPlayer->flush();
    if (changed) {
      RenderCost cost = {};
      auto starTime = GetTimer();
      currentBitmap = MakeSnapshot(pagSurface);
      cost.readPixelsCost = GetTimer() - starTime;
      auto cache = pagPlayer->renderCache;
      cost.totalTime = cache->totalTime + cost.readPixelsCost;
      cost.performance = cache->getPerformanceString();
      performanceMap[currentFrame] = cost;
    }
    if (currentFrame > 0) {
      CompareMD5(currentFrame - 1);
    }
    if (changed) {
      auto executor = new MD5Task(currentFrame, currentBitmap);
      lastMD5 = Task::Make(std::unique_ptr<MD5Task>(executor));
      lastMD5->run();
    }
    pagPlayer->nextFrame();
    currentFrame++;
  }
  CompareMD5(currentFrame - 1);

  auto cost = static_cast<float>(GetTimer() - timer) / 1000000;
  if (PrintPerformance) {
    LOGI(
        "========================================== %s : start"
        " ==========================================",
        fileName.c_str());
    for (auto& item : performanceMap) {
      //      if (item.second.totalTime < 10000) {  // 小于10ms不打印
      //        continue;
      //      }
      auto& renderCost = item.second;
      LOGI("%4d | %6.1fms :%s %6.1fms[Pixels] %6.1fms[MD5]", item.first,
           renderCost.totalTime / 1000.0, renderCost.performance.c_str(),
           renderCost.readPixelsCost / 1000.0, renderCost.md5Cost / 1000.0);
    }
  }
  EXPECT_EQ(errorMsg, "") << fileName << " frame fail";
  LOGI(
      "====================================== %s : %.2fs "
      "======================================\n",
      fileName.c_str(), cost);
  (*dumpJson)[fileName] = output;
  semaphore->signal();
}

/**
 * 用例描述: 校验md5文件夹中各个文件渲染结果
 */
PAG_TEST(SimplePAGMD5Test, TestMD5) {
  json dumpJson;
  json compareJson;
  std::ifstream inputFile("../test/res/compare_md5_dump.json");
  if (inputFile) {
    inputFile >> compareJson;
  }
  std::vector<std::string> files;
  GetAllPAGFiles("../resources/md5", files);

  // 强制仅测试指定文件的MD5
  //  files.clear();
  //  files.push_back("../resources/md5/vt_zhongyudd.pag");

  static const int MAX_THREADS = 6;
  Semaphore semaphore(MAX_THREADS);
  std::vector<std::thread> threads = {};
  for (auto& file : files) {
    semaphore.wait();
    threads.emplace_back(DumpFileMD5, &semaphore, file, &compareJson, &dumpJson);
  }
  for (auto& thread : threads) {
    thread.join();
  }

  std::ofstream outFile("../test/out/compare_md5_dump.json");
  outFile << std::setw(4) << dumpJson << std::endl;
  outFile.close();
}
}  // namespace pag
#endif
