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

#include "PAGTaskFactory.h"
#include <QDir>
#include <QUrl>
#include <QVariantMap>
#include "task/export/PAGExportAPNGTask.h"
#include "task/export/PAGExportPNGTask.h"
#include "task/profiling/PAGBenchmarkTask.h"
#include "task/profiling/PAGProfilingTask.h"

namespace pag {

PAGTask* PAGTaskFactory::createTask(PAGTaskType taskType, const QString& outPath,
                                    const QVariantMap& extraParams) {
  if (pagFile == nullptr) {
    return nullptr;
  }

  QString path = outPath;
  if (path.startsWith("file://")) {
    path = QUrl(path).toLocalFile();
  }

  switch (taskType) {
    case PAGTaskType::PAGTaskType_ExportPNG: {
      if (extraParams.contains("exportFrame")) {
        int exportFrame = extraParams.value("exportFrame").toInt();
        task = new PAGExportPNGTask(pagFile, path, exportFrame);
      } else {
        task = new PAGExportPNGTask(pagFile, path);
      }
      break;
    }
    case PAGTaskType::PAGTaskType_ExportAPNG: {
      task = new PAGExportAPNGTask(pagFile, path);
      break;
    }
    case PAGTaskType::PAGTaskType_Profiling: {
      task = new PAGProfilingTask(pagFile, path);
      break;
    }
    case PAGTaskType::PAGTaskType_Benchmark: {
      task = new PAGBenchmarkTask(pagFile, path);
      break;
    }
    default: {
      delete task;
      task = nullptr;
      break;
    }
  }

  return task;
}

void PAGTaskFactory::setFilePath(const std::string& filePath) {
  pagFile = PAGFile::Load(filePath);
}

}  // namespace pag
