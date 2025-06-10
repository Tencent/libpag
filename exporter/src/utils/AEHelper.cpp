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

#include "AEHelper.h"

namespace exporter {

AEGP_PluginID AEHelper::PluginID = 0L;
std::shared_ptr<AEGP_SuiteHandler> AEHelper::Suites = nullptr;

void AEHelper::SetSuitesAndPluginID(SPBasicSuite* basicSuite, AEGP_PluginID id) {
  Suites = std::make_unique<AEGP_SuiteHandler>(basicSuite);
  PluginID = id;
}

AEGP_ItemH AEHelper::GetActiveCompositionItem() {
  const auto& suites = GetSuites();
  AEGP_ItemH activeItemH = nullptr;
  suites->ItemSuite6()->AEGP_GetActiveItem(&activeItemH);
  if (activeItemH == nullptr) {
    AEGP_ProjectH projectHandle;
    suites->ProjSuite6()->AEGP_GetProjectByIndex(0, &projectHandle);
    suites->ItemSuite6()->AEGP_GetFirstProjItem(projectHandle, &activeItemH);
  }
  if (activeItemH == nullptr) {
    return nullptr;
  }
  AEGP_ItemType itemType = AEGP_ItemType_NONE;
  suites->ItemSuite6()->AEGP_GetItemType(activeItemH, &itemType);
  if (itemType != AEGP_ItemType_COMP) {
    return nullptr;
  }
  return activeItemH;
}

}  // namespace exporter
