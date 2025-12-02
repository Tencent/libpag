#include "platform/PAGViewerCheck.h"
// clang-format off
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stringapiset.h>
#include <winerror.h>
#include <winreg.h>
// clang-format on
#include <algorithm>
#include <filesystem>
#include <memory>
#include <unordered_map>
#include "platform/PlatformHelper.h"
#include "utils/FileHelper.h"
#include "utils/StringHelper.h"

namespace exporter {

class RegistryKey {
 public:
  RegistryKey() : hKey(nullptr) {
  }
  explicit RegistryKey(HKEY hKey) : hKey(hKey) {
  }
  ~RegistryKey() {
    close();
  }

  RegistryKey(RegistryKey&& other) noexcept : hKey(other.hKey) {
    other.hKey = nullptr;
  }

  RegistryKey& operator=(RegistryKey&& other) noexcept {
    if (this != &other) {
      close();
      hKey = other.hKey;
      other.hKey = nullptr;
    }
    return *this;
  }

  RegistryKey(const RegistryKey&) = delete;
  RegistryKey& operator=(const RegistryKey&) = delete;

  bool isValid() const {
    return hKey != nullptr;
  }
  HKEY get() const {
    return hKey;
  }

  void close() {
    if (hKey) {
      RegCloseKey(hKey);
      hKey = nullptr;
    }
  }

  static RegistryKey Open(HKEY parent, const std::wstring& subKey, REGSAM access = KEY_READ) {
    HKEY hKey = nullptr;
    LONG result = RegOpenKeyExW(parent, subKey.c_str(), 0, access, &hKey);
    return (result == ERROR_SUCCESS) ? RegistryKey(hKey) : RegistryKey();
  }

 private:
  HKEY hKey;
};

std::wstring stringToWstring(const std::string& str) {
  if (str.empty()) {
    return std::wstring();
  }

  auto size =
      MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()), nullptr, 0);
  if (size <= 0) {
    return std::wstring();
  }

  std::wstring result(size, 0);
  MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()), &result[0], size);
  return result;
}

static std::string WstringToString(const std::wstring& wstr) {
  if (wstr.empty()) {
    return "";
  }

  auto size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.length()),
                                  nullptr, 0, nullptr, nullptr);
  if (size <= 0) {
    return std::string();
  }
  std::string result(size, 0);
  WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.length()), &result[0], size,
                      nullptr, nullptr);
  return result;
}

static std::string ReadRegistryStringValue(const RegistryKey& key, const std::wstring& valueName) {
  if (!key.isValid()) {
    return "";
  }

  DWORD dataSize = 0;
  LONG result =
      RegQueryValueExW(key.get(), valueName.c_str(), nullptr, nullptr, nullptr, &dataSize);

  if (result != ERROR_SUCCESS || dataSize == 0) {
    return "";
  }

  std::wstring buffer;
  buffer.resize(dataSize / sizeof(wchar_t));
  result = RegQueryValueExW(key.get(), valueName.c_str(), nullptr, nullptr,
                            reinterpret_cast<LPBYTE>(&buffer[0]), &dataSize);

  if (result == ERROR_SUCCESS) {
    buffer.resize(wcslen(buffer.c_str()));
    return WstringToString(buffer);
  }

  return "";
}

