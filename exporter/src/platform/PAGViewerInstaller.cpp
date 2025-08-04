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
PAGViewerInstaller::PAGViewerInstaller(std::shared_ptr<AppConfig> config, QObject* parent)
    : QObject(parent), config(std::move(config)) {
  networkManager = new QNetworkAccessManager(this);
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

void PAGViewerInstaller::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
  if (!progressCallback) {
    return;
  }

  if (bytesTotal <= 0) {
    progressCallback(0);
    return;
  }

  int progress = static_cast<int>((bytesReceived * kDownloadProgressWeight) / bytesTotal);
  progressCallback(progress);
}

InstallStatus PAGViewerInstaller::installPAGViewer() {
  if (progressCallback) {
    progressCallback(0);
  }

#ifdef Q_OS_WIN
  QString filePath = tempDir + "/PAGViewer_Installer.exe";
#else
  QString filePath = tempDir + "/PAGViewer.zip";
#endif
  QFile::remove(filePath);

  QNetworkRequest request{QUrl(downloadUrl)};
  request.setRawHeader("User-Agent", "PAGExporter/1.0");

  downloadReply = networkManager->get(request);
  if (!downloadReply) {
    return InstallStatus(InstallResult::ExecutionFailed, "failed to create network request");
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
    return InstallStatus(InstallResult::ExecutionFailed, errorMsg.toStdString());
  }

  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly)) {
    downloadReply->deleteLater();
    downloadReply = nullptr;
    return InstallStatus(InstallResult::AccessDenied, "can not create temp folder");
  }

  QByteArray downloadedData = downloadReply->readAll();
  qint64 written = file.write(downloadedData);
  file.close();

  if (written != downloadedData.size()) {
    downloadReply->deleteLater();
    downloadReply = nullptr;
    return InstallStatus(InstallResult::AccessDenied, "failed to write bytes");
  }

  downloadReply->deleteLater();
  downloadReply = nullptr;

  if (progressCallback) {
    progressCallback(50);
  }

  return executeInstall(filePath);
}

void PAGViewerInstaller::setProgressCallback(std::function<void(int)> callback) {
  progressCallback = std::move(callback);
}

}  // namespace exporter
