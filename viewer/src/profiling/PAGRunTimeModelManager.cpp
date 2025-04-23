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

FrameTimeMetrics::FrameTimeMetrics(int64_t renderTime, int64_t presentTime,
                                   int64_t imageDecodeTime) {
  this->renderTime = renderTime;
  this->presentTime = presentTime;
  this->imageDecodeTime = imageDecodeTime;
}

PAGRunTimeModelManager::PAGRunTimeModelManager(QObject* parent) : QObject(parent) {
}

auto PAGRunTimeModelManager::getTotalFrame() const -> QString {
  return QString::number(totalFrame);
}

auto PAGRunTimeModelManager::getCurrentFrame() const -> QString {
  return QString::number(currentFrame);
}

auto PAGRunTimeModelManager::getFileInfoModel() const -> const PAGFileInfoModel* {
  return &fileInfoModel;
}

auto PAGRunTimeModelManager::getFrameDisplayInfoModel() const -> const PAGFrameDisplayInfoModel* {
  return &frameDisplayInfoModel;
}

auto PAGRunTimeModelManager::setCurrentFrame(const QString& currentFrame) -> void {
  if (this->currentFrame == currentFrame.toLongLong()) {
    return;
  }
  this->currentFrame = currentFrame.toLongLong();
  Q_EMIT dataChanged();
}

auto PAGRunTimeModelManager::updateData(int64_t currentFrame, int64_t renderTime,
                                        int64_t presentTime, int64_t imageDecodeTime) -> void {
  if (this->currentFrame == currentFrame) {
    return;
  }
  this->currentFrame = currentFrame;
  if (currentFrame >= frameTimeMetricsVector.size()) {
    frameTimeMetricsVector.push_back(FrameTimeMetrics(renderTime, presentTime, imageDecodeTime));
  } else {
    frameTimeMetricsVector[currentFrame] =
        FrameTimeMetrics(renderTime, presentTime, imageDecodeTime);
  }
  updateFrameDisplayInfo(renderTime, presentTime, imageDecodeTime);
  Q_EMIT dataChanged();
}

auto PAGRunTimeModelManager::resetFile(const std::shared_ptr<PAGFile>& pagFile,
                                       const std::string& filePath) -> void {
  totalFrame = TimeToFrame(pagFile->duration(), pagFile->frameRate());
  currentFrame = -1;
  frameTimeMetricsVector.resize(totalFrame);
  frameTimeMetricsVector.squeeze();
  frameTimeMetricsVector.clear();
  fileInfoModel.resetFile(pagFile, filePath);
  updateFrameDisplayInfo(0, 0, 0);
  Q_EMIT dataChanged();
}

auto PAGRunTimeModelManager::updateFrameDisplayInfo(int64_t renderTime, int64_t presentTime,
                                                    int64_t imageDecodeTime) -> void {
  int64_t renderAvg = 0;
  int64_t renderMax = 0;
  int64_t renderTotal = 0;
  int64_t presentAvg = 0;
  int64_t presentMax = 0;
  int64_t presentTotal = 0;
  int64_t imageDecodeAvg = 0;
  int64_t imageDecodeMax = 0;
  int64_t imageDecodeTotal = 0;
  int64_t size = static_cast<int64_t>(frameTimeMetricsVector.size());

  if (size > 0) {
    for (auto& item : frameTimeMetricsVector) {
      renderTotal += item.renderTime;
      renderMax = std::max(item.renderTime, renderMax);
      presentTotal += item.presentTime;
      presentMax = std::max(item.presentTime, presentMax);
      imageDecodeTotal += item.imageDecodeTime;
      imageDecodeMax = std::max(item.imageDecodeTime, imageDecodeMax);
    }
    renderAvg = renderTotal / size;
    presentAvg = presentTotal / size;
    imageDecodeAvg = imageDecodeTotal / size;
  }

  FrameDisplayInfo renderInfo("Render", "#0096D8", renderTime, renderAvg, renderMax);
  FrameDisplayInfo presentInfo("Present", "#DDB259", presentTime, presentAvg, presentMax);
  FrameDisplayInfo imageDecodeInfo("Image", "#74AD59", imageDecodeTime, imageDecodeAvg,
                                   imageDecodeMax);
  frameDisplayInfoModel.updateData(renderInfo, presentInfo, imageDecodeInfo);
}

}  // namespace pag