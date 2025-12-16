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

#include "platform/PluginInstaller.h"
#include <shlobj.h>
#include <windows.h>
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTextStream>
#include <QThread>
#include "FileOperations.h"

namespace pag {

static constexpr int MaxCopyRetries = 5;
static constexpr int RetryDelayMs = 100;

QString PluginInstaller::GetH264EncoderToolsExePath() {
  QString appDir = QCoreApplication::applicationDirPath();
  QString localPath = appDir + "/H264EncoderTools.exe";
  if (QFile::exists(localPath)) {
    return localPath;
  }

  QString roaming = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  return roaming + "/H264EncoderTools/H264EncoderTools.exe";
}

bool PluginInstaller::checkAeRunning() {
  QProcess process;
  process.start("tasklist", QStringList() << "/FI"
                                          << "IMAGENAME eq AfterFX.exe");
  process.waitForFinished(3000);  // 3 second timeout
  QString output = process.readAllStandardOutput();
  return output.contains("AfterFX.exe");
}

bool PluginInstaller::requestConfirmation(const QString& title, const QString& message) {
  return QMessageBox::question(nullptr, title, message, QMessageBox::Yes | QMessageBox::No) ==
         QMessageBox::Yes;
}

void PluginInstaller::showMessage(const QString& title, const QString& message, bool isWarning) {
  QMessageBox msgBox;
  msgBox.setWindowTitle(title);
  msgBox.setText(message);
  msgBox.setIcon(isWarning ? QMessageBox::Warning : QMessageBox::Information);
  msgBox.setStandardButtons(QMessageBox::Ok);
  msgBox.exec();
}

QStringList PluginInstaller::getAeInstallPaths() {
  QStringList paths;

  const QStringList adobeRegPaths = {
      "HKEY_LOCAL_MACHINE\\SOFTWARE\\Adobe\\After Effects",
      "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Adobe\\After Effects",
      "HKEY_CURRENT_USER\\SOFTWARE\\Adobe\\After Effects"};

  for (const QString& regPath : adobeRegPaths) {
    QSettings registry(regPath, QSettings::NativeFormat);
    const QStringList versionKeys = registry.childGroups();

    for (const QString& versionKey : versionKeys) {
      QSettings aeRegistry(regPath + "\\" + versionKey, QSettings::NativeFormat);

      QString installPath = aeRegistry.value("InstallPath").toString();
      if (installPath.isEmpty()) {
        installPath = aeRegistry.value("ApplicationPath").toString();
      }

      if (!installPath.isEmpty()) {
        QString supportFilesPath = QDir::cleanPath(installPath + "/Support Files");
        if (!QDir(supportFilesPath).exists()) {
          supportFilesPath = QDir::cleanPath(installPath);
        }

        if (QDir(supportFilesPath).exists() && QFile::exists(supportFilesPath + "/AfterFX.exe")) {
          int version = static_cast<int>(versionKey.split('.').first().toDouble());
          QString pathWithVersion = QString("%1|%2").arg(supportFilesPath).arg(version);
          if (!paths.contains(pathWithVersion)) {
            paths << pathWithVersion;
          }
        }
      }
    }
  }

  const QStringList uninstallRegPaths = {
      "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
      "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall"};

  for (const QString& uninstallPath : uninstallRegPaths) {
    QSettings uninstallRegistry(uninstallPath, QSettings::NativeFormat);
    const QStringList uninstallKeys = uninstallRegistry.childGroups();

    for (const QString& key : uninstallKeys) {
      QSettings appRegistry(uninstallPath + "\\" + key, QSettings::NativeFormat);
      QString displayName = appRegistry.value("DisplayName").toString();

      if (displayName.contains("Adobe After Effects", Qt::CaseInsensitive)) {
        QString installLocation = appRegistry.value("InstallLocation").toString();
        if (!installLocation.isEmpty()) {
          QString supportFilesPath = QDir::cleanPath(installLocation + "/Support Files");
          if (!QDir(supportFilesPath).exists()) {
            supportFilesPath = QDir::cleanPath(installLocation);
          }

          if (QDir(supportFilesPath).exists() && QFile::exists(supportFilesPath + "/AfterFX.exe")) {
            QRegularExpression versionRegex("After Effects(?: CC)? (\\d+)");
            QRegularExpressionMatch match = versionRegex.match(displayName);
            QString pathWithVersion;
            if (match.hasMatch()) {
              int version = match.captured(1).toInt();
              pathWithVersion = QString("%1|%2").arg(supportFilesPath).arg(version);
            } else {
              pathWithVersion = supportFilesPath;
            }

            if (!paths.contains(pathWithVersion)) {
              paths << pathWithVersion;
            }
          }
        }
      }
    }
  }

  std::sort(paths.begin(), paths.end(), [](const QString& a, const QString& b) {
    int versionA = 0, versionB = 0;
    if (a.contains('|')) {
      versionA = a.split('|').last().toInt();
    } else {
      QRegularExpression versionRegex("After Effects(?: CC)? (\\d+)");
      QRegularExpressionMatch matchA = versionRegex.match(a);
      versionA = matchA.hasMatch() ? matchA.captured(1).toInt() : 0;
    }

    if (b.contains('|')) {
      versionB = b.split('|').last().toInt();
    } else {
      QRegularExpression versionRegex("After Effects(?: CC)? (\\d+)");
      QRegularExpressionMatch matchB = versionRegex.match(b);
      versionB = matchB.hasMatch() ? matchB.captured(1).toInt() : 0;
    }

    return versionA > versionB;
  });

  QStringList cleanPaths;
  for (const QString& path : paths) {
    if (path.contains('|')) {
      cleanPaths << path.split('|').first();
    } else {
      cleanPaths << path;
    }
  }

  return cleanPaths;
}

QString PluginInstaller::getPluginFullName(const QString& pluginName) const {
  if (pluginName == "H264EncoderTools") {
    return pluginName + ".exe";
  } else {
    return pluginName + ".aex";
  }
}

QString PluginInstaller::getPluginSourcePath(const QString& pluginName) const {
  QString appDir = QCoreApplication::applicationDirPath();
  QString fullName = getPluginFullName(pluginName);
  return appDir + "/" + fullName;
}

QString PluginInstaller::getPluginInstallPath(const QString& pluginName) const {
  QString fullName = getPluginFullName(pluginName);

  if (pluginName == "H264EncoderTools") {
    QString roaming = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return roaming + "/H264EncoderTools/" + fullName;
  } else {
    return "C:/Program Files/Adobe/Common/Plug-ins/7.0/MediaCore/" + fullName;
  }
}

VersionResult PluginInstaller::getPluginVersionString(const QString& pluginPath,
                                                      QString& version) const {
  version.clear();
  QString targetPath = pluginPath;
  if (!targetPath.endsWith(".aex") && !targetPath.endsWith(".exe")) {
    if (QFile::exists(targetPath + ".aex")) {
      targetPath = targetPath + ".aex";
    } else if (QFile::exists(targetPath + ".exe")) {
      targetPath = targetPath + ".exe";
    } else {
      QDir pluginDir(pluginPath);
      if (pluginDir.exists()) {
        QStringList entries = pluginDir.entryList(QStringList() << "*.aex"
                                                                << "*.exe",
                                                  QDir::Files);
        if (!entries.isEmpty()) {
          targetPath = pluginPath + "/" + entries.first();
        } else {
          return VersionResult::NoPluginFiles;
        }
      } else {
        return VersionResult::DirectoryNotFound;
      }
    }
  }

  if (!QFile::exists(targetPath)) {
    return VersionResult::FileNotFound;
  }

  std::wstring filePathW = targetPath.toStdWString();
  DWORD handle = 0;
  DWORD size = GetFileVersionInfoSizeW(filePathW.c_str(), &handle);

  if (size == 0) {
    return VersionResult::VersionInfoUnavailable;
  }

  std::vector<BYTE> buffer(size);
  if (!GetFileVersionInfoW(filePathW.c_str(), handle, size, buffer.data())) {
    return VersionResult::VersionInfoUnavailable;
  }

  VS_FIXEDFILEINFO* fileInfo = nullptr;
  UINT fileInfoSize = 0;
  if (!VerQueryValueW(buffer.data(), L"\\", reinterpret_cast<LPVOID*>(&fileInfo), &fileInfoSize)) {
    return VersionResult::VersionInfoUnavailable;
  }

  if (fileInfo == nullptr || fileInfoSize == 0) {
    return VersionResult::VersionInfoUnavailable;
  }

  int major = HIWORD(fileInfo->dwFileVersionMS);
  int minor = LOWORD(fileInfo->dwFileVersionMS);
  int patch = HIWORD(fileInfo->dwFileVersionLS);
  int build = LOWORD(fileInfo->dwFileVersionLS);

  QString versionStr = QString::number(major);
  if (minor > 0 || patch > 0 || build > 0) {
    versionStr += "." + QString::number(minor);
  }
  if (patch > 0 || build > 0) {
    versionStr += "." + QString::number(patch);
  }
  if (build > 0) {
    versionStr += "." + QString::number(build);
  }

  version = versionStr;
  return VersionResult::Success;
}

int PluginInstaller::getAeVersionForPath(const QString& aePath) const {
  const QStringList registryPaths = {"HKEY_LOCAL_MACHINE\\SOFTWARE\\Adobe",
                                     "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Adobe",
                                     "HKEY_CURRENT_USER\\SOFTWARE\\Adobe"};

  for (const QString& regPath : registryPaths) {
    QSettings registry(regPath, QSettings::NativeFormat);
    const QStringList adobeKeys = registry.childGroups();

    for (const QString& key : adobeKeys) {
      if (key.contains("After Effects", Qt::CaseInsensitive)) {
        QSettings aeRegistry(regPath + "\\" + key, QSettings::NativeFormat);
        QString installPath = aeRegistry.value("InstallPath").toString();
        if (!installPath.isEmpty()) {
          QString supportFilesPath = QDir::cleanPath(installPath + "/Support Files");
          if (supportFilesPath == aePath) {
            QRegularExpression versionRegex("After Effects(?: CC)? (\\d+)");
            QRegularExpressionMatch match = versionRegex.match(key);
            if (match.hasMatch()) {
              return match.captured(1).toInt();
            }
          }
        }
      }
    }
  }

  QSettings ccRegistry(
      "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
      QSettings::NativeFormat);
  const QStringList uninstallKeys = ccRegistry.childGroups();

  for (const QString& key : uninstallKeys) {
    QSettings appRegistry(
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" + key,
        QSettings::NativeFormat);
    QString displayName = appRegistry.value("DisplayName").toString();
    if (displayName.contains("Adobe After Effects", Qt::CaseInsensitive)) {
      QString installLocation = appRegistry.value("InstallLocation").toString();
      if (!installLocation.isEmpty()) {
        QString supportFilesPath = QDir::cleanPath(installLocation + "/Support Files");
        if (supportFilesPath == aePath) {
          QRegularExpression versionRegex("After Effects(?: CC)? (\\d+)");
          QRegularExpressionMatch match = versionRegex.match(displayName);
          if (match.hasMatch()) {
            return match.captured(1).toInt();
          }
        }
      }
    }
  }

  return 0;
}

bool PluginInstaller::executeWithPrivileges(const QString& command) const {
  QTemporaryFile tempFile;
  tempFile.setFileTemplate(QDir::tempPath() + "/install_plugin_XXXXXX.bat");
  tempFile.setAutoRemove(false);

  if (!tempFile.open()) {
    return false;
  }

  QString batPath = tempFile.fileName();
  tempFile.write("@echo off\n");
  tempFile.write("chcp 65001 > nul\n");
  tempFile.write(command.toUtf8());
  tempFile.write("\necho.\necho Press any key to continue...\npause > nul\n");
  tempFile.close();

  HINSTANCE result = ShellExecuteW(
      nullptr, L"runas", L"cmd.exe",
      QString("/c \"%1\"").arg(QDir::toNativeSeparators(batPath)).toStdWString().c_str(), nullptr,
      SW_SHOW);
  QFile::remove(batPath);

  return reinterpret_cast<intptr_t>(result) > 32;
}

bool PluginInstaller::copyPluginFiles(const QStringList& plugins) const {
  QList<FileOperation> operations;
  QStringList aePaths = const_cast<PluginInstaller*>(this)->getAeInstallPaths();

  storeViewerPathForPlugin();

  if (plugins.contains("H264EncoderTools")) {
    copyH264EncoderToolsWithRetry(MaxCopyRetries);
  }

  if (plugins.contains("PAGExporter") && !aePaths.isEmpty()) {
    QString sourcePath = getPluginSourcePath("PAGExporter");
    if (QFile::exists(sourcePath)) {
      for (const QString& aePath : aePaths) {
        QString pluginsDir = aePath + "/Plug-ins";
        operations << FileOperation::createDirectory(pluginsDir);
        operations << FileOperation::copyFile(sourcePath, pluginsDir + "/PAGExporter.aex");
      }
    }

    QString appDir = getQtResourceDir();
    QDir dir(appDir);
    QStringList dllFiles = dir.entryList(QStringList() << "*.dll", QDir::Files);
    QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString& aePath : aePaths) {
      for (const QString& dllFile : dllFiles) {
        if (shouldExcludeFile(dllFile)) {
          continue;
        }
        operations << FileOperation::copyFile(appDir + "/" + dllFile, aePath + "/" + dllFile);
      }
      for (const QString& subDir : subDirs) {
        if (shouldExcludeDir(subDir)) {
          continue;
        }
        operations << FileOperation::copyDirectory(appDir + "/" + subDir, aePath + "/" + subDir);
      }
    }
  }

