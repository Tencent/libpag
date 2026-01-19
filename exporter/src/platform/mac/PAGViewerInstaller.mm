#include "platform/PAGViewerInstaller.h"
#include <QFile>
#include <QProcess>
#include "platform/PAGViewerCheck.h"

#ifdef Q_OS_MAC
#import <CoreServices/CoreServices.h>
#import <Foundation/Foundation.h>

namespace exporter {

bool PAGViewerInstaller::isPAGViewerInstalled() {
  @autoreleasepool {
    std::string bundleIDString = config->getPlatformSpecificConfig("bundleID");
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

bool PAGViewerInstaller::copyToApplications(const QString& sourcePath) {
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

InstallStatus PAGViewerInstaller::executeInstall(const QString& zipPath) {
  QProcess unzipProcess;
  QStringList arguments;

  unzipProcess.setProgram("unzip");
  arguments << "-o"
            << "-q" << zipPath << "-d" << tempDir;
  unzipProcess.setArguments(arguments);
  unzipProcess.start();
  unzipProcess.waitForFinished(UNZIP_PROCESS_TIMEOUT_MS);

  if (unzipProcess.exitCode() != 0) {
    QString error = unzipProcess.readAllStandardError();
    QString output = unzipProcess.readAllStandardOutput();
    return InstallStatus(InstallResult::ExecutionFailed, "unzip failed: " + error.toStdString());
  }

  if (progressCallback) {
    progressCallback(80);
  }

  QString appPath = tempDir + "/PAGViewer.app";
  QFile appFile(appPath);
  if (!appFile.exists()) {
    return InstallStatus(InstallResult::ExecutionFailed, "can not find file after unzip");
  }

  if (!copyToApplications(appPath)) {
    return InstallStatus(InstallResult::AccessDenied, "install PAGViewer failed");
  }

  if (progressCallback) {
    progressCallback(90);
  }
  return InstallStatus(InstallResult::Success);
}

}

#endif
