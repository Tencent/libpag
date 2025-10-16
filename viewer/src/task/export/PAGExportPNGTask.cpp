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

#include "task/export/PAGExportPNGTask.h"
#include <QImage>
#include <QOpenGLContext>
#include "utils/FileUtils.h"

namespace pag {

const std::string ExportPNGFileSuffix = "png";

PAGExportPNGTask::PAGExportPNGTask(std::shared_ptr<PAGFile> pagFile, const QString& pngfilePath,
                                   int exportFrame)
    : PAGPlayTask(std::move(pagFile), pngfilePath), exportFrame(exportFrame) {
}

PAGExportPNGTask::PAGExportPNGTask(std::shared_ptr<PAGFile> pagFile,
                                   const QString& pngSequenceDirPath)
    : PAGPlayTask(std::move(pagFile), pngSequenceDirPath) {
  Utils::MakeDir(pngSequenceDirPath);
}

void PAGExportPNGTask::onFrameFlush(double progress) {
  PAGPlayTask::onFrameFlush(progress);
  QString outPath;
  if (exportFrame >= 0) {
    outPath = filePath;
  } else {
    outPath = QString("%1/%2.png").arg(filePath).arg(currentFrame);
  }
  exportCurrentFrameAsPNG(outPath);
}

bool PAGExportPNGTask::isNeedRenderCurrentFrame() {
  if (exportFrame < 0) {
    return true;
  }
  return exportFrame == currentFrame;
}

void PAGExportPNGTask::exportCurrentFrameAsPNG(const QString& outPath) {
  context->makeCurrent(offscreenSurface.get());
  auto image = frameBuffer->toImage(false);
  bool result = image.save(outPath, ExportPNGFileSuffix.c_str());
  if (!result) {
    qDebug() << "Failed to save image to path: " << outPath;
  }
  context->doneCurrent();
}

}  // namespace pag