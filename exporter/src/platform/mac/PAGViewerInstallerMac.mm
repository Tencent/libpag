#include "platform/PAGViewerInstaller.h"

#ifdef Q_OS_MAC
#import <CoreServices/CoreServices.h>
#import <Foundation/Foundation.h>

namespace exporter {

bool PAGViewerInstaller::IsPAGViewerInstalledMac() {
  @autoreleasepool {
    std::string bundleIDString = config->GetPlatformSpecificConfig("bundleID");
    if (bundleIDString.empty()) {
      bundleIDString = "com.tencent.libpag.viewer";
    }

    NSString* bundleIDStr = [NSString stringWithUTF8String:bundleIDString.c_str()];
    CFStringRef bundleID = (__bridge CFStringRef)bundleIDStr;
    CFArrayRef appURLs = LSCopyApplicationURLsForBundleIdentifier(bundleID, NULL);

    bool isInstalled = false;
    if (appURLs) {
      isInstalled = (CFArrayGetCount(appURLs) > 0);
      CFRelease(appURLs);
    }

    return isInstalled;
  }
}

bool PAGViewerInstaller::copyToApplicationsMac(const QString& sourcePath) {
  @autoreleasepool {
    NSString* source = [NSString stringWithUTF8String:sourcePath.toUtf8().constData()];
    NSString* destination = @"/Applications/PAGViewer.app";

    NSFileManager* fileManager = [NSFileManager defaultManager];

    if ([fileManager fileExistsAtPath:destination]) {
      NSError* error = nil;
      if (![fileManager removeItemAtPath:destination error:&error]) {
        return false;
      }
    }

    NSError* error = nil;
    return [fileManager copyItemAtPath:source toPath:destination error:&error];
  }
}

}

#endif