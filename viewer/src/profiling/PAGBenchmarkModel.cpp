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

#include "PAGBenchmarkModel.h"
#include <QFile>
#include "pag/pag.h"
#include "task/profiling/PAGBenchmarkTask.h"

namespace pag {

PAGBenchmarkModel::PAGBenchmarkModel(QObject* parent) : QObject(parent) {
}

bool PAGBenchmarkModel::startBenchmarkOnTemplate(bool isAuto) {
  this->isAuto = isAuto;
  QString templatePAGFilePath = ":pag/performance-baseline.pag";
  QString templatePNGFilePath = ":images/window-icon.png";

  QFile pagQFile(templatePAGFilePath);
  QFile pngQFile(templatePNGFilePath);

  if (!pagQFile.open(QIODevice::ReadOnly)) {
    return false;
  }
  if (!pngQFile.open(QIODevice::ReadOnly)) {
    return false;
  }

  QByteArray pagByteArray = pagQFile.readAll();
  QByteArray pngByteArray = pngQFile.readAll();
  auto pagFile = PAGFile::Load(pagByteArray.data(), pagByteArray.size());
  if (pagFile == nullptr) {
    return false;
  }

  for (int i = 0; i < pagFile->numImages(); i++) {
    auto layer = pagFile->getLayersByEditableIndex(i, pag::LayerType::Image);
    if (layer.empty()) {
      continue;
    }
    pagFile->replaceImage(i, PAGImage::FromBytes(pngByteArray.data(), pngByteArray.size()));
  }

  auto task = std::make_shared<PAGBenchmarkTask>(pagFile, templatePAGFilePath);
  connect(task.get(), &PAGBenchmarkTask::taskFinished, this,
          &PAGBenchmarkModel::onBenchmarkOnTemplateFinished);
  taskMap.insert(templatePAGFilePath, task);
  taskMap[templatePAGFilePath]->start();
  return true;
}

void PAGBenchmarkModel::onBenchmarkOnTemplateFinished(int, QString filePath) {
  if (taskMap.find(filePath) == taskMap.end()) {
    return;
  }
  auto task = taskMap[filePath].get();
  int64_t templateAvgRenderingTime = task->avgPerformanceData->renderingTime;
  int64_t templateFirstFrameRenderingTime = task->firstFramePerformanceData->renderingTime;

  Q_EMIT benchmarkComplete(isAuto, QString::number(templateAvgRenderingTime),
                           QString::number(templateFirstFrameRenderingTime));

  taskMap.remove(filePath);
}

}  // namespace pag
