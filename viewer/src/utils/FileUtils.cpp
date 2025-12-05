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

#include "FileUtils.h"
#include <QDesktopServices>
#include <QDir>
#include <QUrl>
#include "PAGUtilsImpl.h"

namespace pag::Utils {

void OpenInFinder(const QString& path, bool select) {
  QFileInfo fileInfo(path);
  if (!fileInfo.exists()) {
    return;
  }

  if (select) {
    openFileInFinder(fileInfo);
  } else {
    QDesktopServices::openUrl(QUrl::fromLocalFile(fileInfo.absoluteFilePath()));
  }
}

bool RemoveFile(const QString& path) {
  QFile file(path);
  if (!file.exists()) {
    return true;
  }
  return file.remove();
}

bool DeleteDir(const QString& path) {
  QDir dir(path);
  if (!dir.exists()) {
    return true;
  }

  dir.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
  QFileInfoList fileList = dir.entryInfoList();
  for (auto& fileInfo : fileList) {
    if (fileInfo.isFile()) {
      if (!fileInfo.dir().remove(fileInfo.fileName())) {
        return false;
      }
    } else {
      if (!DeleteDir(fileInfo.absoluteFilePath())) {
        return false;
      }
    }
  }

  return dir.rmdir(path);
}

bool MakeDir(const QString& path, bool isDir) {
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

bool WriteFileToDisk(std::shared_ptr<File> file, const QString& filePath) {
  auto encodeByteData = pag::Codec::Encode(std::move(file));
  if (encodeByteData == nullptr) {
    return false;
  }

  return WriteDataToDisk(filePath, encodeByteData->data(), encodeByteData->length());
}

bool WriteDataToDisk(const QString& filePath, const void* data, size_t length) {
  FILE* fp = fopen(filePath.toStdString().c_str(), "wb");
  if (fp == nullptr) {
    return false;
  }

  size_t result = fwrite(data, 1, length, fp);
  if (result != length) {
    fclose(fp);
    return false;
  }

  result = fclose(fp);
  if (result != 0) {
    return false;
  }
  return true;
}

bool DuplicateFile(const QString& sourcePath, const QString& targetPath, bool overwrite) {
  if (sourcePath.isEmpty() || targetPath.isEmpty()) {
    return false;
  }

  QFile sourceFile(sourcePath);
  if (!sourceFile.exists()) {
    return false;
  }

  QString targetDir = QFileInfo(targetPath).absolutePath();
  if (!MakeDir(targetDir)) {
    return false;
  }

  if (QFile::exists(targetPath)) {
    if (!overwrite) {
      return false;
    }
    QFile::remove(targetPath);
  }

  return sourceFile.copy(targetPath);
}

bool CopyDir(const QString& sourceDir, const QString& targetDir, bool overwrite) {
  if (sourceDir.isEmpty() || targetDir.isEmpty()) {
    return false;
  }

  QDir source(sourceDir);
  if (!source.exists()) {
    return false;
  }

  if (!MakeDir(targetDir)) {
    return false;
  }

  QStringList files = source.entryList(QDir::Files);
  for (const QString& file : files) {
    QString srcPath = sourceDir + "/" + file;
    QString dstPath = targetDir + "/" + file;
    if (!DuplicateFile(srcPath, dstPath, overwrite)) {
      return false;
    }
  }

  QStringList dirs = source.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
  for (const QString& dir : dirs) {
    QString srcPath = sourceDir + "/" + dir;
    QString dstPath = targetDir + "/" + dir;
    if (!CopyDir(srcPath, dstPath, overwrite)) {
      return false;
    }
  }

  return true;
}

}  // namespace pag::Utils
