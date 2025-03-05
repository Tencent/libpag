#ifndef REPORT_PAGREPORT_H_
#define REPORT_PAGREPORT_H_

#include <memory>
#include <vector>
#include <string>

class HttpClient;

class PAGReport {
public:
    ~PAGReport();
    static PAGReport *getInstance() {
      if (PAGReport::instance == nullptr) {
        PAGReport::instance = new PAGReport();
      }
      return PAGReport::instance;
    }
    PAGReport(const PAGReport &s) = delete;
    PAGReport(const PAGReport &&s) = delete;
    PAGReport& operator=(const PAGReport &s) = delete;

    void report();
    void setEvent(std::string event);
    void addParam(std::string key, std::string value);
    void setAppKey(std::string appKey);
    void setAppName(std::string appName);
    void setAppVersion(std::string appVersion);
    void setAppBundleId(std::string appBundleId);

private:
    PAGReport();
    std::string generateReportData();

public:

private:
    static PAGReport *instance;
    std::unique_ptr<HttpClient> httpClient;

    std::string event;
    std::string appKey;
    std::string appName;
    std::string reportUrl;
    std::string appVersion;
    std::string appBundleId;
    std::string appPlatform;
    std::vector<std::pair<std::string, std::string> > extraParams;
};

#endif // REPORT_PAGREPORT_H_