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
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QTimer>
#include <functional>
#include "platform/PAGViewerCheck.h"

namespace exporter {
class PAGViewerInstaller : public QObject {
  Q_OBJECT
 public:
  explicit PAGViewerInstaller(std::shared_ptr<AppConfig> config, QObject* parent = nullptr);
  ~PAGViewerInstaller();

  bool isPAGViewerInstalled();
  InstallStatus installPAGViewer();
  void setProgressCallback(std::function<void(int)> callback);

 private:
  InstallStatus extractAndInstall(const QString& zipPath);

#ifdef Q_OS_WIN
  InstallStatus executeInstaller(const QString& installerPath);
#endif

  bool copyToApplications(const QString& sourcePath);

  Q_SLOT void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);

  std::shared_ptr<AppConfig> config = nullptr;
  std::function<void(int)> progressCallback = nullptr;
  QNetworkAccessManager* networkManager = nullptr;
  QNetworkReply* downloadReply = nullptr;
  QString tempDir;
  QString downloadUrl;
};
}  // namespace exporter
