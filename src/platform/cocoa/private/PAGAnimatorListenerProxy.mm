/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#import "PAGAnimatorListenerProxy.h"

@implementation PAGAnimatorListenerProxy {
  __unsafe_unretained id<PAGViewAnimatorForwarder> forwarder;
}

- (instancetype)initWithForwarder:(id<PAGViewAnimatorForwarder>)value {
  if (self = [super init]) {
    forwarder = value;
  }
  return self;
}

- (void)detach {
  forwarder = nil;
}

- (void)onAnimationStart:(id<PAGAnimatorUpdater>)updater {
  [forwarder dispatchListenerEvent:@selector(onAnimationStart:)];
}

- (void)onAnimationEnd:(id<PAGAnimatorUpdater>)updater {
  [forwarder dispatchListenerEvent:@selector(onAnimationEnd:)];
}

- (void)onAnimationCancel:(id<PAGAnimatorUpdater>)updater {
  [forwarder dispatchListenerEvent:@selector(onAnimationCancel:)];
}

- (void)onAnimationRepeat:(id<PAGAnimatorUpdater>)updater {
  [forwarder dispatchListenerEvent:@selector(onAnimationRepeat:)];
}

- (void)onAnimationUpdate:(id<PAGAnimatorUpdater>)updater {
  [forwarder dispatchListenerEvent:@selector(onAnimationUpdate:)];
}

@end
