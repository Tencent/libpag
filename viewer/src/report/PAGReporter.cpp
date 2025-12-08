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

#include "PAGReporter.h"
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <chrono>
#include <string>
#include "version.h"

namespace pag {

HttpClient::HttpClient(QObject* parent) : QObject(parent) {
  manager = std::make_unique<QNetworkAccessManager>();
  connect(manager.get(), &QNetworkAccessManager::finished, this, &HttpClient::onFinished);
}

void HttpClient::post(const std::string& url, const std::string& data) {
  QUrl request_url = QString::fromStdString(url);
  QNetworkRequest request(request_url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json;charset=UTF-8");
  QByteArray jsonData = data.c_str();
  manager->post(request, jsonData);
}

void HttpClient::onFinished(QNetworkReply* reply) {
  if (reply->error() == QNetworkReply::NoError) {
    QByteArray responseData = reply->readAll();
    qDebug() << "Http Response: \n" << responseData;
  } else {
    qDebug() << "Http Error: \n" << reply->errorString();
  }
  reply->deleteLater();
}

std::string GetCurrentTimeInMilliseconds() {
  auto now = std::chrono::system_clock::now();
  auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
  auto value = now_ms.time_since_epoch();
  return std::to_string(value.count());
}

PAGReporter* PAGReporter::instance = nullptr;

PAGReporter::PAGReporter() {
#if defined(WIN32)
  appPlatform = "Windows";
#elif defined(Q_OS_MACOS)
  appPlatform = "macOS";
#else
  appPlatform = "Unknown";
#endif
  event = "pag_sdk_report";
  reportUrl = ReportUrl;
  httpClient = std::make_unique<HttpClient>();
}

void PAGReporter::report() {
  std::string reportData = generateReportData();
  if (httpClient != nullptr) {
    httpClient->post(reportUrl, reportData);
  }
  extraParams.clear();
}

void PAGReporter::setEvent(const std::string& event) {
  this->event = event;
}

void PAGReporter::addParam(const std::string& key, const std::string& value) {
  extraParams.emplace_back(key, value);
}

void PAGReporter::setAppKey(const std::string& appKey) {
  this->appKey = appKey;
}

void PAGReporter::setAppName(const std::string& appName) {
  this->appName = appName;
}

void PAGReporter::setAppVersion(const std::string& appVersion) {
  this->appVersion = appVersion;
}

void PAGReporter::setAppBundleId(const std::string& appBundleId) {
  this->appBundleId = appBundleId;
}

std::string PAGReporter::generateReportData() {
  QJsonObject root;
  root["appVersion"] = "";
  root["sdkId"] = "js";
  root["sdkVersion"] = "4.3.4-web";
  root["mainAppKey"] = QString::fromStdString(appKey);
  root["platformId"] = "3";

  QJsonObject common;
  common["A2"] = QString::fromStdString(this->event);
  root["common"] = common;

  QJsonObject mapValue;
  mapValue["appName"] = QString::fromStdString(appName);
  mapValue["appID"] = QString::fromStdString(appBundleId);
  mapValue["appPlatform"] = QString::fromStdString(appPlatform);
  mapValue["previousSDKVersion"] = QString::fromStdString(appVersion);
  mapValue["PAGViewerVersion"] = QString::fromStdString(appVersion);
  for (const auto& pair : extraParams) {
    mapValue[QString::fromStdString(pair.first)] = QString::fromStdString(pair.second);
  }

  QJsonObject eventObj;
  eventObj["eventCode"] = QString::fromStdString(this->event);
  eventObj["eventTime"] = QString::fromStdString(GetCurrentTimeInMilliseconds());
  eventObj["mapValue"] = mapValue;

  QJsonArray events;
  events.append(eventObj);
  root["events"] = events;

  QJsonDocument doc(root);
  return doc.toJson(QJsonDocument::Indented).toStdString();
}

}  // namespace pag
