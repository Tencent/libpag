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

#pragma once


#include <QList>

namespace pag {

enum class FileOpResult {
  Success,
  FileNotFound,
  AccessDenied,
  Failed
};

struct FileOperation {
  enum class Type { CopyFiles, CopyDirectory, DeleteFiles, DeleteDirectory, CreateDir };

  Type type;
  QString source;
  QString destination;

  static FileOperation copyFile(const QString& src, const QString& dst);
  static FileOperation copyDirectory(const QString& src, const QString& dst);
  static FileOperation deleteFile(const QString& path);
  static FileOperation deleteDirectory(const QString& path);
  static FileOperation createDirectory(const QString& path);
};

class FileOperations {
 public:
  static FileOpResult copyFile(const QString& source, const QString& destination);
  static FileOpResult copyDirectory(const QString& source, const QString& destination);
  static FileOpResult deleteFile(const QString& path);
  static FileOpResult deleteDirectory(const QString& path);
  static FileOpResult createDirectory(const QString& path);

  static FileOpResult executeBatch(const QList<FileOperation>& operations);
  static bool executeBatchWithPrivileges(const QList<FileOperation>& operations);

  static bool requiresElevation(const QString& path);
  static FileOpResult fromWindowsError(unsigned long errorCode);

 private:
  static QString generateBatchScript(const QList<FileOperation>& operations);
  static FileOpResult copyDirectoryRecursive(const QString& source, const QString& destination);
};

}  // namespace pag