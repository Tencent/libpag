#include "PAGReport.h"
#include <chrono>
#include <string>
#include <QUrl>
#include <QDebug>
#include <QJsonObject>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include "cJSON/cJSON.h"

class HttpClient : public QObject {
    Q_OBJECT

public:
    HttpClient(QObject *parent = nullptr);
    void post(const std::string &url, const std::string &data);

private:
    Q_SLOT
    void onFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *manager;
};

HttpClient::HttpClient(QObject *parent) : QObject(parent) {
    manager = new QNetworkAccessManager(this);
    connect(manager, &QNetworkAccessManager::finished, this, &HttpClient::onFinished);
}

void HttpClient::post(const std::string &url, const std::string &data) {
    QUrl request_url = QString::fromStdString(url);
    QNetworkRequest request(request_url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json;charset=UTF-8");

    QByteArray jsonData = data.c_str();
    manager->post(request, jsonData);
}

void HttpClient::onFinished(QNetworkReply *reply) {
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

PAGReport *PAGReport::instance = nullptr;

PAGReport::PAGReport() {
#if defined(PAG_WINDOWS)
    appPlatform = "Windows";
#elif defined(PAG_MACOS)
    appPlatform = "macOS";
#else
    appPlatform = "Unknown";
#endif
    event = "pag_sdk_report";
    reportUrl = "https://otheve.beacon.qq.com/analytics/v2_upload";

    httpClient = std::make_unique<HttpClient>();
}

PAGReport::~PAGReport() = default;

void PAGReport::report() {
    std::string reportData = generateReportData();
    if (httpClient != nullptr) {
        httpClient->post(reportUrl, reportData);
    }

    extraParams.clear();
}

void PAGReport::setEvent(std::string event) {
    this->event = event;
}

void PAGReport::addParam(std::string key, std::string value) {
    std::pair<std::string, std::string> param(key, value);
    extraParams.push_back(std::move(param));
}

void PAGReport::setAppKey(std::string appKey) {
    this->appKey = appKey;
}

void PAGReport::setAppName(std::string appName) {
    this->appName = appName;
}

void PAGReport::setAppVersion(std::string appVersion) {
    this->appVersion = appVersion;
}

void PAGReport::setAppBundleId(std::string appBundleId) {
    this->appBundleId = appBundleId;
}

std::string PAGReport::generateReportData() {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "appVersion", "");
    cJSON_AddStringToObject(root, "sdkId", "js");
    cJSON_AddStringToObject(root, "sdkVersion", "4.3.4-web");
    cJSON_AddStringToObject(root, "mainAppKey", appKey.c_str());
    cJSON_AddStringToObject(root, "platformId", "3");

    cJSON* common = cJSON_CreateObject();
    cJSON_AddStringToObject(common, "A2", this->event.c_str());
    cJSON_AddItemToObject(root, "common", common);

    cJSON* events = cJSON_CreateArray();
    cJSON* event = cJSON_CreateObject();
    cJSON_AddStringToObject(event, "eventCode", this->event.c_str());
    cJSON_AddStringToObject(event, "eventTime", GetCurrentTimeInMilliseconds().c_str());

    cJSON* mapValue = cJSON_CreateObject();
    cJSON_AddStringToObject(mapValue, "appName", appName.c_str());
    cJSON_AddStringToObject(mapValue, "appID", appBundleId.c_str());
    cJSON_AddStringToObject(mapValue, "appPlatform", appPlatform.c_str());
    cJSON_AddStringToObject(mapValue, "previousSDKVersion", appVersion.c_str());
    cJSON_AddStringToObject(mapValue, "PAGViewerVersion", appVersion.c_str());
    if (!extraParams.empty()) {
        for (auto &pair : extraParams) {
            cJSON_AddStringToObject(mapValue, pair.first.c_str(), pair.second.c_str());
        }
    }
    cJSON_AddItemToObject(event, "mapValue", mapValue);

    cJSON_AddItemToArray(events, event);
    cJSON_AddItemToObject(root, "events", events);

    char* jsonData = cJSON_Print(root);
    cJSON_Delete(root);
    
    std::string reportData(jsonData);
    free(jsonData);

    return reportData;
}

#include "PAGReport.moc"
