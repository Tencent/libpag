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

#pragma once

#include <QObject>
#include "pagx/PAGXDocument.h"
#include "profiling/PAGChartDataModel.h"
#include "profiling/PAGFileInfoModel.h"
#include "profiling/PAGFrameDisplayInfoModel.h"
#include "profiling/PAGNodeStatsModel.h"

namespace pag {

class FrameTimeMetrics {
 public:
  FrameTimeMetrics(int64_t renderTime, int64_t presentTime, int64_t imageDecodeTime);

  int64_t renderTime = 0;
  int64_t presentTime = 0;
  int64_t imageDecodeTime = 0;
};

class PAGRunTimeDataModel : public QObject {
  Q_OBJECT
 public:
  explicit PAGRunTimeDataModel(QObject* parent = nullptr);

  Q_PROPERTY(QString totalFrame READ getTotalFrame NOTIFY totalFrameChanged)
  Q_PROPERTY(
      QString currentFrame READ getCurrentFrame WRITE setCurrentFrame NOTIFY currentFrameChanged)
  Q_PROPERTY(int chartDataSize READ getChartDataSize WRITE setChartDataSize)
  Q_PROPERTY(
      const PAGFileInfoModel* fileInfoModel READ getFileInfoModel NOTIFY fileInfoModelChanged)
  Q_PROPERTY(const PAGChartDataModel* chartDataModel READ getChartDataModel NOTIFY dataChanged)
  Q_PROPERTY(const PAGFrameDisplayInfoModel* frameDisplayInfoModel READ getFrameDisplayInfoModel
                 NOTIFY frameDisplayInfoModelChanged)
  Q_PROPERTY(
      const PAGNodeStatsModel* nodeStatsModel READ getNodeStatsModel NOTIFY nodeStatsModelChanged)
  Q_PROPERTY(bool isPAGX READ isPAGX NOTIFY isPAGXChanged)

  QString getTotalFrame() const;
  QString getCurrentFrame() const;
  int getChartDataSize() const;
  const PAGFileInfoModel* getFileInfoModel() const;
  const PAGChartDataModel* getChartDataModel() const;
  const PAGFrameDisplayInfoModel* getFrameDisplayInfoModel() const;
  const PAGNodeStatsModel* getNodeStatsModel() const;
  bool isPAGX() const;

  void setCurrentFrame(const QString& currentFrame);
  void setChartDataSize(int chartDataSize);

  Q_SIGNAL void totalFrameChanged();
  Q_SIGNAL void currentFrameChanged();
  Q_SIGNAL void fileInfoModelChanged();
  Q_SIGNAL void frameDisplayInfoModelChanged();
  Q_SIGNAL void nodeStatsModelChanged();
  Q_SIGNAL void isPAGXChanged();
  Q_SIGNAL void dataChanged();

  Q_SLOT void updateData(int64_t currentFrame, int64_t renderTime, int64_t presentTime,
                         int64_t imageDecodeTime);
  Q_SLOT void updatePAGXRenderTime(int64_t renderTime, int64_t imageTime, int64_t presentTime);

  void setPAGFile(std::shared_ptr<PAGFile> pagFile);
  void setPAGXDocument(std::shared_ptr<pagx::PAGXDocument> pagxDocument);

 private:
  void updateChartData();
  void refreshChartDataModel();
  void updateFrameDisplayInfo(int64_t renderTime, int64_t presentTime, int64_t imageDecodeTime);

 private:
  int64_t totalFrame = -1;
  int64_t currentFrame = -1;
  int64_t lastUpdatedFrame = -1;
  int chartDataSize = 0;
  bool pagxMode = false;
  PAGFileInfoModel fileInfoModel = {};
  PAGChartDataModel chartDataModel = {};
  PAGFrameDisplayInfoModel frameDisplayInfoModel = {};
  PAGNodeStatsModel nodeStatsModel = {};
  QVector<FrameTimeMetrics> frameTimeMetricsVector = {};
};
}  // namespace pag
