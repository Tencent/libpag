/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
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
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QThread>
#include <QXmlStreamReader>
#include "PAGUpdater.h"

namespace pag {

static const QString ServerUrl = "https://pag.qq.com/server.html";

PAGNetworkFetcher::PAGNetworkFetcher(const QString& url, QObject* parent)
    : QObject(parent), url(url) {
}

void PAGNetworkFetcher::fetch() {
  QNetworkAccessManager manager;
  QNetworkReply* reply = manager.get(QNetworkRequest(QUrl(url)));

  QEventLoop eventLoop;
  connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
  eventLoop.exec();

  if (reply->error() == QNetworkReply::NoError) {
    Q_EMIT fetched(reply->readAll());
  } else {
    Q_EMIT fetched({});
  }

  reply->deleteLater();
  Q_EMIT finished();
}

PAGUpdateVersionFetcher::PAGUpdateVersionFetcher(const QString& url, QObject* parent)
    : PAGNetworkFetcher(url, parent) {
  connect(this, &PAGUpdateVersionFetcher::fetched, this, &PAGUpdateVersionFetcher::parseAppcast);
}

void PAGUpdateVersionFetcher::parseAppcast(const QByteArray& data) {
  QXmlStreamReader xml(data);

  QString sparkleNs = "http://www.andymatuschak.org/xml-namespaces/sparkle";
  while (!xml.atEnd()) {
    xml.readNext();
    if (xml.isStartElement() && xml.name() == "item") {
      while (!(xml.isEndElement() && xml.name() == "item")) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == "enclosure") {
          QString version = xml.attributes().value(sparkleNs, "version").toString();
          if (!version.isEmpty()) {
            Q_EMIT versionFound(url, version);
            return;
          }
        }
      }
    }
  }

  Q_EMIT versionFound(url, "0");
}

QVector<QString> PAGCheckUpdateModel::AvailableUpdateUrls = {};
QMap<QString, QString> PAGCheckUpdateModel::AvailableUpdates = {};

PAGCheckUpdateModel::PAGCheckUpdateModel(QObject* parent) : QObject(parent) {
}

void PAGCheckUpdateModel::checkForUpdates(bool keepSlient, bool isUseBeta) {
  if (!AvailableUpdateUrls.empty()) {
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
  qDebug() << "get update base url: " << baseUrl;

  AvailableUpdates.clear();
  AvailableUpdateUrls.clear();

  if (baseUrl.isEmpty()) {
    qDebug() << "No update url found";
    return;
  }

  QString productType = QSysInfo::productType();
  QString AppcastName = (productType == "windows" ? "PagAppcastWindows.xml" : "PagAppcast.xml");
  QString updateUrl = baseUrl + AppcastName;
  AvailableUpdateUrls.push_back(updateUrl);
  if (isUseBeta) {
    updateUrl = baseUrl + "beta/" + AppcastName;
    AvailableUpdateUrls.push_back(updateUrl);
  }

  for (const auto& url : AvailableUpdateUrls) {
    auto* thread = new QThread();
    auto* fetcher = new PAGUpdateVersionFetcher(url);
    fetcher->moveToThread(thread);
    connect(thread, &QThread::started, fetcher, &PAGUpdateVersionFetcher::fetch);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    connect(fetcher, &PAGUpdateVersionFetcher::finished, thread, &QThread::quit);
    connect(fetcher, &PAGUpdateVersionFetcher::finished, fetcher,
            &PAGUpdateVersionFetcher::deleteLater);
    connect(fetcher, &PAGUpdateVersionFetcher::versionFound, this,
            &PAGCheckUpdateModel::getUpdateVersion);
    thread->start();
  }
}

void PAGCheckUpdateModel::getUpdateVersion(const QString& url, const QString& version) {
  qDebug() << "Get Version: " << version << " from " << url;
  AvailableUpdates[url] = version;
  if (AvailableUpdates.size() < AvailableUpdateUrls.size()) {
    return;
  }

  QString selectedUrl = AvailableUpdateUrls.first();
  QString selectedVersion = "";
  for (const auto& [theUrl, theVersion] : AvailableUpdates.asKeyValueRange()) {
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
    AvailableUpdates.clear();
    AvailableUpdateUrls.clear();
    PAGUpdater::CheckForUpdates(keepSilent, selectedUrl.toStdString());
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