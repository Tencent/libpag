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

#include "PAGRunTimeDataModel.h"
#include "base/utils/TimeUtil.h"
#include "pag/pag.h"

namespace pag {

FrameTimeMetrics::FrameTimeMetrics(int64_t renderTime, int64_t presentTime, int64_t imageDecodeTime)
    : renderTime(renderTime), presentTime(presentTime), imageDecodeTime(imageDecodeTime) {
}

PAGRunTimeDataModel::PAGRunTimeDataModel(QObject* parent) : QObject(parent) {
}

QString PAGRunTimeDataModel::getTotalFrame() const {
  return QString::number(totalFrame);
}

QString PAGRunTimeDataModel::getCurrentFrame() const {
  return QString::number(currentFrame);
}

int PAGRunTimeDataModel::getChartDataSize() const {
  return chartDataSize;
}

const PAGFileInfoModel* PAGRunTimeDataModel::getFileInfoModel() const {
  return &fileInfoModel;
}

const PAGChartDataModel* PAGRunTimeDataModel::getChartDataModel() const {
  return &chartDataModel;
}

const PAGFrameDisplayInfoModel* PAGRunTimeDataModel::getFrameDisplayInfoModel() const {
  return &frameDisplayInfoModel;
}

void PAGRunTimeDataModel::setCurrentFrame(const QString& currentFrame) {
  if (this->currentFrame == currentFrame.toLongLong()) {
    return;
  }
  this->currentFrame = currentFrame.toLongLong();
  Q_EMIT dataChanged();
}

void PAGRunTimeDataModel::setChartDataSize(int chartDataSize) {
  if (this->chartDataSize == chartDataSize) {
    return;
  }
  this->chartDataSize = chartDataSize;
  if (chartDataSize > 0) {
    refreshChartDataModel();
  }
}

void PAGRunTimeDataModel::updateData(int64_t currentFrame, int64_t renderTime, int64_t presentTime,
                                     int64_t imageDecodeTime) {
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
  updateChartData();
  Q_EMIT dataChanged();
}

void PAGRunTimeDataModel::setPAGFile(std::shared_ptr<PAGFile> pagFile) {
  totalFrame = TimeToFrame(pagFile->duration(), pagFile->frameRate());
  currentFrame = -1;
  chartDataModel.clearItems();
  frameTimeMetricsVector.resize(totalFrame, {0, 0, 0});
  frameTimeMetricsVector.squeeze();
  frameTimeMetricsVector.clear();
  fileInfoModel.setPAGFile(pagFile);
  updateFrameDisplayInfo(0, 0, 0);
  Q_EMIT dataChanged();
}

void PAGRunTimeDataModel::updateChartData() {
  if (totalFrame == 0) {
    return;
  }
  if (chartDataSize == 0) {
    return;
  }
  double ratio = static_cast<double>(chartDataSize) / totalFrame;
  auto startChartDataIndex = static_cast<int64_t>((currentFrame + 1) * ratio);
  auto startIndex =
      static_cast<int64_t>(std::floor(static_cast<double>(startChartDataIndex) / ratio));
  auto endIndex =
      static_cast<int64_t>(std::ceil(static_cast<double>((startChartDataIndex + 1)) / ratio));
  if (endIndex < (startIndex + 1)) {
    endIndex = startIndex + 1;
  }

  int64_t count = 0;
  int64_t renderTime = 0;
  int64_t presentTime = 0;
  int64_t imageDecodeTime = 0;
  for (int64_t i = startIndex; i < endIndex && i < frameTimeMetricsVector.size(); ++i) {
    renderTime += frameTimeMetricsVector[i].renderTime;
    presentTime += frameTimeMetricsVector[i].presentTime;
    imageDecodeTime += frameTimeMetricsVector[i].imageDecodeTime;
    count++;
  }

  if (count > 0) {
    renderTime /= count;
    presentTime /= count;
    imageDecodeTime /= count;
    int64_t endChartDataIndex = endIndex * ratio;
    PAGCharDataItem item;
    item.renderTime = renderTime;
    item.presentTime = presentTime;
    item.imageDecodeTime = imageDecodeTime;
    for (int64_t i = startChartDataIndex; i < endChartDataIndex; ++i) {
      chartDataModel.updateOrInsertItem(i, &item);
    }
  }
}

void PAGRunTimeDataModel::refreshChartDataModel() {
  int64_t maxIndex = frameTimeMetricsVector.size() - 1;
  int64_t lastChartDataIndex = 0;
  int64_t count = 0;
  int64_t renderTime = 0;
  int64_t presentTime = 0;
  int64_t imageDecodeTime = 0;
  PAGChartDataModel newChartDataModel;
  double ratio = static_cast<double>(chartDataSize) / totalFrame;

  for (int64_t i = 0; i <= maxIndex; i++) {
    int64_t currentChartDataIndex = std::floor(i * ratio);
    if (currentChartDataIndex != lastChartDataIndex && count != 0) {
      PAGCharDataItem item;
      item.renderTime = renderTime / count;
      item.presentTime = presentTime / count;
      item.imageDecodeTime = imageDecodeTime / count;
      while (lastChartDataIndex != currentChartDataIndex) {
        newChartDataModel.updateOrInsertItem(lastChartDataIndex, &item);
        ++lastChartDataIndex;
      }
      renderTime = 0;
      presentTime = 0;
      imageDecodeTime = 0;
      count = 0;
    }

    auto& item = frameTimeMetricsVector[i];
    renderTime += item.renderTime;
    presentTime += item.presentTime;
    imageDecodeTime += item.imageDecodeTime;
    count++;
  }
  if (count > 0) {
    int currentChartDataIndex = std::floor(maxIndex * ratio);
    currentChartDataIndex = currentChartDataIndex == lastChartDataIndex ? lastChartDataIndex + 1
                                                                        : currentChartDataIndex;
    PAGCharDataItem item;
    item.renderTime = renderTime / count;
    item.presentTime = presentTime / count;
    item.imageDecodeTime = imageDecodeTime / count;
    while (lastChartDataIndex != currentChartDataIndex) {
      newChartDataModel.updateOrInsertItem(lastChartDataIndex, &item);
      ++lastChartDataIndex;
    }
  }
  chartDataModel.resetItems(&newChartDataModel);
}

void PAGRunTimeDataModel::updateFrameDisplayInfo(int64_t renderTime, int64_t presentTime,
                                                 int64_t imageDecodeTime) {
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
