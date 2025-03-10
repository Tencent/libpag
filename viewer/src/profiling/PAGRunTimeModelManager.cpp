#include "PAGRunTimeModelManager.h"
#include "utils/Time.h"

RunTimeData::RunTimeData() = default;

RunTimeData::RunTimeData(const RunTimeData& data) {
  renderingTime = data.renderingTime;
  presentingTime = data.presentingTime;
  imageDecodingTime = data.imageDecodingTime;
}

RunTimeData::RunTimeData(int renderingTime, int presentingTime, int imageDecodingTime)
 : renderingTime(renderingTime), presentingTime(presentingTime), imageDecodingTime(imageDecodingTime){

}

PAGRunTimeModelManager::PAGRunTimeModelManager() = default;

auto PAGRunTimeModelManager::getChartSize() const -> float {
  return static_cast<float>(chartSize);
}

auto PAGRunTimeModelManager::getTotalFrame() const -> int {
  return totalFrame;
}

auto PAGRunTimeModelManager::getCurrentFrame() const -> int {
  return currentFrame;
}

auto PAGRunTimeModelManager::getDataModel() const -> PAGRunTimeDataModel* {
  return const_cast<PAGRunTimeDataModel *>(&dataModel);
}

auto PAGRunTimeModelManager::getChartModel() const -> PAGRunTimeChartModel* {
  return const_cast<PAGRunTimeChartModel*>(&chartModel);
}

auto PAGRunTimeModelManager::setChartSize(float size) -> void {
  if (chartSize == size) {
    return;
  }
  chartSize = size;
  if (chartSize > 0) {
    refreshChartModel();
  }
}

auto PAGRunTimeModelManager::setCurrentFrame(int currentFrame) -> void {
  if (this->currentFrame == currentFrame) {
    return;
  }
  this->currentFrame = currentFrame;
  Q_EMIT dataChange();
}

void PAGRunTimeModelManager::updateDisplayData(int frame, int renderingTime, int presentingTime, int imageDecodingTime) {
  if (currentFrame == frame) {
    return;
  }
  currentFrame = frame;
  dataMap[frame] = RunTimeData(renderingTime, presentingTime, imageDecodingTime);
  updateRunTimeDataModel(renderingTime, presentingTime, imageDecodingTime);
  updateChartModel();
  Q_EMIT dataChange();
}

void PAGRunTimeModelManager::resetFile(const std::shared_ptr<pag::PAGFile>& pagFile, std::string filePath) {
  totalFrame = Utils::timeToFrame(pagFile->duration(), pagFile->frameRate());
  dataMap.clear();
  chartModel.resetFile(pagFile->getFile());
  currentFrame = -1;
  PAGRunTimeData render(0, 0, 0, "Render", "#0096D8");
  PAGRunTimeData decode(0, 0, 0, "Image", "#74AD59");
  PAGRunTimeData present(0, 0, 0, "Present", "DDB259");
  dataModel.updateRunTimeData(render, present, decode);
  Q_EMIT dataChange();
}

auto PAGRunTimeModelManager::updateChartModel() -> void {
  int count = 0;
  int decode = 0;
  int render = 0;
  int present = 0;
  int index = static_cast<int>(std::floor(currentFrame * chartSize / totalFrame));
  int start = index * totalFrame / chartSize;
  int end = static_cast<int>(std::ceil((index + 1) * totalFrame / chartSize));
  if (end < start + 1) {
    end = start + 1;
  }
  for (int i = start; i < end; ++i) {
    if (dataMap.contains(i)) {
      auto const item = dataMap[i];
      render += item.renderingTime;
      decode += item.imageDecodingTime;
      present += item.presentingTime;
      count ++;
    }
  }
  if (count > 0) {
    PAGChartData data(decode / count, render / count, present / count);
    int endIndex = end * chartSize / totalFrame;
    for (int i = index ; i < endIndex; i ++) {
      chartModel.updateColumnItem(&data, i);
    }
  }
}

auto PAGRunTimeModelManager::refreshChartModel() -> void {
  if (totalFrame == 0) {
    return;
  }
  int maxKey = 0;
  auto keys = dataMap.keys();
  for (int & key : keys) {
      maxKey = maxKey > key ? maxKey : key;
  }

  int count = 0;
  int decode = 0;
  int render = 0;
  int present = 0;
  int lastChartIndex = 0;
  PAGRunTimeChartModel newModel;
  for (int i = 0; i <= maxKey; i++) {
    int currentChartIndex = static_cast<int>(std::floor(i * chartSize / totalFrame));
    if (lastChartIndex != currentChartIndex) {
      if (count > 0) {
        PAGChartData data(decode / count, render / count, present / count);
        while (lastChartIndex != currentChartIndex) {
            newModel.updateColumnItem(&data, lastChartIndex);
            lastChartIndex++;
        }
        count = 0;
        decode = 0;
        render = 0;
        present = 0;
      }
    }
    if (dataMap.contains(i)) {
      auto item = dataMap[i];
      count ++;
      decode += item.imageDecodingTime;
      render += item.renderingTime;
      present += item.presentingTime;
    }
  }
  if (count != 0) {
    int currentChartIndex = static_cast<int>(std::floor((maxKey + 1) * chartSize / totalFrame));
    if (currentChartIndex == lastChartIndex) {
      currentChartIndex = lastChartIndex + 1;
    }
    if (lastChartIndex != currentChartIndex) {
      if (count > 0) {
        PAGChartData data(decode / count, render / count, present / count);
        while (lastChartIndex != currentChartIndex) {
          newModel.updateColumnItem(&data, lastChartIndex);
          lastChartIndex++;
        }
      }
    }
  }
  int index = static_cast<int>(std::floor(currentFrame * chartSize / totalFrame));
  int start = index * totalFrame / chartSize;
  int end = static_cast<int>(std::ceil((index + 1) * totalFrame / chartSize));
  if (end < start + 1) {
      end = start + 1;
  }
  int endIndex = end * chartSize / totalFrame;
  newModel.setIndex(endIndex);
  chartModel.resetColumns(&newModel);
}

auto PAGRunTimeModelManager::updateRunTimeDataModel(int renderingTime, int presentingTime, int imageDecodingTime) -> void {
  int size = static_cast<int>(dataMap.size());
  int pTotal = 0, rTotal = 0, dTotal = 0;
  int pMax = 0, rMax = 0, dMax = 0;
  auto iter = dataMap.begin();
  while (iter != dataMap.end()) {
    pTotal += iter->presentingTime;
    rTotal += iter->renderingTime;
    dTotal += iter->imageDecodingTime;
    pMax = iter->presentingTime > pMax ? iter->presentingTime : pMax;
    rMax = iter->renderingTime > rMax ? iter->renderingTime : rMax;
    dMax = iter->imageDecodingTime > dMax ? iter->imageDecodingTime : dMax;
    ++iter;
  }
  PAGRunTimeData render(rTotal / size, rMax, renderingTime, "Render", "#0096D8");
  PAGRunTimeData present(pTotal / size, pMax, presentingTime, "Present", "#DDB259");
  PAGRunTimeData decode(dTotal / size, dMax, imageDecodingTime, "Image", "#74AD59");
  dataModel.updateRunTimeData(render, present, decode);
}
