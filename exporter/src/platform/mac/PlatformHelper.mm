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

#include "platform/PlatformHelper.h"
#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#include <dlfcn.h>
#include <QDir>
#include <QFileInfo>
#include <string>
#include "platform/PAGViewerCheck.h"
#include "platform/PAGViewerInstaller.h"
#include "ui/WindowManager.h"

std::string TempFolderPath = "";
namespace exporter {
std::string GetRoamingPath() {
  NSArray* arr =
      NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
  if ([arr count] == 0) {
    return "";
  }
  NSString* basePath = [arr objectAtIndex:0];
  NSString* path = [basePath stringByAppendingPathComponent:@""];
  NSString* retainedPath = [path retain];
  std::string result = std::string([retainedPath UTF8String]);
  [retainedPath release];
  return result;
}

std::string GetConfigPath() {
  auto path = GetRoamingPath() + "PAGExporter/";
  NSFileManager* fileManager = [NSFileManager defaultManager];
  NSString* nsPath = [NSString stringWithUTF8String:path.c_str()];
  if (![fileManager fileExistsAtPath:nsPath]) {
    NSError* error = nil;
    if (![fileManager createDirectoryAtPath:nsPath
                withIntermediateDirectories:YES
                                 attributes:nil
                                      error:&error]) {
      NSLog(@"Failed to create directory: %@", error);
      return "";
    }
  }
  return path;
}

std::string GetTempFolderPath() {
  if (TempFolderPath.empty()) {
    NSString* tempDir = NSTemporaryDirectory();
    if (tempDir) {
      TempFolderPath = std::string([tempDir UTF8String]);
    } else {
      TempFolderPath = "/tmp/";
    }
  }
  return TempFolderPath;
}

bool IsAEWindowActive() {
  NSRunningApplication* frontApp = [[NSWorkspace sharedWorkspace] frontmostApplication];
  return [frontApp.bundleIdentifier containsString:@"com.adobe.AfterEffects"];
}

std::string GetQmlPath() {
  // Use shared Qt resources directory to avoid modifying plugin bundle (which would break code
  // signature)
  return "/Library/Application Support/PAGExporter/Resources/qml";
}

std::string GetDownloadsPath() {
  @autoreleasepool {
    NSArray* paths =
        NSSearchPathForDirectoriesInDomains(NSDownloadsDirectory, NSUserDomainMask, YES);

    if (paths.count > 0) {
      NSString* downloadsPath = [paths objectAtIndex:0];
      return std::string([downloadsPath UTF8String]);
    }

    NSString* homeDir = NSHomeDirectory();
    if (homeDir) {
      NSString* downloadsPath = [homeDir stringByAppendingPathComponent:@"Downloads"];
      return std::string([downloadsPath UTF8String]);
    }

    return "";
  }
}

static void StartPreview(const std::string& pagFilePath) {
  @autoreleasepool {
    NSString* nsFilePath = [NSString stringWithCString:pagFilePath.c_str()
                                              encoding:NSUTF8StringEncoding];
    if (!nsFilePath) {
      QString errorMsg = QString::fromUtf8(Messages::FILE_PATH_ENCODING_ERROR);
      WindowManager::GetInstance().showSimpleError(errorMsg);
      return;
    }

    NSFileManager* fileManager = [NSFileManager defaultManager];
    if (![fileManager fileExistsAtPath:nsFilePath]) {
      QString errorMsg =
          QString::fromUtf8(Messages::FILE_NOT_EXIST) + QString::fromStdString(pagFilePath);
      WindowManager::GetInstance().showSimpleError(errorMsg);
      return;
    }

    NSString* bundleId = @"com.tencent.libpag.viewer";
    NSURL* appURL = [[NSWorkspace sharedWorkspace] URLForApplicationWithBundleIdentifier:bundleId];

    if (!appURL) {
      NSString* fallbackAppPath = @"/Applications/PAGViewer.app";
      if ([fileManager fileExistsAtPath:fallbackAppPath]) {
        appURL = [NSURL fileURLWithPath:fallbackAppPath];
      }
    }

    if (!appURL) {
      QString errorMsg = QString::fromUtf8(Messages::PAGVIEWER_NOT_FOUND_MAC);
      WindowManager::GetInstance().showSimpleError(errorMsg);
      return;
    }

    NSURL* fileURL = [NSURL fileURLWithPath:nsFilePath];
    if (!fileURL) {
      QString errorMsg = QString::fromUtf8(Messages::INVALID_FILE_PATH);
      WindowManager::GetInstance().showSimpleError(errorMsg);
      return;
    }

    if (@available(macOS 10.15, *)) {
      NSWorkspaceOpenConfiguration* configuration = [NSWorkspaceOpenConfiguration new];
      [[NSWorkspace sharedWorkspace] openURLs:@[ fileURL ]
                         withApplicationAtURL:appURL
                                configuration:configuration
                            completionHandler:^(NSRunningApplication* _Nullable __unused app,
                                                NSError* _Nullable error) {
                              if (error) {
                                dispatch_async(dispatch_get_main_queue(), ^{
                                  QString errorMsg =
                                      QString::fromUtf8(Messages::PAGVIEWER_OPEN_FAILED) +
                                      QString::fromUtf8([error.localizedDescription UTF8String]);
                                  WindowManager::GetInstance().showSimpleError(errorMsg);
                                });
                              }
                            }];
    }
  }
}

void PreviewPAGFile(std::string pagFilePath) {
  auto config = std::make_shared<AppConfig>();
  config->setAppName("PAGViewer.app");
  auto installer = std::make_unique<PAGViewerInstaller>(config);

  if (!installer->isPAGViewerInstalled()) {
    bool installSuccess = WindowManager::GetInstance().showPAGViewerInstallDialog(pagFilePath);

    if (!installSuccess) {
      return;
    }
  }
  StartPreview(pagFilePath);
}

std::filesystem::path Utf8ToPath(const std::string& utf8) {
  return {utf8};
}

std::string PathToUtf8(const std::filesystem::path& path) {
  return path.u8string();
}

}
