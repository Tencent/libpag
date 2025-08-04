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

#include "FileReporter.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/layers/PAGStage.h"

namespace pag {

int64_t GetAverage(int64_t totalData, int count) {
  if (totalData == 0 || count == 0) {
    return 0;
  }
  return totalData / count;
}

std::unique_ptr<FileReporter> FileReporter::Make(std::shared_ptr<PAGLayer> pagLayer) {
  FileReporter* reporter = nullptr;
  while (pagLayer) {
    auto file = pagLayer->getFile();
    if (file) {
      reporter = new FileReporter(file.get());
      break;
    }
    if (pagLayer->layerType() != LayerType::PreCompose ||
        std::static_pointer_cast<PAGComposition>(pagLayer)->layers.empty()) {
      break;
    }
    pagLayer = std::static_pointer_cast<PAGComposition>(pagLayer)->layers.front();
  }
  return std::unique_ptr<FileReporter>(reporter);
}

FileReporter::FileReporter(File* file) {
  setFileInfo(file);
}

FileReporter::~FileReporter() {
  reportData();
}

void FileReporter::setFileInfo(File* file) {
  auto frameRate = static_cast<int>(file->frameRate());
  std::string separator = "|";
  pagInfoString =
      file->path + separator + std::to_string(frameRate) + separator +
      std::to_string(file->duration()) + separator + std::to_string(file->width()) + separator +
      std::to_string(file->height()) + separator + std::to_string(file->numLayers()) + separator +
      std::to_string(file->numVideos()) + separator + std::to_string(file->tagLevel()) + separator;
}

void FileReporter::reportData() {
  std::unordered_map<std::string, std::string> reportInfos;
  reportInfos.insert(
      std::make_pair("presentFirstFrameTime", std::to_string(presentFirstFrameTime)));
  reportInfos.insert(std::make_pair("presentMaxTime", std::to_string(presentMaxTime)));
  reportInfos.insert(std::make_pair("presentAverageTime",
                                    std::to_string(GetAverage(presentTotalTime, flushCount - 1))));
  reportInfos.insert(std::make_pair("renderFirstFrameTime", std::to_string(renderFirstFrameTime)));
  reportInfos.insert(std::make_pair("renderMaxTime", std::to_string(renderMaxTime)));
  reportInfos.insert(std::make_pair("renderAverageTime",
                                    std::to_string(GetAverage(renderTotalTime, flushCount - 1))));
  reportInfos.insert(std::make_pair("flushFirstFrameTime", std::to_string(flushFirstFrameTime)));
  reportInfos.insert(std::make_pair("flushMaxTime", std::to_string(flushMaxTime)));
  reportInfos.insert(std::make_pair("flushAverageTime",
                                    std::to_string(GetAverage(flushTotalTime, flushCount - 1))));

  reportInfos.insert(std::make_pair("imageDecodingMaxTime", std::to_string(imageDecodingMaxTime)));
  reportInfos.insert(
      std::make_pair("hardwareDecodingInitialTime", std::to_string(hardwareDecodingInitialTime)));
  reportInfos.insert(
      std::make_pair("hardwareDecodingMaxTime", std::to_string(hardwareDecodingMaxTime)));
  reportInfos.insert(
      std::make_pair("hardwareDecodingAverageTime",
                     std::to_string(GetAverage(hardwareDecodingTotalTime, hardwareDecodingCount))));
  reportInfos.insert(
      std::make_pair("softwareDecodingInitialTime", std::to_string(softwareDecodingInitialTime)));
  reportInfos.insert(
      std::make_pair("softwareDecodingMaxTime", std::to_string(softwareDecodingMaxTime)));
  reportInfos.insert(
      std::make_pair("softwareDecodingAverageTime",
                     std::to_string(GetAverage(softwareDecodingTotalTime, softwareDecodingCount))));
  reportInfos.insert(std::make_pair("graphicsMemoryMax", std::to_string(graphicsMemoryMax)));
  reportInfos.insert(std::make_pair("graphicsMemoryAverage",
                                    std::to_string(GetAverage(graphicsMemoryTotal, flushCount))));
  reportInfos.insert(std::make_pair("flushCount", std::to_string(flushCount)));
  reportInfos.insert(std::make_pair("pagInfo", pagInfoString));
  reportInfos.insert(std::make_pair("event", "pag_monitor"));
  reportInfos.insert(std::make_pair("version", PAG::SDKVersion()));
  reportInfos.insert(std::make_pair("usability", "1"));
}

static int64_t GetOldRenderTime(RenderCache* cache) {
  // 性能统计增加了数据字段，部分含义发生了改变，这里拼装出原先的
  // renderingTime，确保数据上报结果跟之前一致。
  return cache->totalTime - cache->presentingTime;
}

void FileReporter::recordPerformance(RenderCache* cache) {
  flushCount++;

  if (presentFirstFrameTime == 0) {
    presentFirstFrameTime = cache->presentingTime;
  } else {
    presentMaxTime = std::max(presentMaxTime, cache->presentingTime);
    presentTotalTime += cache->presentingTime;
  }

  if (renderFirstFrameTime == 0) {
    renderFirstFrameTime = GetOldRenderTime(cache);
  } else {
    renderMaxTime = std::max(renderMaxTime, GetOldRenderTime(cache));
    renderTotalTime += GetOldRenderTime(cache);
  }

  if (flushFirstFrameTime == 0) {
    flushFirstFrameTime = presentFirstFrameTime + renderFirstFrameTime;
  } else {
    flushMaxTime = std::max(flushMaxTime, cache->presentingTime + GetOldRenderTime(cache));
    flushTotalTime = presentTotalTime + renderTotalTime;
  }

  if (cache->softwareDecodingTime > 0) {
    softwareDecodingMaxTime = std::max(softwareDecodingMaxTime, cache->softwareDecodingTime);
    softwareDecodingTotalTime += cache->softwareDecodingTime;
    softwareDecodingCount++;
  }

  if (cache->hardwareDecodingTime > 0) {
    hardwareDecodingMaxTime = std::max(hardwareDecodingMaxTime, cache->hardwareDecodingTime);
    hardwareDecodingTotalTime += cache->hardwareDecodingTime;
    hardwareDecodingCount++;
  }

  imageDecodingMaxTime = std::max(imageDecodingMaxTime, cache->imageDecodingTime);
  graphicsMemoryMax = std::max(graphicsMemoryMax, cache->memoryUsage());
  graphicsMemoryTotal += cache->memoryUsage();
  softwareDecodingInitialTime =
      std::max(softwareDecodingInitialTime, cache->softwareDecodingInitialTime);
  hardwareDecodingInitialTime =
      std::max(hardwareDecodingInitialTime, cache->hardwareDecodingInitialTime);
}

}  // namespace pag
