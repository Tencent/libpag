/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
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
#ifndef PROJECTPLAYTIME_H
#define PROJECTPLAYTIME_H
#include "AEUtils.h"
#include "PAGDataTypes.h"

class ProjectPlayTime {
 public:
  ProjectPlayTime(const AEGP_ItemH& itemHandle, float timeS) : itemHandle(itemHandle) {
    auto& suites = SUITES();
    // 记录原来播放的时间
    A_Time orgTime = {0, 0};
    suites.ItemSuite8()->AEGP_GetItemCurrentTime(itemHandle, &orgTime);

    // 设置新的播放时间
    A_Time newTime = {static_cast<A_long>(timeS * 100), 100};
    suites.ItemSuite8()->AEGP_SetItemCurrentTime(itemHandle, &newTime);
  }

  ~ProjectPlayTime() {
    auto& suites = SUITES();
    // 结束时复原原来的播放时间
    suites.ItemSuite8()->AEGP_SetItemCurrentTime(itemHandle, &orgTime);
  }

 private:
  const AEGP_ItemH& itemHandle;
  A_Time orgTime = {0, 100};
};
#endif  //PROJECTPLAYTIME_H
