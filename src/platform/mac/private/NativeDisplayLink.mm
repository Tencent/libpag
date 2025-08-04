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
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

static CVReturn OnAnimationCallback(CVDisplayLinkRef, const CVTimeStamp*, const CVTimeStamp*,
                                    CVOptionFlags, CVOptionFlags*, void* userInfo) {
  reinterpret_cast<NativeDisplayLink*>(userInfo)->update();
  return kCVReturnSuccess;
}

NativeDisplayLink::NativeDisplayLink(std::function<void()> callback)
    : callback(std::move(callback)) {
  CVDisplayLinkCreateWithActiveCGDisplays(&displayLink);
  CVDisplayLinkSetOutputCallback(displayLink, &OnAnimationCallback, this);
}

NativeDisplayLink::~NativeDisplayLink() {
  stop();
  CVDisplayLinkRelease(displayLink);
}

void NativeDisplayLink::start() {
  if (started) {
    return;
  }
  started = true;
  CVDisplayLinkStart(displayLink);
}

void NativeDisplayLink::stop() {
  if (!started) {
    return;
  }
  started = false;
  CVDisplayLinkStop(displayLink);
}

void NativeDisplayLink::update() {
  callback();
}

#pragma clang diagnostic pop
}  // namespace pag
