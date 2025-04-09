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

#include "PAGRunTimeModelManager.h"
#include "base/utils/TimeUtil.h"
#include "pag/pag.h"

namespace pag {

FrameTimeMetrics::FrameTimeMetrics() = default;

FrameTimeMetrics::FrameTimeMetrics(const FrameTimeMetrics& data) {
  renderTime = data.renderTime;
  presentTime = data.presentTime;
  imageDecodeTime = data.imageDecodeTime;
}

FrameTimeMetrics::FrameTimeMetrics(int renderTime, int presentTime, int imageDecodeTime) {
  this->renderTime = renderTime;
  this->presentTime = presentTime;
  this->imageDecodeTime = imageDecodeTime;
}

auto FrameTimeMetrics::operator=(const FrameTimeMetrics& other) -> FrameTimeMetrics& = default;

PAGRunTimeModelManager::PAGRunTimeModelManager(QObject* parent) : QObject(parent) {
}

auto PAGRunTimeModelManager::getTotalFrame() const -> int {
  return totalFrame;
}

auto PAGRunTimeModelManager::getCurrentFrame() const -> int {
  return currentFrame;
}

auto PAGRunTimeModelManager::getFileInfoModel() const -> PAGFileInfoModel* {
  return const_cast<PAGFileInfoModel*>(&fileInfoModel);
}

auto PAGRunTimeModelManager::getFrameDisplayInfoModel() const -> PAGFrameDisplayInfoModel* {
  return const_cast<PAGFrameDisplayInfoModel*>(&frameDisplayInfoModel);
}

auto PAGRunTimeModelManager::setCurrentFrame(int currentFrame) -> void {
  if (this->currentFrame == currentFrame) {
    return;
  }
  this->currentFrame = currentFrame;
  Q_EMIT dataChanged();
}

auto PAGRunTimeModelManager::updateData(int currentFrame, int renderTime, int presentTime,
                                        int imageDecodeTime) -> void {
  if (this->currentFrame == currentFrame) {
    return;
  }
  this->currentFrame = currentFrame;
  frameTimeMetricsMap[currentFrame] = FrameTimeMetrics(renderTime, presentTime, imageDecodeTime);
  updateFrameDisplayInfo(renderTime, presentTime, imageDecodeTime);
  Q_EMIT dataChanged();
}

auto PAGRunTimeModelManager::resetFile(const std::shared_ptr<PAGFile>& pagFile,
                                       std::string filePath) -> void {
  totalFrame = static_cast<int>(TimeToFrame(pagFile->duration(), pagFile->frameRate()));
  currentFrame = -1;
  frameTimeMetricsMap.clear();
  fileInfoModel.resetFile(pagFile, filePath);
  updateFrameDisplayInfo(0, 0, 0);
  Q_EMIT dataChanged();
}

auto PAGRunTimeModelManager::updateFrameDisplayInfo(int renderTime, int presentTime,
                                                    int imageDecodeTime) -> void {
  int renderAvg = 0;
  int renderMax = 0;
  int renderTotal = 0;
  int presentAvg = 0;
  int presentMax = 0;
  int presentTotal = 0;
  int imageDecodeAvg = 0;
  int imageDecodeMax = 0;
  int imageDecodeTotal = 0;
  int size = static_cast<int>(frameTimeMetricsMap.size());
  size = size > 0 ? size : 1;

  auto iter = frameTimeMetricsMap.begin();
  while (iter != frameTimeMetricsMap.end()) {
    renderTotal += iter->renderTime;
    renderMax = std::max(iter->renderTime, renderMax);
    presentTotal += iter->presentTime;
    presentMax = std::max(iter->presentTime, presentMax);
    imageDecodeTotal += iter->imageDecodeTime;
    imageDecodeMax = std::max(iter->imageDecodeTime, imageDecodeMax);
    ++iter;
  }

  renderAvg = renderTotal / size;
  presentAvg = presentTotal / size;
  imageDecodeAvg = imageDecodeTotal / size;

  FrameDisplayInfo renderInfo("Render", "#0096D8", renderTime, renderAvg, renderMax);
  FrameDisplayInfo presentInfo("Present", "#DDB259", presentTime, presentAvg, presentMax);
  FrameDisplayInfo imageDecodeInfo("Image", "#74AD59", imageDecodeTime, imageDecodeAvg,
                                   imageDecodeMax);
  frameDisplayInfoModel.updateData(renderInfo, presentInfo, imageDecodeInfo);
}

}  // namespace pag