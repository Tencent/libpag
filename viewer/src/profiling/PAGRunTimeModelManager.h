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

#pragma once

#include <QObject>
#include "profiling/PAGFileInfoModel.h"
#include "profiling/PAGFrameDisplayInfoModel.h"

namespace pag {

class FrameTimeMetrics {
 public:
  FrameTimeMetrics(int64_t renderTime, int64_t presentTime, int64_t imageDecodeTime);

  int64_t renderTime = 0;
  int64_t presentTime = 0;
  int64_t imageDecodeTime = 0;
};

class PAGRunTimeModelManager : public QObject {
  Q_OBJECT
 public:
  explicit PAGRunTimeModelManager(QObject* parent = nullptr);

  Q_PROPERTY(QString totalFrame READ getTotalFrame NOTIFY totalFrameChanged)
  Q_PROPERTY(
      QString currentFrame READ getCurrentFrame WRITE setCurrentFrame NOTIFY currentFrameChanged)
  Q_PROPERTY(
      const PAGFileInfoModel* fileInfoModel READ getFileInfoModel NOTIFY fileInfoModelChanged)
  Q_PROPERTY(const PAGFrameDisplayInfoModel* frameDisplayInfoModel READ getFrameDisplayInfoModel
                 NOTIFY frameDisplayInfoModelChanged)

  auto getTotalFrame() const -> QString;
  auto getCurrentFrame() const -> QString;
  auto getFileInfoModel() const -> const PAGFileInfoModel*;
  auto getFrameDisplayInfoModel() const -> const PAGFrameDisplayInfoModel*;

  auto setCurrentFrame(const QString& currentFrame) -> void;

  Q_SIGNAL void totalFrameChanged();
  Q_SIGNAL void currentFrameChanged();
  Q_SIGNAL void fileInfoModelChanged();
  Q_SIGNAL void frameDisplayInfoModelChanged();
  Q_SIGNAL void dataChanged();

  Q_SLOT void updateData(int64_t currentFrame, int64_t renderTime, int64_t presentTime,
                         int64_t imageDecodeTime);

  auto resetFile(const std::shared_ptr<PAGFile>& pagFile, const std::string& filePath) -> void;

 private:
  auto updateFrameDisplayInfo(int64_t renderTime, int64_t presentTime, int64_t imageDecodeTime)
      -> void;

 private:
  int64_t totalFrame = -1;
  int64_t currentFrame = -1;
  PAGFileInfoModel fileInfoModel = {};
  PAGFrameDisplayInfoModel frameDisplayInfoModel = {};
  QVector<FrameTimeMetrics> frameTimeMetricsVector = {};
};
}  // namespace pag
