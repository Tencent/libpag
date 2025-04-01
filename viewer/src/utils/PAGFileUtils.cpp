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

#include "utils/PAGFileUtils.h"
#include <QDesktopServices>
#include <QDir>
#include <QUrl>
#include "PAGUtilsImpl.h"

namespace pag::Utils {

auto openInFinder(const QString& path, bool select) -> void {
  QFileInfo fileInfo(path);
  if (fileInfo.exists() == false) {
    return;
  }

  if (select) {
    openFileInFinder(fileInfo);
  } else {
    QDesktopServices::openUrl(QUrl::fromLocalFile(fileInfo.absoluteFilePath()));
  }
}

auto deleteFile(const QString& path) -> bool {
  QFile file(path);
  if (!file.exists()) {
    return true;
  }
  return file.remove();
}

auto deleteDir(const QString& path) -> bool {
  QDir dir(path);
  if (!dir.exists()) {
    return true;
  }

  dir.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
  QFileInfoList fileList = dir.entryInfoList();
  for (auto& fileInfo : fileList) {
    if (fileInfo.isFile()) {
      if (!fileInfo.dir().remove(fileInfo.fileName())) {
        return false;
      }
    } else {
      if (!deleteDir(fileInfo.absoluteFilePath())) {
        return false;
      }
    }
  }

  return dir.rmdir(path);
}

auto makeDir(const QString& path, bool isDir) -> bool {
  QString dirPath;
  QFileInfo fileInfo(path);
  if (isDir) {
    dirPath = path;
  } else {
    dirPath = fileInfo.absolutePath();
  }
  QDir dir(dirPath);
  if (dir.exists()) {
    return true;
  }
  return dir.mkpath(dirPath);
}

}  // namespace pag::Utils