  for (const QString& plugin : plugins) {
    if (plugin == "H264EncoderTools" || plugin == "PAGExporter") {
      continue;
    }

    QString source = getPluginSourcePath(plugin);
    QString target = getPluginInstallPath(plugin);

    if (!QFile::exists(source)) {
      continue;
    }

    QString targetDir = QFileInfo(target).absolutePath();
    operations << FileOperation::createDirectory(targetDir);
    operations << FileOperation::copyFile(source, target);
  }

  if (operations.isEmpty()) {
    return true;
  }

  auto result = FileOperations::executeBatch(operations);
  if (result == FileOpResult::Success) {
    return true;
  }

  if (result == FileOpResult::AccessDenied) {
    return FileOperations::executeBatchWithPrivileges(operations);
  }

  return false;
}

bool PluginInstaller::removePluginFiles(const QStringList& plugins) const {
  QList<FileOperation> operations;
  QStringList aePaths = const_cast<PluginInstaller*>(this)->getAeInstallPaths();

  if (plugins.contains("PAGExporter") && !aePaths.isEmpty()) {
    for (const QString& aePath : aePaths) {
      operations << FileOperation::deleteFile(aePath + "/Plug-ins/PAGExporter.aex");
    }

    QString appDir = getQtResourceDir();
    QDir dir(appDir);
    QStringList dllFiles = dir.entryList(QStringList() << "*.dll", QDir::Files);
    QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString& aePath : aePaths) {
      for (const QString& dllFile : dllFiles) {
        if (shouldExcludeFile(dllFile)) {
          continue;
        }
        operations << FileOperation::deleteFile(aePath + "/" + dllFile);
      }
      for (const QString& subDir : subDirs) {
        if (shouldExcludeDir(subDir)) {
          continue;
        }
        operations << FileOperation::deleteDirectory(aePath + "/" + subDir);
      }
    }
  }

  if (plugins.contains("H264EncoderTools")) {
    QString h264Path = getPluginInstallPath("H264EncoderTools");
    if (QFile::exists(h264Path)) {
      QFile::remove(h264Path);
    }
  }

  for (const QString& plugin : plugins) {
    if (plugin == "H264EncoderTools" || plugin == "PAGExporter") {
      continue;
    }
    operations << FileOperation::deleteFile(getPluginInstallPath(plugin));
  }

  if (operations.isEmpty()) {
    return true;
  }

  auto result = FileOperations::executeBatch(operations);
  if (result == FileOpResult::Success) {
    return true;
  }

  if (result == FileOpResult::AccessDenied) {
    return FileOperations::executeBatchWithPrivileges(operations);
  }

  return false;
}