static std::vector<std::wstring> EnumerateRegistrySubKeys(const RegistryKey& key) {
  std::vector<std::wstring> subKeys = {};
  if (!key.isValid()) {
    return subKeys;
  }

  DWORD subKeyCount = 0;
  DWORD maxSubKeyLen = 0;
  if (RegQueryInfoKey(key.get(), nullptr, nullptr, nullptr, &subKeyCount, &maxSubKeyLen, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) {
    return subKeys;
  }

  if (subKeyCount == 0) {
    return subKeys;
  }

  std::vector<wchar_t> subKeyName(maxSubKeyLen + 1);
  for (DWORD i = 0; i < subKeyCount; ++i) {
    DWORD subKeyNameSize = static_cast<DWORD>(subKeyName.size());
    if (RegEnumKeyExW(key.get(), i, subKeyName.data(), &subKeyNameSize, nullptr, nullptr, nullptr,
                      nullptr) == ERROR_SUCCESS) {
      subKeys.emplace_back(subKeyName.data());
    }
  }

  return subKeys;
}

static PackageInfo ReadPackageInfoFromRegistry(const RegistryKey& parentKey,
                                               const std::wstring& subKeyName) {
  PackageInfo info = {};

  auto subKey = RegistryKey::Open(parentKey.get(), subKeyName);
  if (!subKey.isValid()) {
    return info;
  }

  info.displayName = ReadRegistryStringValue(subKey, L"DisplayName");
  if (info.displayName.empty()) {
    info.displayName = ReadRegistryStringValue(subKey, L"QuietDisplayName");
  }

  info.version = ReadRegistryStringValue(subKey, L"DisplayVersion");
  info.installLocation = ReadRegistryStringValue(subKey, L"InstallLocation");
  info.uninstallString = ReadRegistryStringValue(subKey, L"UninstallString");

  return info;
}

static void ScanRegistryView(std::vector<PackageInfo>& softwareList, HKEY rootKey, REGSAM access) {
  const std::wstring uninstallKeyPath = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
  auto uninstallKey = RegistryKey::Open(rootKey, uninstallKeyPath, KEY_READ | access);
  if (!uninstallKey.isValid()) {
    return;
  }

  auto subKeys = EnumerateRegistrySubKeys(uninstallKey);
  for (const auto& subKeyName : subKeys) {
    PackageInfo info = ReadPackageInfoFromRegistry(uninstallKey, subKeyName);
    if (!info.displayName.empty()) {
      softwareList.push_back(std::move(info));
    }
  }
}

static std::vector<PackageInfo> ScanUninstallRegistry() {
  std::vector<PackageInfo> softwareList = {};
  ScanRegistryView(softwareList, HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY);
  ScanRegistryView(softwareList, HKEY_LOCAL_MACHINE, KEY_WOW64_32KEY);
  ScanRegistryView(softwareList, HKEY_CURRENT_USER, KEY_WOW64_64KEY);
  ScanRegistryView(softwareList, HKEY_CURRENT_USER, KEY_WOW64_32KEY);
  return softwareList;
}

void AppConfig::setAppName(const std::string& name) {
  this->AppName = name;
}

std::string AppConfig::getAppName() {
  return this->AppName;
}

void AppConfig::setInstallerPath(const std::string& path) {
  this->installerPath = path;
}

std::string AppConfig::getInstallerPath() {
  if (!this->installerPath.empty()) {
    return this->installerPath;
  }

  std::string downloadsPath = GetDownloadsPath();
  return downloadsPath + "\\PAGViewer_Installer.exe";
}

void AppConfig::addConfig(const std::string& key, const std::string& value) {
  this->platformConfig[key] = value;
}

std::string AppConfig::getPlatformSpecificConfig(const std::string& key) {
  auto it = this->platformConfig.find(key);
  return (it != this->platformConfig.end()) ? it->second : "";
}

PAGViewerCheck::PAGViewerCheck(std::shared_ptr<AppConfig> config) : config(config) {
}

bool PAGViewerCheck::isPAGViewerInstalled() {
  auto results = findPackageinfoByName(config->getAppName());
  return !results.empty();
}

PackageInfo PAGViewerCheck::getPackageInfo() {
  auto results = findPackageinfoByName(config->getAppName());
  return results.empty() ? PackageInfo() : results[0];
}

std::vector<PackageInfo> PAGViewerCheck::findPackageinfoByName(const std::string& namePattern) {
  std::vector<PackageInfo> results = {};
  auto allSoftware = ScanUninstallRegistry();
  std::string lowerPattern = ToLowerCase(namePattern);

  for (const auto& software : allSoftware) {
    std::string lowerName = ToLowerCase(software.displayName);

    if (lowerName.find(lowerPattern) != std::string::npos) {
      results.push_back(software);
    }
  }

  return results;
}

InstallStatus PAGViewerCheck::installPAGViewer() {
  std::string installerPath = config->getInstallerPath();
  if (installerPath.empty() || installerPath.length() > MAX_PATH) {
    return InstallStatus(InstallResult::InvalidPath, "Invalid installer path: " + installerPath);
  }

  std::wstring installerPathW = stringToWstring(installerPath);
  if (!FileIsExist(installerPath)) {
    return InstallStatus(InstallResult::FileNotFound, "Installer file not found: " + installerPath);
  }

  SHELLEXECUTEINFOW sei = {sizeof(sei)};
  sei.fMask = SEE_MASK_NOCLOSEPROCESS;
  sei.lpVerb = L"runas";
  sei.lpFile = installerPathW.c_str();
  sei.nShow = SW_SHOWNORMAL;

  if (!ShellExecuteExW(&sei)) {
    DWORD error = GetLastError();
    if (error == ERROR_CANCELLED) {
      return InstallStatus(InstallResult::PermissionDenied, "User denied permission.");
    }
    return InstallStatus(InstallResult::ExecutionFailed,
                         "Failed to execute installer. Error: " + std::to_string(error));
  }

  if (sei.hProcess) {
    WaitForSingleObject(sei.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(sei.hProcess, &exitCode);
    CloseHandle(sei.hProcess);

    if (exitCode == 0) {
      return InstallStatus(InstallResult::Success);
    } else {
      return InstallStatus(InstallResult::ExecutionFailed,
                           "Installer exited with code: " + std::to_string(exitCode));
    }
  }

  return InstallStatus(InstallResult::Success);
}

}  // namespace exporter
