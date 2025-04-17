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
  FrameTimeMetrics();
  FrameTimeMetrics(const FrameTimeMetrics& data);
  FrameTimeMetrics(int renderTime, int presentTime, int imageDecodeTime);
  auto operator=(const FrameTimeMetrics& other) -> FrameTimeMetrics&;

 public:
  int renderTime{0};
  int presentTime{0};
  int imageDecodeTime{0};
};

class PAGRunTimeModelManager : public QObject {
  Q_OBJECT
 public:
  explicit PAGRunTimeModelManager(QObject* parent = nullptr);

  Q_PROPERTY(int totalFrame READ getTotalFrame NOTIFY totalFrameChanged)
  Q_PROPERTY(int currentFrame READ getCurrentFrame WRITE setCurrentFrame NOTIFY currentFrameChanged)
  Q_PROPERTY(
      const PAGFileInfoModel* fileInfoModel READ getFileInfoModel NOTIFY fileInfoModelChanged)
  Q_PROPERTY(const PAGFrameDisplayInfoModel* frameDisplayInfoModel READ getFrameDisplayInfoModel
                 NOTIFY frameDisplayInfoModelChanged)

  auto getTotalFrame() const -> int;
  auto getCurrentFrame() const -> int;
  auto getFileInfoModel() const -> const PAGFileInfoModel*;
  auto getFrameDisplayInfoModel() const -> const PAGFrameDisplayInfoModel*;

  auto setCurrentFrame(int currentFrame) -> void;

  Q_SIGNAL void totalFrameChanged();
  Q_SIGNAL void currentFrameChanged();
  Q_SIGNAL void fileInfoModelChanged();
  Q_SIGNAL void frameDisplayInfoModelChanged();
  Q_SIGNAL void dataChanged();

  Q_SLOT void updateData(int currentFrame, int renderTime, int presentTime, int imageDecodeTime);

  auto resetFile(const std::shared_ptr<PAGFile>& pagFile, const std::string& filePath) -> void;

 private:
  auto updateFrameDisplayInfo(int renderTime, int presentTime, int imageDecodeTime) -> void;

 private:
  int totalFrame{-1};
  int currentFrame{-1};
  PAGFileInfoModel fileInfoModel{};
  PAGFrameDisplayInfoModel frameDisplayInfoModel{};
  QMap<int, FrameTimeMetrics> frameTimeMetricsMap{};
};
}  // namespace pag
