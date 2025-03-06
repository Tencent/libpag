#ifndef PROFILING_PAG_RUNTIME_MODEL_MANAGER_H_
#define PROFILING_PAG_RUNTIME_MODEL_MANAGER_H_

#include <QObject>
#include <pag/pag.h>
#include "PAGRunTimeDataModel.h"
#include "PAGRunTimeChartModel.h"

class RunTimeData {
 public:
  RunTimeData();
  RunTimeData(const RunTimeData& data);
  RunTimeData(int renderingTime, int presentingTime, int imageDecodingTime);

 public:
  int renderingTime = 0;
  int presentingTime = 0;
  int imageDecodingTime = 0;
};

class PAGRunTimeModelManager : public QObject {
  Q_OBJECT
 public:
  PAGRunTimeModelManager();

  Q_PROPERTY(float                 chartSize    READ getChartSize    WRITE setChartSize)
  Q_PROPERTY(int                   totalFrame   READ getTotalFrame)
  Q_PROPERTY(int                   currentFrame READ getCurrentFrame WRITE setCurrentFrame)
  Q_PROPERTY(PAGRunTimeDataModel*  dataModel    READ getDataModel                             NOTIFY dataModelChange)
  Q_PROPERTY(PAGRunTimeChartModel* chartModel   READ getChartModel)

  auto getChartSize() const -> float;
  auto getTotalFrame() const -> int;
  auto getCurrentFrame() const -> int;
  auto getDataModel() const -> PAGRunTimeDataModel*;
  auto getChartModel() const -> PAGRunTimeChartModel*;

  auto setChartSize(float size) -> void;
  auto setCurrentFrame(int currentFrame) -> void;

  Q_SIGNAL void dataChange();
  Q_SIGNAL void dataModelChange();

  Q_SLOT void updateDisplayData(int frame, int renderingTime, int presentingTime, int imageDecodingTime);

  void resetFile(const std::shared_ptr<pag::PAGFile>& pagFile, std::string filePath);

 private:
  auto updateChartModel() -> void;
  auto refreshChartModel() -> void;
  auto updateRunTimeDataModel(int renderingTime, int presentingTime, int imageDecodingTime) -> void;

 private:
  int chartSize = 0;
  int totalFrame = 0;
  int currentFrame = 0;
  PAGRunTimeDataModel dataModel;
  PAGRunTimeChartModel chartModel;
  QMap<int, RunTimeData> dataMap;
};

#endif // PROFILING_PAG_RUNTIME_MODEL_MANAGER_H_