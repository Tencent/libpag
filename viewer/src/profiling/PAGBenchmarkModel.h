#ifndef PROFILING_PAG_BENCHMARK_MODEL_H_
#define PROFILING_PAG_BENCHMARK_MODEL_H_

#include <fstream>
#include <QFile>
#include <QObject>
#include <QSettings>
#include <pag/pag.h>

class PAGBenchmarkModel : public QObject {
  Q_OBJECT
 public:
  enum BENCHMARK_SCENE_TYPE{
    BENCHMARK_SCENE_TYPE_TEMPLATE,
    BENCHMARK_SCENE_TYPE_UI
  };
  Q_ENUMS(BENCHMARK_SCENE_TYPE)

  explicit PAGBenchmarkModel(QObject *parent = nullptr);
  ~PAGBenchmarkModel() override;

  Q_SIGNAL void benchmarkComplete(int templateAvgRenderingTime, int templateFirstFrameRenderingTime, int uiAvgRenderingTime, int uiFirstFrameRenderingTime, bool isAuto);

  Q_SLOT void onBenchmarkTaskComplete(QString filePath, int result);
  Q_SLOT void onBenchmarkFromQRCTaskComplete(QString filePath, int result);

  Q_INVOKABLE bool startBenchmark();
  Q_INVOKABLE bool startBenchmarkFromQRC(bool isAuto);

 private:
  auto getAllPAGFiles(std::string path) -> std::vector<std::string>;
  auto clearBenchmarkData() -> void;

 private:
  bool isAuto = false;
  int callbackCount = 0;
  int graphicsMemory = 0;
  std::ofstream outFile;
  std::vector<int> avgRenderingTimes;
  std::vector<int> maxRenderingTimes;
  std::vector<int> firstFrameRenderingTimes;
  std::vector<std::string> pagFiles;
  // TODO Store tasks
  std::map<std::string, int> avgRenderingTimeMap;
};

#endif // PROFILING_PAG_BENCHMARK_MODEL_H_
