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

#include "PAGProfilingTask.h"
#include "utils/FileUtils.h"

namespace pag {

PAGProfilingTask::PAGProfilingTask(std::shared_ptr<PAGFile> pagFile, const QString& filePath)
    : PAGPlayTask(std::move(pagFile), filePath) {
  openAfterExport = false;
}

auto PAGProfilingTask::onBegin() -> void {
  PAGPlayTask::onBegin();
  performanceData = std::make_shared<PerformanceData>();
}

auto PAGProfilingTask::onFinish() -> int {
  PAGPlayTask::onFinish();
  if (currentFrame > 0) {
    performanceData->presentingTime /= currentFrame;
    performanceData->renderingTime /= currentFrame;
    performanceData->imageDecodingTime /= currentFrame;
  }
  performanceData->graphicsMemory = CalculateGraphicsMemory(pagFile->getFile());
  auto byteData = pag::Codec::Encode(pagFile->getFile(), performanceData);
  if (byteData != nullptr || byteData->length() > 0) {
    auto outFilePath = filePath;
    if (!outFilePath.endsWith(".pag")) {
      outFilePath.append(".pag");
    }
    Utils::WriteDataToDisk(outFilePath, byteData->data(), byteData->length());
    Utils::OpenInFinder(outFilePath);
    return 0;
  }
  return 1;
}

auto PAGProfilingTask::onFrameFlush(double progress) -> void {
  PAGPlayTask::onFrameFlush(progress);
  if (currentFrame == 0) {
    return;
  }
  performanceData->renderingTime += pagPlayer->renderingTime();
  performanceData->presentingTime += pagPlayer->presentingTime();
  performanceData->imageDecodingTime += pagPlayer->imageDecodingTime();
}

}  // namespace pag
