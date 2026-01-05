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

#include <QMap>
#include <QObject>
#include <QThreadPool>

namespace pag {

class PAGCheckUpdateModel : public QObject {
  Q_OBJECT
 public:
  explicit PAGCheckUpdateModel(QObject* parent = nullptr);
  ~PAGCheckUpdateModel() override;
  Q_INVOKABLE void checkForUpdates(bool keepSlient, bool isUseBeta);
  void getAppcast(const QByteArray& data);
  void getUpdateVersion(QString url, QString version);

  static int CompareAppVersion(const QString& version1, const QString& version2);
  QVector<QString> availableUpdateUrls = {};
  QMap<QString, QString> availableUpdates = {};

 private:
  bool isUseBeta = false;
  bool keepSilent = false;
  std::unique_ptr<QThreadPool> threadPool = nullptr;
};

}  // namespace pag
