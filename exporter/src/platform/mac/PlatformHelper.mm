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
#import <Foundation/Foundation.h>
#include <string>
#include "platform/PAGViewerCheck.h"
#include "platform/PAGViewerInstaller.h"
#include "ui/WindowManager.h"

std::string TempFolderPath = "";
namespace exporter {
std::string GetRoamingPath() {
  NSArray* arr =
      NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
  if (arr.count == 0) {
    return "";
  }
  NSString* path = [arr[0] stringByAppendingPathComponent:@""];
  return std::string([path UTF8String]);
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

static void executePreviewLogic(const std::string& pagFilePath) {
  @autoreleasepool {
    NSString* nsFilePath = [NSString stringWithCString:pagFilePath.c_str()
                                              encoding:NSUTF8StringEncoding];
    if (!nsFilePath) {
      QString errorMsg = QString::fromUtf8("文件路径编码错误，无法预览文件。");
      WindowManager::GetInstance().showSimpleError(errorMsg);
      return;
    }

    NSFileManager* fileManager = [NSFileManager defaultManager];
    if (![fileManager fileExistsAtPath:nsFilePath]) {
      QString errorMsg =
          QString::fromUtf8("文件不存在，无法预览：") + QString::fromStdString(pagFilePath);
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
      QString errorMsg = QString::fromUtf8("PAGViewer.app not found.");
      WindowManager::GetInstance().showSimpleError(errorMsg);
      return;
    }

    NSURL* fileURL = [NSURL fileURLWithPath:nsFilePath];
    if (!fileURL) {
      QString errorMsg = QString::fromUtf8("无效的文件路径，无法创建预览。");
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
                                      QString::fromUtf8("使用PAGViewer打开文件失败：") +
                                      QString::fromUtf8([error.localizedDescription UTF8String]);
                                  WindowManager::GetInstance().showSimpleError(errorMsg);
                                });
                              }
                            }];
    }
  }
}
void PreviewPAGFile(std::string pagFilePath) {
  auto config = std::make_shared<CheckConfig>();
  config->SetTargetAppName("PAGViewer.app");
  auto installer = std::make_unique<PAGViewerInstaller>(config);

  if (!installer->IsPAGViewerInstalled()) {
    bool installSuccess = WindowManager::GetInstance().showPAGViewerInstallDialog(pagFilePath);

    if (!installSuccess) {
      return;
    }
  }
  executePreviewLogic(pagFilePath);
}
}