/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "NativeDisplayLink.h"

namespace pag {
NativeDisplayLink::NativeDisplayLink(std::function<void()> callback) {
  animationCallback = [[PAGAnimationCallback alloc] initWithCallback:callback];
}

NativeDisplayLink::~NativeDisplayLink() {
  stop();
  [animationCallback release];
}

void NativeDisplayLink::start() {
  if (displayLink != nullptr) {
    return;
  }
  displayLink = [CADisplayLink displayLinkWithTarget:animationCallback selector:@selector(update:)];
  // The default mode was previously set here. However, rendering is not possible when the UI is in
  // drag mode. Therefore, it has been changed to common modes.
  [displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
}

void NativeDisplayLink::stop() {
  [displayLink invalidate];
  displayLink = nullptr;
}
}  // namespace pag
