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

#pragma once

// clang-format off
#include <stdint.h>
#include <napi/native_api.h>
#include <native_display_soloist/native_display_soloist.h>
// clang-format on
#include <atomic>
#include <functional>
#include "rendering/utils/DisplayLink.h"
namespace pag {

class NativeDisplayLink : public DisplayLink {
 public:
  static bool InitThreadSafeFunction(napi_env env);

  explicit NativeDisplayLink(std::function<void()> callback);
  ~NativeDisplayLink() override;

  void start() override;
  void stop() override;
  void update();

 private:
  static void PAGDisplaySoloistCallback(long long timestamp, long long targetTimestamp, void* data);
  OH_DisplaySoloist* displaySoloist = nullptr;
  std::function<void()> callback = nullptr;
  std::atomic<bool> playing = false;
};
}  // namespace pag
