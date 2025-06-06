/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
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
#include "SnapshotCenter.h"

SnapshotCenter::SnapshotCenter(std::shared_ptr<AECompositionPanel> compositionPanel, QObject* parent)
    : compositionPanel(compositionPanel), QObject(parent) {
  compositionImageProvider = new CompositionImageProvider();
}

void SnapshotCenter::initTimer() {
  timer = new QTimer(this);
  connect(timer, &QTimer::timeout, this, &SnapshotCenter::update);
  timer->setInterval(intervalTime);
}

SnapshotCenter::~SnapshotCenter() noexcept {
  delete timer;
  timer = nullptr;
  delete compositionImageProvider;
  compositionImageProvider = nullptr;
}

void SnapshotCenter::setSize(int width, int height) {
  this->width = width;
  this->height = height;
}

void SnapshotCenter::getImageInProgress(int progress) {
  if (compositionPanel == nullptr) {
    return;
  }
  frameWaitQueue.push(progress);
  if (timer == nullptr) {
    initTimer();
    timer->start();
  }
}

void SnapshotCenter::setCompositionPanel(std::shared_ptr<AECompositionPanel> compositionPanel) {
  this->compositionPanel = std::move(compositionPanel);
}

void SnapshotCenter::update() {
  if (frameWaitQueue.empty()) {
    hitMissTime++;
    if (hitMissTime >= maxIdleTime / intervalTime) {
      timer->stop();
      disconnect(timer, &QTimer::timeout, this, &SnapshotCenter::update);
      delete timer;
      timer = nullptr;
      hitMissTime = 0;
      qDebug() << "idle too long, remove timer";
    }
    return;
  }

  int progress = frameWaitQueue.back();
  frameWaitQueue = std::queue<int>();
  QString frameId = QString::number(progress);
  frameId.append("_").append(compositionPanel->compositionName.c_str());
  if (progress != 1 && compositionImageProvider->isCacheFrame(frameId)) {
    qDebug() << "get cache image for frameId:" << frameId;
    Q_EMIT imageChange(frameId);
    return;
  }

  std::shared_ptr<uint8_t> rgbaData = compositionPanel->getPreviewImage(progress, width, height);
  QImage* imageData = new QImage(rgbaData.get(), width, height, QImage::Format_RGBA8888);
  compositionImageProvider->cacheFrame(frameId, imageData);
  qDebug() << "cache image for frameId:" << frameId << ", imageData:" << imageData;
  Q_EMIT imageChange(frameId);
}
