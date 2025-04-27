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

#include "PAGExportAPNGTask.h"
#include <QDebug>
#include "utils/FileUtils.h"
#include "utils/Utils.h"

namespace pag {

PAGExportAPNGTask::PAGExportAPNGTask(std::shared_ptr<PAGFile>& pagFile, const QString& apngFilePath,
                                     const QString& pngFilePath)
    : PAGExportPNGTask(pagFile, pngFilePath), apngFilePath(apngFilePath) {
  openAfterExport = false;
}

void PAGExportAPNGTask::onFrameFlush(double progress) {
  PAGExportPNGTask::onFrameFlush(progress * 0.9);
}

int PAGExportAPNGTask::onFinish() {
  std::string outPath = apngFilePath.toStdString();
  std::string firstPNGPath = QString("%1/1.png").arg(filePath).toStdString();
  int frameRate = static_cast<int>(pagFile->frameRate());
  Utils::exportAPNGFromPNGSequence(outPath, firstPNGPath, frameRate);
  PAGExportPNGTask::onFrameFlush(1.0);
  Utils::deleteDir(filePath);
  Utils::openInFinder(apngFilePath, true);
  return PAGExportPNGTask::onFinish();
}

}  // namespace pag