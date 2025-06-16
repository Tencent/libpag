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
#include <iostream>
#include "platform/PlatformHelper.h"
namespace AEHelper {

AEGP_PluginID PluginID = 0L;
std::shared_ptr<AEGP_SuiteHandler> Suites = nullptr;
std::string DocumentsFolderPath = "";
std::string AeVersion = "";

int32_t MAJORVERSION = 23;

void SetSuitesAndPluginID(SPBasicSuite* basicSuite, AEGP_PluginID id) {
  Suites = std::make_shared<AEGP_SuiteHandler>(basicSuite);
  PluginID = id;
}

AEGP_ItemH GetActiveCompositionItem() {
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

AEGP_PluginID GetPluginID() {
  return PluginID;
}

std::shared_ptr<AEGP_SuiteHandler> GetSuites() {
  return Suites;
}

std::string RunScript(std::shared_ptr<AEGP_SuiteHandler> suites, AEGP_PluginID pluginID,
                      const std::string& scriptText) {
  AEGP_MemHandle scriptResult;
  AEGP_MemHandle errorResult;
  suites->UtilitySuite6()->AEGP_ExecuteScript(pluginID, scriptText.c_str(), FALSE, &scriptResult,
                                              &errorResult);
  A_char* result = nullptr;
  suites->MemorySuite1()->AEGP_LockMemHandle(scriptResult, reinterpret_cast<void**>(&result));
  std::string resultText = result;
  suites->MemorySuite1()->AEGP_FreeMemHandle(scriptResult);
  A_char* error = nullptr;
  suites->MemorySuite1()->AEGP_LockMemHandle(errorResult, reinterpret_cast<void**>(&error));
  std::string errorText = error;
  suites->MemorySuite1()->AEGP_FreeMemHandle(errorResult);
  return resultText;
}

void RegisterTextDocumentScript() {
  static bool hasInit = false;
  if (!hasInit) {
    const auto& suites = GetSuites();
    auto pluginID = GetPluginID();
    RunScript(suites, pluginID, TextDocumentScript);
    hasInit = true;
  }
}

std::string GetDocumentsFolderPath() {
  if (DocumentsFolderPath.empty()) {
    const auto& suites = GetSuites();
    auto pluginID = GetPluginID();
    DocumentsFolderPath = RunScript(suites, pluginID, "Folder.myDocuments.fsName;");
  }
  return DocumentsFolderPath;
}

std::string GetAeVersion() {
  if (AeVersion.empty()) {
    const auto& suites = GetSuites();
    auto pluginID = GetPluginID();
    AeVersion = RunScript(suites, pluginID, "app.version");
  }
  return AeVersion;
}

void RunScriptPreWarm() {
  static bool hasInit = false;
  if (!hasInit) {
    RegisterTextDocumentScript();
    GetDocumentsFolderPath();
    GetAeVersion();
    hasInit = true;
  }
}

bool CheckAeVersion() {
  int32_t majorVersion = 0;
  if (AeVersion.empty()) {
    return false;
  }
  try {
    size_t dotPos = AeVersion.find('.');
    if (dotPos == std::string::npos) {
      std::cerr << "Invalid version format" << std::endl;
      return false;
    }
    std::string versionStr = AeVersion.substr(0, dotPos);
    majorVersion = std::stoi(versionStr);
  } catch (const std::invalid_argument& e) {
    std::cerr << "Invalid argument: " << e.what() << std::endl;
    return false;
  } catch (const std::out_of_range& e) {
    std::cerr << "Out of range: " << e.what() << std::endl;
    return false;
  }
  if (majorVersion >= MAJORVERSION) {
    return true;
  }
  return false;
}

}  // namespace AEHelper
