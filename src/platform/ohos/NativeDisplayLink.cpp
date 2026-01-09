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
#include <mutex>
#include "base/utils/USE.h"
namespace pag {

static std::once_flag init_flag;
static napi_threadsafe_function js_threadsafe_function = nullptr;
void NativeDisplayLink::PAGDisplaySoloistCallback(long long timestamp, long long targetTimestamp,
                                                  void* data) {
  USE(timestamp);
  USE(targetTimestamp);
  if (js_threadsafe_function == nullptr) {
    return;
  }
  napi_call_threadsafe_function(js_threadsafe_function, data, napi_tsfn_blocking);
}

static void CallJsFunction(napi_env env, napi_value callBack, void* context, void* data) {
  USE(env);
  USE(callBack);
  USE(context);
  auto displayLink = static_cast<NativeDisplayLink*>(data);
  if (displayLink) {
    displayLink->update();
  }
};

bool NativeDisplayLink::InitThreadSafeFunction(napi_env env) {
  std::call_once(init_flag, [env] {
    napi_value resourceName = nullptr;
    napi_create_string_utf8(env, "NativeDisplayLink Safe Function", NAPI_AUTO_LENGTH,
                            &resourceName);
    napi_create_threadsafe_function(env, nullptr, nullptr, resourceName, 0, 1, nullptr, nullptr,
                                    nullptr, CallJsFunction, &js_threadsafe_function);
  });
  return js_threadsafe_function != nullptr;
}

NativeDisplayLink::NativeDisplayLink(std::function<void()> callback) : callback(callback) {
  displaySoloist = OH_DisplaySoloist_Create(true);
  DisplaySoloist_ExpectedRateRange expectedRateRange{60, 120, 120};
  OH_DisplaySoloist_SetExpectedFrameRateRange(displaySoloist, &expectedRateRange);
}

void NativeDisplayLink::start() {
  if (playing == false) {
    playing = true;
    OH_DisplaySoloist_Start(displaySoloist, &PAGDisplaySoloistCallback, this);
  }
}

void NativeDisplayLink::stop() {
  playing = false;
  OH_DisplaySoloist_Stop(displaySoloist);
}

void NativeDisplayLink::update() {
  if (playing) {
    callback();
  }
}

NativeDisplayLink::~NativeDisplayLink() {
  OH_DisplaySoloist_Destroy(displaySoloist);
  if (js_threadsafe_function != nullptr) {
    napi_release_threadsafe_function(js_threadsafe_function, napi_tsfn_release);
    js_threadsafe_function = nullptr;
  }
}

}  // namespace pag