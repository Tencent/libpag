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

#include "PAGViewerInstaller.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QNetworkRequest>
#include <QProcess>
#include <QStandardPaths>
#include <utility>

namespace exporter {

PAGViewerInstaller::PAGViewerInstaller(std::shared_ptr<CheckConfig> config, QObject* parent)
    : QObject(parent), config(std::move(config)) {
  networkManager = new QNetworkAccessManager(this);
  downloadUrl = "https://pag.qq.com/update/libpag/PAGViewer.zip";

  tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/PAGInstaller";

  if (!QDir().mkpath(tempDir)) {
    tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
  }
}

PAGViewerInstaller::~PAGViewerInstaller() {
  if (downloadReply) {
    downloadReply->abort();
    downloadReply->deleteLater();
  }

  QDir(tempDir).removeRecursively();
}

bool PAGViewerInstaller::IsPAGViewerInstalled() {
#ifdef Q_OS_WIN
  return IsPAGViewerInstallWin();
#elif defined(Q_OS_MAC)
  return IsPAGViewerInstalledMac();
#endif
  return false;
}

bool PAGViewerInstaller::copyToApplicationsPlatform(const QString& sourcePath) {
#ifdef Q_OS_WIN
  return copyToApplicationsWindows(sourcePath);
#elif defined(Q_OS_MAC)
  return copyToApplicationsMac(sourcePath);
#else
  return false;
#endif
}

void PAGViewerInstaller::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
  if (bytesTotal > 0 && progressCallback) {
    int progress = static_cast<int>((bytesReceived * 50) / bytesTotal);
    progressCallback(progress);
  }
}

InstallStatus PAGViewerInstaller::InstallPAGViewer() {
  if (progressCallback) {
    progressCallback(0);
  }

  QString zipPath = tempDir + "/PAGViewer.zip";
  QFile::remove(zipPath);

  QNetworkRequest request{QUrl(downloadUrl)};
  request.setRawHeader("User-Agent", "PAGExporter/1.0");

  downloadReply = networkManager->get(request);
  if (!downloadReply) {
    return InstallStatus::Error(InstallResult::ExecutionFailed, "failed to create network request");
  }

  connect(downloadReply, &QNetworkReply::downloadProgress, this,
          &PAGViewerInstaller::onDownloadProgress);

  QEventLoop loop;
  connect(downloadReply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  loop.exec();

  if (downloadReply->error() != QNetworkReply::NoError) {
    QString errorMsg = "下载失败: " + downloadReply->errorString();
    downloadReply->deleteLater();
    downloadReply = nullptr;
    return InstallStatus::Error(InstallResult::ExecutionFailed, errorMsg.toStdString());
  }

  QFile file(zipPath);
  if (!file.open(QIODevice::WriteOnly)) {
    downloadReply->deleteLater();
    downloadReply = nullptr;
    return InstallStatus::Error(InstallResult::AccessDenied, "can not create temp folder");
  }

  QByteArray downloadedData = downloadReply->readAll();
  qint64 written = file.write(downloadedData);
  file.close();

  if (written != downloadedData.size()) {
    downloadReply->deleteLater();
    downloadReply = nullptr;
    return InstallStatus::Error(InstallResult::AccessDenied, "failed to write bytes");
  }

  downloadReply->deleteLater();
  downloadReply = nullptr;

  if (progressCallback) progressCallback(50);
  return extractAndInstall(zipPath);
}

void PAGViewerInstaller::setProgressCallback(std::function<void(int)> callback) {
  progressCallback = std::move(callback);
}

InstallStatus PAGViewerInstaller::extractAndInstall(const QString& zipPath) {
  QProcess unzipProcess;
  QStringList arguments;

#ifdef Q_OS_WIN
  unzipProcess.setProgram("powershell");
  arguments
      << "-Command"
      << QString("Expand-Archive -Path '%1' -DestinationPath '%2' -Force").arg(zipPath, tempDir);
#else
  unzipProcess.setProgram("unzip");
  arguments << "-o"
            << "-q" << zipPath << "-d" << tempDir;
#endif
  unzipProcess.setArguments(arguments);
  unzipProcess.start();
  unzipProcess.waitForFinished(30000);

  if (unzipProcess.exitCode() != 0) {
    QString error = unzipProcess.readAllStandardError();
    QString output = unzipProcess.readAllStandardOutput();
    return InstallStatus::Error(InstallResult::ExecutionFailed,
                                "unzip failed: " + error.toStdString());
  }

  if (progressCallback) {
    progressCallback(80);
  }

#ifdef Q_OS_WIN
  QString appPath = tempDir + "/PAGViewer.exe";
#elif defined(Q_OS_MAC)
  QString appPath = tempDir + "/PAGViewer.app";
#endif
  QFile appFile(appPath);
  if (!appFile.exists()) {
    return InstallStatus::Error(InstallResult::ExecutionFailed, "can not find file after unzip");
  }

  if (!copyToApplicationsPlatform(appPath)) {
    return InstallStatus::Error(InstallResult::AccessDenied, "install PAGViewer failed");
  }

  if (progressCallback) {
    progressCallback(90);
  }
  return InstallStatus::Success();
}

}  // namespace exporter
