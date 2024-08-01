//
// Created on 2024/7/29.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include "NativeDisplayLink.h"

namespace pag {

void NativeDisplayLink::PAGVSyncCallback(long long, void* data) {
  auto displayLink = static_cast<NativeDisplayLink*>(data);
  displayLink->callback();
  {
    std::lock_guard lock_guard(displayLink->locker);
    if (displayLink->playing) {
      OH_NativeVSync_RequestFrame(displayLink->vSync, &PAGVSyncCallback, displayLink);
    }
  }
}

NativeDisplayLink::NativeDisplayLink(std::function<void()> callback) : callback(callback) {
  char name[] = "pag_vsync";
  vSync = OH_NativeVSync_Create(name, strlen(name));
}

void NativeDisplayLink::start() {
  std::lock_guard lock_guard(locker);
  if (playing == false) {
    playing = true;
    OH_NativeVSync_RequestFrame(vSync, &PAGVSyncCallback, this);
  }
}

void NativeDisplayLink::stop() {
  std::lock_guard lock_guard(locker);
  playing = false;
}

NativeDisplayLink::~NativeDisplayLink() {
  OH_NativeVSync_Destroy(vSync);
}

}  // namespace pag