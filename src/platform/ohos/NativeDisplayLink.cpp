/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 Tencent. All rights reserved.
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

void NativeDisplayLink::PAGVSyncCallback(long long, void* data) {
  auto displayLink = static_cast<NativeDisplayLink*>(data);
  displayLink->callback();
  if (displayLink->playing) {
    OH_NativeVSync_RequestFrame(displayLink->vSync, &PAGVSyncCallback, displayLink);
  }
}

NativeDisplayLink::NativeDisplayLink(std::function<void()> callback) : callback(callback) {
  char name[] = "pag_vsync";
  vSync = OH_NativeVSync_Create(name, strlen(name));
}

void NativeDisplayLink::start() {
  if (playing == false) {
    playing = true;
    OH_NativeVSync_RequestFrame(vSync, &PAGVSyncCallback, this);
  }
}

void NativeDisplayLink::stop() {
  playing = false;
}

NativeDisplayLink::~NativeDisplayLink() {
  OH_NativeVSync_Destroy(vSync);
}

}  // namespace pag