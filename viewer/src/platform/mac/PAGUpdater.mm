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

#include "PAGUpdater.h"
#include <Cocoa/Cocoa.h>
#import <Sparkle/Sparkle.h>
#include "rendering/PAGWindow.h"
#include "utils/Utils.h"

@interface PAGUpdaterDelegate : NSObject <SPUUpdaterDelegate>
@property BOOL showUI;
@property(nonatomic, copy) NSString* feedUrl;
@end

@implementation PAGUpdaterDelegate

- (NSString*)feedURLStringForUpdater:(SPUUpdater*)updater {
  return self.feedUrl;
}

- (bool)updaterDidFindValidUpdate:(SPUUpdater*)updater {
  for (int i = 0; i < pag::PAGWindow::AllWindows.count(); i++) {
    auto window = pag::PAGWindow::AllWindows[i];
    auto root = window->getEngine()->rootObjects().first();
    if (root) {
      QMetaObject::invokeMethod(root, "updateAvailable", Q_ARG(QVariant, true));
    }
  }
  return self.showUI;
}

@end

static PAGUpdaterDelegate* updaterDelegate = nil;
static SPUStandardUpdaterController* updaterController = nil;

namespace pag {

void InitUpdater() {
  if (updaterDelegate == nil) {
    updaterDelegate = [[PAGUpdaterDelegate alloc] init];
  }

  if (updaterController == nil) {
    updaterController =
        [[SPUStandardUpdaterController alloc] initWithStartingUpdater:NO
                                                      updaterDelegate:updaterDelegate
                                                   userDriverDelegate:nil];

    SPUUpdater* updater = updaterController.updater;
    updater.automaticallyChecksForUpdates = NO;
    updater.automaticallyDownloadsUpdates = NO;
  }
}

void CheckForUpdates(bool keepSilent, const std::string& url) {
  if (updaterController == nil) {
    qDebug() << "Updater is not initialized";
    return;
  }

  @autoreleasepool {
    if (updaterDelegate) {
      updaterDelegate.showUI = !keepSilent;
      updaterDelegate.feedUrl = [NSString stringWithUTF8String:url.c_str()];
    }

    SPUUpdater* updater = updaterController.updater;
    if (updater) {
      if (![updater canCheckForUpdates]) {
        [updaterController startUpdater];
      }

      if ([updater canCheckForUpdates]) {
        if (keepSilent) {
          [updater checkForUpdatesInBackground];
        } else {
          [updaterController checkForUpdates:nil];
        }
      } else {
        qDebug() << "Error: Updater is not ready to check for updates";
      }
    }
  }
}

}  // namespace pag
