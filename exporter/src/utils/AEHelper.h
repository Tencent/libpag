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

#pragma once
#include <memory>
#include <string>
#include "AEGP_SuiteHandler.h"
#include "AE_GeneralPlug.h"

namespace exporter {

class AEHelper {
 public:
  static AEGP_ItemH GetActiveCompositionItem();
  static void SetSuitesAndPluginID(SPBasicSuite* basicSuite, AEGP_PluginID id);
  static std::shared_ptr<AEGP_SuiteHandler> GetSuites() {
    return Suites;
  }
  static AEGP_PluginID GetPluginID() {
    return PluginID;
  }

 private:
  static AEGP_PluginID PluginID;
  static std::shared_ptr<AEGP_SuiteHandler> Suites;
};
}  // namespace exporter
