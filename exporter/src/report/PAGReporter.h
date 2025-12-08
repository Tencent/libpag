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

#include <QNetworkReply>
#include <QObject>
#include <memory>
#include <string>
#include <vector>

namespace exporter {

class HttpClient : public QObject {
  Q_OBJECT
 public:
  explicit HttpClient(QObject* parent = nullptr);
  void post(const std::string& url, const std::string& data);

 private:
  Q_SLOT void onFinished(QNetworkReply* reply);

  std::unique_ptr<QNetworkAccessManager> manager = nullptr;
};

class PAGReporter {
 public:
  static PAGReporter* GetInstance() {
    if (instance == nullptr) {
      instance = new PAGReporter();
    }
    return instance;
  }
  void report();
  void setEvent(const std::string& event);
  void addParam(const std::string& key, const std::string& value);
  void setAppKey(const std::string& appKey);
  void setAppName(const std::string& appName);
  void setAppVersion(const std::string& appVersion);
  void setAppBundleId(const std::string& appBundleId);
  void setPAGCount(const std::string& pagCount);
  void setPAGExportEntry(const std::string& pagExportEntry);

  PAGReporter(const PAGReporter& s) = delete;
  PAGReporter(const PAGReporter&& s) = delete;
  PAGReporter& operator=(const PAGReporter& s) = delete;

 private:
  PAGReporter();
  std::string generateReportData();

  static PAGReporter* instance;

  std::string event = "";
  std::string appKey = "";
  std::string appName = "";
  std::string pagCount = "";
  std::string reportUrl = "";
  std::string appVersion = "";
  std::string appBundleId = "";
  std::string appPlatform = "";
  std::string pagExportEntry = "";
  std::unique_ptr<HttpClient> httpClient = nullptr;
  std::vector<std::pair<std::string, std::string> > extraParams = {};
};

}  // namespace exporter