QString PluginInstaller::getQtResourceDir() const {
  return QCoreApplication::applicationDirPath();
}

bool PluginInstaller::shouldExcludeFile(const QString& fileName) const {
  // Files that should NOT be copied to AE directory
  static const QStringList excludedFiles = {
      "PAGViewer.exe", "PAGExporter.aex", "H264EncoderTools.exe", "ffmovie.dll", "WinSparkle.dll",
  };
  return excludedFiles.contains(fileName, Qt::CaseInsensitive);
}

bool PluginInstaller::shouldExcludeDir(const QString& dirName) const {
  static const QStringList excludedDirs = {
      "scripts",
  };
  return excludedDirs.contains(dirName, Qt::CaseInsensitive);
}

QString PluginInstaller::getH264EncoderToolsInstallDir() const {
  QString roaming = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  return roaming + "/H264EncoderTools";
}

bool PluginInstaller::copyH264EncoderToolsWithRetry(int maxRetries) const {
  QString sourcePath = getPluginSourcePath("H264EncoderTools");
  if (!QFile::exists(sourcePath)) {
    return false;
  }

  QString installDir = getH264EncoderToolsInstallDir();
  QString targetPath = installDir + "/H264EncoderTools.exe";

  QDir dir;
  if (!dir.exists(installDir)) {
    if (!dir.mkpath(installDir)) {
      return false;
    }
  }

  for (int attempt = 0; attempt < maxRetries; ++attempt) {
    if (QFile::exists(targetPath)) {
      QFile::remove(targetPath);
    }

    if (QFile::copy(sourcePath, targetPath)) {
      return true;
    }

    if (attempt < maxRetries - 1) {
      QThread::msleep(RetryDelayMs);
    }
  }

  return false;
}

void PluginInstaller::storeViewerPathForPlugin() const {
  QString roaming = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QString pagExporterDir = roaming + "/PAGExporter";

  QDir dir;
  if (!dir.exists(pagExporterDir)) {
    if (!dir.mkpath(pagExporterDir)) {
      return;
    }
  }

  QString viewerPathFile = pagExporterDir + "/PAGViewerPath.txt";
  QString viewerExePath = QCoreApplication::applicationFilePath();

  QFile file(viewerPathFile);
  if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QTextStream stream(&file);
    stream << viewerExePath;
    file.close();
  }
}

}  // namespace pag
