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
#ifndef SNAPSHOTCENTER_H
#define SNAPSHOTCENTER_H
#include <QtCore/QObject>
#include <queue>
#include "CompositionImageProvider.h"
#include "src/utils/Panels/AECompositionPanel.h"
#include <QtCore/QTimer>
#include <QtCore/QDebug>
class SnapshotCenter : public QObject {
  Q_OBJECT
 public:
  SnapshotCenter(std::shared_ptr<AECompositionPanel> compositionPanel, QObject* parent = nullptr);
  ~SnapshotCenter();
  Q_INVOKABLE void setSize(int width, int height);
  Q_INVOKABLE void getImageInProgress(int progress);
  Q_SIGNAL void imageChange(QString frameId);
  CompositionImageProvider* compositionImageProvider;

  void setCompositionPanel(std::shared_ptr<AECompositionPanel> compositionPanel);

protected:
  Q_SLOT void update();

private:
  int width;
  int height;
  int intervalTime = 48;
  int maxIdleTime = 1000;
  int hitMissTime = 0;
  std::shared_ptr<AECompositionPanel> compositionPanel;
  std::queue<int> frameWaitQueue;
  QTimer* timer = nullptr;
  void initTimer();
};

#endif //SNAPSHOTCENTER_H
