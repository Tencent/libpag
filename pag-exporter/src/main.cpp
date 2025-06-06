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

#include <iostream>
#include "AEConfig.h"
#include "AEGP_SuiteHandler.h"
#include "AE_GeneralPlug.h"
#include "AE_Macros.h"
#include "aecmd/AECommand.h"
#include "src/utils/AEUtils.h"

extern "C" DllExport A_Err EntryPointFunc(struct SPBasicSuite* suite, A_long majorVersion,
                                          A_long minorVersion, AEGP_PluginID pluginID,
                                          AEGP_GlobalRefcon* globalRefcon) {
  AEUtils::SetSuitesAndPluginID(suite, pluginID);
  A_Err err = A_Err_NONE;
  A_Err err2 = A_Err_NONE;
  const auto& suites = SUITES();

  // PAG Config...
  ERR(suites.CommandSuite1()->AEGP_GetUniqueCommand(&AECommand::PAGConfigCMD));
  ERR(suites.CommandSuite1()->AEGP_InsertMenuCommand(
      AECommand::PAGConfigCMD, "PAG Config...",
      AEGP_Menu_PREFS,  // "After Effect CC" -> "首选项"
      AEGP_MENU_INSERT_AT_BOTTOM));
  ERR(suites.RegisterSuite5()->AEGP_RegisterCommandHook(
      pluginID, AEGP_HP_BeforeAE, AEGP_Command_ALL, AECommand::OnClickConfig, nullptr));
  //PAG File (Panel)...
  ERR(suites.CommandSuite1()->AEGP_GetUniqueCommand(&AECommand::PAGPanelCMD));
  ERR(suites.CommandSuite1()->AEGP_InsertMenuCommand(AECommand::PAGPanelCMD, "PAG File (Panel)...",
                                                     AEGP_Menu_EXPORT, AEGP_MENU_INSERT_SORTED));
  ERR(suites.RegisterSuite5()->AEGP_RegisterCommandHook(
      pluginID, AEGP_HP_BeforeAE, AEGP_Command_ALL, AECommand::OnClickPanel, nullptr));

  // PAG File...
  ERR(suites.CommandSuite1()->AEGP_GetUniqueCommand(&AECommand::PAGExporterCMD));
  ERR(suites.CommandSuite1()->AEGP_InsertMenuCommand(AECommand::PAGExporterCMD,
                                                     "PAG File...",
                                                     AEGP_Menu_EXPORT,
                                                     AEGP_MENU_INSERT_SORTED));
  ERR(suites.RegisterSuite5()->AEGP_RegisterCommandHook(pluginID,
                                                        AEGP_HP_BeforeAE,
                                                        AEGP_Command_ALL,
                                                        AECommand::OnClickExporter,
                                                        nullptr));

  // PAG Preview...
  ERR(suites.CommandSuite1()->AEGP_GetUniqueCommand(&AECommand::PAGPreviewCMD));
  ERR(suites.CommandSuite1()->AEGP_InsertMenuCommand(AECommand::PAGPreviewCMD,
                                                     "PAG Preview...",
                                                     AEGP_Menu_EXPORT,
                                                     AEGP_MENU_INSERT_SORTED));
  ERR(suites.RegisterSuite5()->AEGP_RegisterCommandHook(pluginID,
                                                        AEGP_HP_BeforeAE,
                                                        AEGP_Command_ALL,
                                                        AECommand::OnClickPreview,
                                                        nullptr));


  ERR(suites.RegisterSuite5()->AEGP_RegisterUpdateMenuHook(pluginID, AECommand::OnUpdateMenu,
                                                           nullptr));
  if (err) {
    ERR2(suites.UtilitySuite3()->AEGP_ReportInfo(pluginID,
                                                 "PAGExporter : Could not register command hook."));
  }
  return err;
}
