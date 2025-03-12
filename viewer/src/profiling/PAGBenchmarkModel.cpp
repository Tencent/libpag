#include "PAGBenchmarkModel.h"
#if defined(__APPLE__)
#include <dirent.h>
#endif
#include <QStandardPaths>
#include "utils/Time.h"

PAGBenchmarkModel::PAGBenchmarkModel(QObject *parent) : QObject(parent){

}

PAGBenchmarkModel::~PAGBenchmarkModel() {
  clearBenchmarkData();
}

auto PAGBenchmarkModel::onBenchmarkTaskComplete(QString filePath, int result) -> void {
  // TODO Implement the function
}

auto PAGBenchmarkModel::onBenchmarkFromQRCTaskComplete(QString filePath, int result) -> void {
  // TODO Implement the function
}

auto PAGBenchmarkModel::startBenchmark() -> bool {
  // TODO Implement the function

  return true;
}

auto PAGBenchmarkModel::startBenchmarkFromQRC(bool isAuto) -> bool {
  // TODO Implement the function

  return true;
}

auto PAGBenchmarkModel::getAllPAGFiles(std::string path) -> std::vector<std::string> {
  std::vector<std::string> files;

#if defined(__APPLE__)
  struct dirent *dirp;
  DIR* dir = opendir(path.c_str());
  std::string p;

  while ((dirp = readdir(dir)) != nullptr) {
    if (dirp->d_type == DT_REG) {
      std::string str(dirp->d_name);
      std::string::size_type idx = str.find(".pag");
      if (idx != std::string::npos) {
        files.push_back(p.assign(path).append("/").append(dirp->d_name));
      }
    }

    if (dirp->d_type == DT_DIR) {
      std::string str(dirp->d_name);
      if(str == "." || str == "..") {
        continue;
      }
      std::string dirString = path+"/"+str;
      auto results = getAllPAGFiles(dirString.c_str());
      files.insert(files.end(), results.begin(), results.end());
    }
  }

  closedir(dir);
#endif

  return files;
}

auto PAGBenchmarkModel::clearBenchmarkData() -> void {
  maxRenderingTimes.clear();
  avgRenderingTimes.clear();
  firstFrameRenderingTimes.clear();
  pagFiles.clear();
  // taskMap.clear();
  avgRenderingTimeMap.clear();
  callbackCount = 0;
  isAuto = false;

#if defined QT_DEBUG
  QString filePath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/performanceData.csv";
  outFile.open(filePath.toStdString(), std::ios::trunc);
  outFile.close();
#endif
}