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

#include "PAGCheckUpdateModel.h"
#include <QCoreApplication>
#include <QDebug>
#include "PAGNetworkFetcher.h"
#include "PAGUpdater.h"

namespace pag {

static const QString ServerUrl = "https://pag.qq.com/server.html";

PAGCheckUpdateModel::PAGCheckUpdateModel(QObject* parent) : QObject(parent) {
  threadPool = std::make_unique<QThreadPool>();
  threadPool->setMaxThreadCount(2);
}

PAGCheckUpdateModel::~PAGCheckUpdateModel() {
  if (threadPool) {
    threadPool->waitForDone();
  }
}

void PAGCheckUpdateModel::checkForUpdates(bool keepSlient, bool isUseBeta) {
  if (!availableUpdateUrls.empty()) {
    qDebug() << "Checking for updates is already in progress, please try again later";
    return;
  }

  this->isUseBeta = isUseBeta;
  this->keepSilent = keepSlient;
  PAGNetworkFetcher fetcher(ServerUrl);
  connect(&fetcher, &PAGNetworkFetcher::fetched, this, &PAGCheckUpdateModel::getAppcast);
  fetcher.fetch();
}

void PAGCheckUpdateModel::getAppcast(const QByteArray& data) {
  QString baseUrl = data.data();
  qDebug() << "Get update base url: " << baseUrl;

  availableUpdates.clear();
  availableUpdateUrls.clear();

  if (baseUrl.isEmpty()) {
    qDebug() << "No update url found";
    return;
  }

  QString productType = QSysInfo::productType();
  QString AppcastName = (productType == "windows" ? "PagAppcastWindows.xml" : "PagAppcast.xml");
  QString updateUrl = baseUrl + AppcastName;
  availableUpdateUrls.push_back(updateUrl);
  if (isUseBeta) {
    updateUrl = baseUrl + "beta/" + AppcastName;
    availableUpdateUrls.push_back(updateUrl);
  }

  for (const auto& url : availableUpdateUrls) {
    auto* task = new PAGUpdateVersionFetcherTask(url);
    task->setAutoDelete(false);
    connect(task, &PAGUpdateVersionFetcherTask::versionFound, this,
            &PAGCheckUpdateModel::getUpdateVersion);
    connect(task, &PAGUpdateVersionFetcherTask::finished, task, &QObject::deleteLater);
    threadPool->start(task);
  }
}

void PAGCheckUpdateModel::getUpdateVersion(QString url, QString version) {
  qDebug() << "Get Version: " << version << " from " << url;
  availableUpdates[url] = version;
  if (availableUpdates.size() < availableUpdateUrls.size()) {
    return;
  }

  QString selectedUrl = availableUpdateUrls.first();
  QString selectedVersion = "";
  for (const auto& [theUrl, theVersion] : availableUpdates.asKeyValueRange()) {
    if (selectedVersion.isEmpty()) {
      selectedUrl = theUrl;
      selectedVersion = theVersion;
      continue;
    }
    if (CompareAppVersion(selectedVersion, theVersion) < 0) {
      selectedUrl = theUrl;
      selectedVersion = theVersion;
    }
  }

  QMetaObject::invokeMethod(qApp, [this, selectedUrl]() -> void {
    availableUpdates.clear();
    availableUpdateUrls.clear();
    CheckForUpdates(keepSilent, selectedUrl.toStdString());
  });
}

int PAGCheckUpdateModel::CompareAppVersion(const QString& version1, const QString& version2) {
  QStringList parts1 = version1.split('.');
  QStringList parts2 = version2.split('.');

  int maxLength = std::max(parts1.size(), parts2.size());

  for (int i = 0; i < maxLength; ++i) {
    int num1 = (i < parts1.size()) ? parts1[i].toInt() : 0;
    int num2 = (i < parts2.size()) ? parts2[i].toInt() : 0;

    if (num1 < num2) {
      return -1;
    }
    if (num1 > num2) {
      return 1;
    }
  }

  return 0;
}

}  // namespace pag
