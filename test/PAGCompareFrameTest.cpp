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

#ifndef SKIP_FRAME_COMPARE

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
  int64_t compareCost = 0;
  std::string performance;
};

class CompareFrameTask : public Executor {
 public:
  CompareFrameTask(const std::string& fileName, Frame currentFrame,
                   std::shared_ptr<PixelBuffer> pixelBuffer)
      : fileName(fileName), _currentFrame(currentFrame), pixelBuffer(std::move(pixelBuffer)) {
  }

  Frame currentFrame() const {
    return _currentFrame;
  }

  bool isSuccess() {
    return success;
  }

 private:
  const std::string fileName = "";
  Frame _currentFrame = 0;
  std::shared_ptr<PixelBuffer> pixelBuffer = nullptr;
  bool success = false;

  void execute() override {
    success = Baseline::Compare(
        pixelBuffer, "PAGCompareFrameTest/" + fileName + "/" + std::to_string(_currentFrame));
  }
};

void CompareFileFrames(Semaphore* semaphore, std::string pagPath) {
  auto timer = GetTimer();
  auto fileName = pagPath.substr(pagPath.rfind('/') + 1, pagPath.size());
  auto pagFile = PAGFile::Load(pagPath);
  ASSERT_NE(pagFile, nullptr);
  auto width = pagFile->width();
  auto height = pagFile->height();
  if (std::min(width, height) > 360) {
    if (width > height) {
      width =
          static_cast<int>(ceilf(static_cast<float>(width) * 360.0f / static_cast<float>(height)));
      height = 360;
    } else {
      height =
          static_cast<int>(ceilf(static_cast<float>(height) * 360.0f / static_cast<float>(width)));
      width = 360;
    }
  }
  auto pagSurface = PAGSurface::MakeOffscreen(width, height);
  ASSERT_NE(pagSurface, nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);
  Frame totalFrames = TimeToFrame(pagFile->duration(), pagFile->frameRate());
  Frame currentFrame = 0;
  std::string errorMsg;
  std::map<Frame, RenderCost> performanceMap = {};

  std::shared_ptr<PixelBuffer> currentSnapshot = nullptr;
  std::shared_ptr<Task> lastTask = nullptr;

  auto CompareFrame = [&](Frame currentFrame) {
    if (lastTask == nullptr) {
      return;
    }
    auto startTime = GetTimer();
    auto task = static_cast<CompareFrameTask*>(lastTask->wait());
    auto compareCost = GetTimer() - startTime;
    if (currentFrame == task->currentFrame()) {
      auto& cost = performanceMap[currentFrame];
      cost.compareCost = compareCost;
      cost.totalTime += compareCost;
    }
    if (!task->isSuccess()) {
      errorMsg += (std::to_string(currentFrame) + ";");
    }
    lastTask = nullptr;
  };

  while (currentFrame < totalFrames) {
    auto changed = pagPlayer->flush();
    if (changed) {
      RenderCost cost = {};
      auto starTime = GetTimer();
      currentSnapshot = MakeSnapshot(pagSurface);
      cost.readPixelsCost = GetTimer() - starTime;
      auto cache = pagPlayer->renderCache;
      cost.totalTime = cache->totalTime + cost.readPixelsCost;
      cost.performance = cache->getPerformanceString();
      performanceMap[currentFrame] = cost;
    }
    CompareFrame(currentFrame - 1);
    if (changed) {
      auto executor = new CompareFrameTask(fileName, currentFrame, currentSnapshot);
      lastTask = Task::Make(std::unique_ptr<CompareFrameTask>(executor));
      lastTask->run();
    }
    pagPlayer->nextFrame();
    currentFrame++;
  }
  CompareFrame(currentFrame - 1);

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
      LOGI("%4d | %6.1fms :%s %6.1fms[Pixels] %6.1fms[Compare]", item.first,
           renderCost.totalTime / 1000.0, renderCost.performance.c_str(),
           renderCost.readPixelsCost / 1000.0, renderCost.compareCost / 1000.0);
    }
  }
  EXPECT_EQ(errorMsg, "") << fileName << " frame fail";
  LOGI(
      "====================================== %s : %.2fs "
      "======================================\n",
      fileName.c_str(), cost);
  semaphore->signal();
}

/**
 * 用例描述: 校验 compare 文件夹中各个文件渲染结果
 */
PAG_TEST(PAGFrameCompareTest, RenderFiles) {
  std::vector<std::string> files;
  GetAllPAGFiles("../resources/compare", files);

  // 强制仅测试指定的文件
  //  files.clear();
  //  files.push_back("../resources/compare/vt_zhongyudd.pag");

  static const int MAX_THREADS = 6;
  Semaphore semaphore(MAX_THREADS);
  std::vector<std::thread> threads = {};
  for (auto& file : files) {
    semaphore.wait();
    threads.emplace_back(CompareFileFrames, &semaphore, file);
  }
  for (auto& thread : threads) {
    thread.join();
  }
}
}  // namespace pag
#endif
