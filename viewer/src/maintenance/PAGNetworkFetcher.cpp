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

#include "PAGNetworkFetcher.h"
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QXmlStreamReader>

namespace pag {

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
    if (xml.isStartElement() && xml.name() == "enclosure") {
      QString version = xml.attributes().value(sparkleNs, "version").toString();
      if (!version.isEmpty()) {
        Q_EMIT versionFound(url, version);
        return;
      }
    }
  }

  Q_EMIT versionFound(url, "0");
}

PAGUpdateVersionFetcherTask::PAGUpdateVersionFetcherTask(const QString& url, QObject* parent)
    : PAGUpdateVersionFetcher(url, parent) {
}

void PAGUpdateVersionFetcherTask::run() {
  fetch();
}

}  // namespace pag
