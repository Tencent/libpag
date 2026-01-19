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

#include <QObject>
#include <QRunnable>

namespace pag {

class PAGNetworkFetcher : public QObject {
  Q_OBJECT
 public:
  explicit PAGNetworkFetcher(const QString& url, QObject* parent = nullptr);
  void fetch();
  Q_SIGNAL void finished();
  Q_SIGNAL void fetched(const QByteArray& data);

 protected:
  QString url = "";
};

class PAGUpdateVersionFetcher : public PAGNetworkFetcher {
  Q_OBJECT
 public:
  explicit PAGUpdateVersionFetcher(const QString& url, QObject* parent = nullptr);
  Q_SIGNAL void versionFound(QString url, QString version);

 private:
  void parseAppcast(const QByteArray& data);
};

class PAGUpdateVersionFetcherTask : public PAGUpdateVersionFetcher, public QRunnable {
  Q_OBJECT
 public:
  explicit PAGUpdateVersionFetcherTask(const QString& url, QObject* parent = nullptr);
  void run() override;
};

}  // namespace pag
