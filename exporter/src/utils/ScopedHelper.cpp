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
#include "ScopedHelper.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include "AEHelper.h"
namespace fs = std::filesystem;

namespace exporter {

ScopedTimeSetter::ScopedTimeSetter(const AEGP_ItemH& itemHandle, float time)
    : address(itemHandle), itemHandle(itemHandle) {
  const auto& suites = GetSuites();
  suites->ItemSuite8()->AEGP_GetItemCurrentTime(itemHandle, &orgTime);

  A_Time newTime = {static_cast<A_long>(time * 100), 100};
  suites->ItemSuite8()->AEGP_SetItemCurrentTime(itemHandle, &newTime);
}

ScopedTimeSetter::~ScopedTimeSetter() {
  const auto& suites = GetSuites();
  if (itemHandle == nullptr || itemHandle != address) {
    return;
  }
  suites->ItemSuite8()->AEGP_SetItemCurrentTime(itemHandle, &orgTime);
}

}  // namespace exporter
