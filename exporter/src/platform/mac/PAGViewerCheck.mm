#include "platform/PAGViewerCheck.h"
#import <Cocoa/Cocoa.h>
#import <CoreServices/CoreServices.h>
#include <string>
#include <unordered_map>
#include "platform/PAGViewerInstaller.h"
#include "platform/PlatformHelper.h"
#include "utils/FileHelper.h"

namespace exporter {

static NSBundle* findPAGViewerBundle(const std::shared_ptr<CheckConfig>& config) {
  @autoreleasepool {
    std::string bundleIDString = config->GetPlatformSpecificConfig("bundleID");
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

void CheckConfig::SetTargetAppName(const std::string& name) {
  this->targetAppName = name;
}

std::string CheckConfig::GetTargetAppName() {
  return this->targetAppName;
}

void CheckConfig::SetInstallerPath(const std::string& path) {
  this->installerPath = path;
}

std::string CheckConfig::GetInstallerPath() {
  if (!this->installerPath.empty()) {
    return this->installerPath;
  }
  @autoreleasepool {
    std::string downloadsPathStr = GetDownloadsPath();
    NSString* downloadsPath = [NSString stringWithUTF8String:downloadsPathStr.c_str()];
    NSString* fullPath = [downloadsPath stringByAppendingPathComponent:@"PAGViewer.dmg"];
    return [fullPath UTF8String];
  }
}

void CheckConfig::SetPlatformSpecificConfig(const std::string& key, const std::string& value) {
  this->platformConfig[key] = value;
}

std::string CheckConfig::GetPlatformSpecificConfig(const std::string& key) {
  if (key == "bundleID") {
    auto it = this->platformConfig.find(key);
    return (it != this->platformConfig.end()) ? it->second : "com.tencent.libpag.viewer";
  }
  auto it = this->platformConfig.find(key);
  return (it != this->platformConfig.end()) ? it->second : "";
}

PAGViewerCheck::PAGViewerCheck(std::shared_ptr<CheckConfig> config) : config(config) {
}

bool PAGViewerCheck::IsPAGViewerInstalled() {
  return findPAGViewerBundle(config) != nil;
}

SoftWareInfo PAGViewerCheck::GetPAGViewerInfo() {
  SoftWareInfo info = {};
  @autoreleasepool {
    NSBundle* bundle = findPAGViewerBundle(config);
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

std::vector<SoftWareInfo> PAGViewerCheck::FindSoftwareByName(const std::string& namePattern) {
  std::vector<SoftWareInfo> results;

  if (IsPAGViewerInstalled()) {
    SoftWareInfo info = GetPAGViewerInfo();
    if (!info.displayName.empty() && info.displayName.find(namePattern) != std::string::npos) {
      results.push_back(info);
    }
  }

  return results;
}

InstallStatus PAGViewerCheck::InstallPAGViewer() {
  auto installer = std::make_unique<PAGViewerInstaller>(config);

  if (progressCallback) {
    installer->setProgressCallback(progressCallback);
  }

  return installer->InstallPAGViewer();
}

void PAGViewerCheck::setProgressCallback(std::function<void(int)> callback) {
  progressCallback = std::move(callback);
}
}  // namespace exporter