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

#include "FileOperations.h"
#include <shlobj.h>
#include <windows.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>

namespace pag {

FileOperation FileOperation::copyFile(const QString& src, const QString& dst) {
  return {Type::CopyFiles, src, dst};
}

FileOperation FileOperation::copyDirectory(const QString& src, const QString& dst) {
  return {Type::CopyDirectory, src, dst};
}

FileOperation FileOperation::deleteFile(const QString& path) {
  return {Type::DeleteFiles, path, QString()};
}

FileOperation FileOperation::deleteDirectory(const QString& path) {
  return {Type::DeleteDirectory, path, QString()};
}

FileOperation FileOperation::createDirectory(const QString& path) {
  return {Type::CreateDir, QString(), path};
}

FileOpResult FileOperations::fromWindowsError(unsigned long errorCode) {
  switch (errorCode) {
    case ERROR_SUCCESS:
      return FileOpResult::Success;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
      return FileOpResult::FileNotFound;
    case ERROR_ACCESS_DENIED:
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
      return FileOpResult::AccessDenied;
    default:
      return FileOpResult::Failed;
  }
}

FileOpResult FileOperations::copyFile(const QString& source, const QString& destination) {
  if (!QFile::exists(source)) {
    return FileOpResult::FileNotFound;
  }

  std::wstring srcW = source.toStdWString();
  std::wstring dstW = destination.toStdWString();

  if (CopyFileW(srcW.c_str(), dstW.c_str(), FALSE)) {
    return FileOpResult::Success;
  }

  return fromWindowsError(GetLastError());
}

FileOpResult FileOperations::copyDirectoryRecursive(const QString& source,
                                                    const QString& destination) {
  QDir srcDir(source);
  if (!srcDir.exists()) {
    return FileOpResult::FileNotFound;
  }

  QDir dstDir(destination);
  if (!dstDir.exists()) {
    auto result = createDirectory(destination);
    if (result != FileOpResult::Success) {
      return result;
    }
  }

  QFileInfoList entries = srcDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
  for (const QFileInfo& entry : entries) {
    QString srcPath = entry.absoluteFilePath();
    QString dstPath = destination + "/" + entry.fileName();

    if (entry.isDir()) {
      auto result = copyDirectoryRecursive(srcPath, dstPath);
      if (result != FileOpResult::Success) {
        return result;
      }
    } else {
      auto result = copyFile(srcPath, dstPath);
      if (result != FileOpResult::Success) {
        return result;
      }
    }
  }

  return FileOpResult::Success;
}

FileOpResult FileOperations::copyDirectory(const QString& source, const QString& destination) {
  return copyDirectoryRecursive(source, destination);
}

FileOpResult FileOperations::deleteFile(const QString& path) {
  if (!QFile::exists(path)) {
    return FileOpResult::Success;
  }

  std::wstring pathW = path.toStdWString();
  if (DeleteFileW(pathW.c_str())) {
    return FileOpResult::Success;
  }

  return fromWindowsError(GetLastError());
}

FileOpResult FileOperations::deleteDirectory(const QString& path) {
  QDir dir(path);
  if (!dir.exists()) {
    return FileOpResult::Success;
  }

  QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
  for (const QFileInfo& entry : entries) {
    QString entryPath = entry.absoluteFilePath();
    FileOpResult result;
    if (entry.isDir()) {
      result = deleteDirectory(entryPath);
    } else {
      result = deleteFile(entryPath);
    }
    if (result != FileOpResult::Success) {
      return result;
    }
  }

  std::wstring pathW = path.toStdWString();
  if (RemoveDirectoryW(pathW.c_str())) {
    return FileOpResult::Success;
  }

  return fromWindowsError(GetLastError());
}

FileOpResult FileOperations::createDirectory(const QString& path) {
  if (QDir(path).exists()) {
    return FileOpResult::Success;
  }

  QDir dir;
  if (dir.mkpath(path)) {
    return FileOpResult::Success;
  }

  return FileOpResult::Failed;
}

bool FileOperations::requiresElevation(const QString& path) {
  QString testPath = path;
  QFileInfo info(path);

  if (!info.exists()) {
    QDir dir(path);
    while (!dir.exists() && !dir.isRoot()) {
      dir.cdUp();
    }
    testPath = dir.absolutePath();
  }

  QString testFile = testPath + "/__pag_permission_test__";
  std::wstring testFileW = testFile.toStdWString();

  HANDLE hFile = CreateFileW(testFileW.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                             FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);

  if (hFile != INVALID_HANDLE_VALUE) {
    CloseHandle(hFile);
    return false;
  }

  return GetLastError() == ERROR_ACCESS_DENIED;
}

FileOpResult FileOperations::executeBatch(const QList<FileOperation>& operations) {
  for (const auto& op : operations) {
    FileOpResult result = FileOpResult::Success;

    switch (op.type) {
      case FileOperation::Type::CopyFiles:
        result = copyFile(op.source, op.destination);
        break;
      case FileOperation::Type::CopyDirectory:
        result = copyDirectory(op.source, op.destination);
        break;
      case FileOperation::Type::DeleteFiles:
        result = deleteFile(op.source);
        break;
      case FileOperation::Type::DeleteDirectory:
        result = deleteDirectory(op.source);
        break;
      case FileOperation::Type::CreateDir:
        result = createDirectory(op.destination);
        break;
    }

    if (result != FileOpResult::Success) {
      return result;
    }
  }

  return FileOpResult::Success;
}

QString FileOperations::generateBatchScript(const QList<FileOperation>& operations) {
  QStringList commands;
  commands << "@echo off";
  commands << "chcp 65001 > nul";

  for (const auto& op : operations) {
    QString src = QDir::toNativeSeparators(op.source);
    QString dst = QDir::toNativeSeparators(op.destination);

    switch (op.type) {
      case FileOperation::Type::CopyFiles:
        commands << QString("copy /Y \"%1\" \"%2\"").arg(src).arg(dst);
        break;
      case FileOperation::Type::CopyDirectory:
        commands << QString("xcopy /Y /E /I /R \"%1\" \"%2\"").arg(src).arg(dst);
        break;
      case FileOperation::Type::DeleteFiles:
        commands << QString("if exist \"%1\" del /F /Q \"%1\"").arg(src);
        break;
      case FileOperation::Type::DeleteDirectory:
        commands << QString("if exist \"%1\" rmdir /S /Q \"%1\"").arg(src);
        break;
      case FileOperation::Type::CreateDir:
        commands << QString("if not exist \"%1\" mkdir \"%1\"").arg(dst);
        break;
    }
  }

  return commands.join("\n");
}

bool FileOperations::executeBatchWithPrivileges(const QList<FileOperation>& operations) {
  if (operations.isEmpty()) {
    return true;
  }

  QTemporaryFile tempFile;
  tempFile.setFileTemplate(QDir::tempPath() + "/pag_fileop_XXXXXX.bat");
  tempFile.setAutoRemove(false);

  if (!tempFile.open()) {
    return false;
  }

  QString batPath = tempFile.fileName();
  QString script = generateBatchScript(operations);

  tempFile.write(script.toUtf8());
  tempFile.close();

  HINSTANCE result = ShellExecuteW(
      nullptr, L"runas", L"cmd.exe",
      QString("/c \"%1\"").arg(QDir::toNativeSeparators(batPath)).toStdWString().c_str(), nullptr,
      SW_SHOW);

  QFile::remove(batPath);

  return reinterpret_cast<intptr_t>(result) > 32;
}

}  // namespace pag
