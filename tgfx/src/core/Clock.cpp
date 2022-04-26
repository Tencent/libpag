/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "tgfx/core/Clock.h"
#include "core/utils/Log.h"

namespace tgfx {
int64_t Clock::Now() {
  static const auto START_TIME = std::chrono::high_resolution_clock::now();
  auto now = std::chrono::high_resolution_clock::now();
  auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - START_TIME);
  return static_cast<int64_t>(ns.count() * 1e-3);
}

Clock::Clock() {
  startTime = Now();
}

void Clock::reset() {
  startTime = Now();
  markers = {};
}

void Clock::mark(const std::string& name) {
  if (name.empty()) {
    LOGE("Clock::mark(): An empty marker name was specified!");
    return;
  }
  auto result = markers.find(name);
  if (result != markers.end()) {
    LOGE("Clock::mark(): The specified marker name '%s' already exists!", name.c_str());
    return;
  }
  markers[name] = Now();
}

int64_t Clock::measure(const std::string& makerFrom, const std::string& makerTo) const {
  int64_t start, end;
  if (!makerFrom.empty()) {
    auto result = markers.find(makerFrom);
    if (result == markers.end()) {
      LOGE("Clock::measure(): The specified makerFrom '%s' does not exist!", makerFrom.c_str());
      return 0;
    }
    start = result->second;
  } else {
    start = startTime;
  }
  if (!makerTo.empty()) {
    auto result = markers.find(makerTo);
    if (result == markers.end()) {
      LOGE("Clock::measure(): The specified makerTo '%s' does not exist!", makerTo.c_str());
      return 0;
    }
    end = result->second;
  } else {
    end = Now();
  }
  return end - start;
}
}  // namespace tgfx
