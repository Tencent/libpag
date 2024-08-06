/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 THL A29 Limited, a Tencent company. All rights reserved.
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
#include <native_vsync/native_vsync.h>
#include <functional>
#include <mutex>
#include "rendering/utils/DisplayLink.h"
namespace pag {

class NativeDisplayLink : public DisplayLink {
 public:
  explicit NativeDisplayLink(std::function<void()> callback);
  ~NativeDisplayLink() override;

  void start() override;
  void stop() override;

 private:
  static void PAGVSyncCallback(long long timestamp, void* data);
  OH_NativeVSync* vSync = nullptr;
  std::function<void()> callback = nullptr;
  std::atomic<bool> playing = false;
};
}  // namespace pag
