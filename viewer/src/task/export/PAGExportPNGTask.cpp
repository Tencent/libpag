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

#include "task/export/PAGExportPNGTask.h"
#include <QImage>
#include <QOpenGLContext>
#include "utils/PAGFileUtils.h"

namespace pag {

PAGExportPNGTask::PAGExportPNGTask(std::shared_ptr<PAGFile>& pagFile, const QString& filePath,
                                   int exportFrame)
    : PAGPlayTask(pagFile, filePath), exportFrame(exportFrame) {
  QString lowerFilePath = filePath.toLower();
  if (!lowerFilePath.endsWith(".png")) {
    Utils::makeDir(filePath);
  }
}

auto PAGExportPNGTask::onFrameFlush(double progress) -> void {
  PAGPlayTask::onFrameFlush(progress);
  QString outPath;
  if (exportFrame >= 0) {
    outPath = filePath;
  } else {
    outPath = QString("%1/%2.png").arg(filePath).arg(currentFrame);
  }
  exportCurrentFrameAsPNG(outPath);
}

auto PAGExportPNGTask::isNeedRenderCurrentFrame() -> bool {
  if (exportFrame < 0) {
    return true;
  }
  return exportFrame == currentFrame;
}

auto PAGExportPNGTask::exportCurrentFrameAsPNG(const QString& outPath) -> void {
  context->makeCurrent(offscreenSurface.get());
  auto image = frameBuffer->toImage(false);
  bool result = image.save(outPath, "PNG");
  if (!result) {
    qDebug() << "Failed to save image to path: " << outPath;
  }
  context->doneCurrent();
}

}  // namespace pag