/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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

#include "PAGBenchmarkTask.h"

namespace pag {

PAGBenchmarkTask::PAGBenchmarkTask(std::shared_ptr<PAGFile> pagFile, const QString& filePath)
    : PAGPlayTask(std::move(pagFile), filePath) {
  openAfterExport = false;
}

void PAGBenchmarkTask::onBegin() {
  PAGPlayTask::onBegin();
  avgPerformanceData = std::make_shared<PerformanceData>();
  maxPerformanceData = std::make_shared<PerformanceData>();
  firstFramePerformanceData = std::make_shared<PerformanceData>();
}

int PAGBenchmarkTask::onFinish() {
  PAGPlayTask::onFinish();
  auto frames = currentFrame - 1;
  if (frames > 0) {
    avgPerformanceData->presentingTime /= frames;
    avgPerformanceData->renderingTime /= frames;
    avgPerformanceData->imageDecodingTime /= frames;
  }
  avgPerformanceData->graphicsMemory = CalculateGraphicsMemory(pagFile->getFile());
  return 0;
}

void PAGBenchmarkTask::onFrameFlush(double progress) {
  PAGPlayTask::onFrameFlush(progress);
  int64_t renderingTime = pagPlayer->renderingTime();
  int64_t presentingTime = pagPlayer->presentingTime();
  int64_t imageDecodingTime = pagPlayer->imageDecodingTime();
  if (currentFrame == 0) {
    firstFramePerformanceData->presentingTime = presentingTime;
    firstFramePerformanceData->renderingTime = renderingTime;
    firstFramePerformanceData->imageDecodingTime = imageDecodingTime;
  }
  avgPerformanceData->presentingTime += presentingTime;
  avgPerformanceData->renderingTime += renderingTime;
  avgPerformanceData->imageDecodingTime += imageDecodingTime;
  maxPerformanceData->presentingTime =
      std::max(maxPerformanceData->presentingTime, avgPerformanceData->presentingTime);
  maxPerformanceData->renderingTime =
      std::max(maxPerformanceData->renderingTime, avgPerformanceData->renderingTime);
  maxPerformanceData->imageDecodingTime =
      std::max(maxPerformanceData->imageDecodingTime, avgPerformanceData->imageDecodingTime);
}

}  // namespace pag