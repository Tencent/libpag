#include "platform/PAGViewerCheck.h"
#import <Cocoa/Cocoa.h>
#import <CoreServices/CoreServices.h>
#include <string>
#include <unordered_map>
#include "platform/PAGViewerInstaller.h"
#include "platform/PlatformHelper.h"
#include "utils/FileHelper.h"

namespace exporter {

static NSBundle* FindPAGViewerBundle(std::shared_ptr<AppConfig> config) {
  @autoreleasepool {
    std::string bundleIDString = config->getPlatformSpecificConfig("bundleID");
    NSString* bundleID = [NSString stringWithUTF8String:bundleIDString.c_str()];
    if (!bundleID || [bundleID length] == 0) {
      return nil;
    }

    NSURL* appURL = [[NSWorkspace sharedWorkspace] URLForApplicationWithBundleIdentifier:bundleID];
    if (!appURL) {
      return nil;
    }

    return [NSBundle bundleWithURL:appURL];
  }
}

void AppConfig::setAppName(const std::string& name) {
  this->AppName = std::move(name);
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
  std::string downloadsPathStr = GetDownloadsPath();
  return downloadsPathStr + "/PAGViewer.dmg";
}

void AppConfig::addConfig(const std::string& key, const std::string& value) {
  this->platformConfig[key] = value;
}

std::string AppConfig::getPlatformSpecificConfig(const std::string& key) {
  auto it = platformConfig.find(key);
  if (it != platformConfig.end()) {
    return it->second;
  }
  if (key == "bundleID") {
    return "com.tencent.libpag.viewer";
  }
  return "";
}

PAGViewerCheck::PAGViewerCheck(std::shared_ptr<AppConfig> config) : config(config) {
}

bool PAGViewerCheck::isPAGViewerInstalled() {
  return FindPAGViewerBundle(config) != nil;
}

PackageInfo PAGViewerCheck::getPackageInfo() {
  PackageInfo info = {};
  @autoreleasepool {
    NSBundle* bundle = FindPAGViewerBundle(config);
    if (bundle) {
      NSString* displayName = [bundle objectForInfoDictionaryKey:@"CFBundleDisplayName"];
      if (!displayName || [displayName length] == 0) {
        displayName = [bundle objectForInfoDictionaryKey:@"CFBundleName"];
      }

      NSString* version = [bundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
      NSString* bundleVersion = [bundle objectForInfoDictionaryKey:@"CFBundleVersion"];

      if (displayName) {
        info.displayName = [displayName UTF8String];
      }

      if (version) {
        info.version = [version UTF8String];
        if (bundleVersion && ![version isEqualToString:bundleVersion]) {
          info.version += " (" + std::string([bundleVersion UTF8String]) + ")";
        }
      }

      info.installLocation = [[[bundle bundleURL] path] UTF8String];
      info.bundleID = [[bundle bundleIdentifier] UTF8String];
    }
  }
  return info;
}

std::vector<PackageInfo> PAGViewerCheck::findPackageinfoByName(const std::string& namePattern) {
  std::vector<PackageInfo> results;

  if (isPAGViewerInstalled()) {
    PackageInfo info = getPackageInfo();
    if (!info.displayName.empty() && info.displayName.find(namePattern) != std::string::npos) {
      results.push_back(info);
    }
  }

  return results;
}

InstallStatus PAGViewerCheck::installPAGViewer() {
  auto installer = std::make_unique<PAGViewerInstaller>(config);

  if (progressCallback) {
    installer->setProgressCallback(progressCallback);
  }

  return installer->installPAGViewer();
}

void PAGViewerCheck::setProgressCallback(std::function<void(int)> callback) {
  progressCallback = std::move(callback);
}
}  // namespace exporter
