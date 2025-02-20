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

#include "AECommand.h"
#include <iostream>
#include "AEConfig.h"
#include "AEGP_SuiteHandler.h"
#include "src/utils/AEUtils.h"
#include "src/ui/qt/EnvConfig.h"

A_Err OnUpdateMenu(AEGP_GlobalRefcon globalRefcon, AEGP_UpdateMenuRefcon menuRefcon,
                   AEGP_WindowType windowType) {
  A_Err err = A_Err_NONE, err2 = A_Err_NONE;
  auto& suites = SUITES();
  AEGP_ItemH active_itemH = AEUtils::GetActiveCompositionItem();
  if (active_itemH) {
    ERR(suites.CommandSuite1()->AEGP_EnableCommand(PAGExporterCMD));
    ERR(suites.CommandSuite1()->AEGP_EnableCommand(PAGPreviewCMD));
  } else {
    ERR2(suites.CommandSuite1()->AEGP_DisableCommand(PAGExporterCMD));
    ERR2(suites.CommandSuite1()->AEGP_DisableCommand(PAGPreviewCMD));
  }

  ERR(suites.CommandSuite1()->AEGP_EnableCommand(PAGConfigCMD));
  return err;
}

A_Err OnClickConfig(AEGP_GlobalRefcon globalRefcon, AEGP_CommandRefcon commandRefcon,
                    AEGP_Command command, AEGP_HookPriority hookPriority, A_Boolean alreadyHandled,
                    A_Boolean* handled) {

  A_Err err = A_Err_NONE;
  if (command != PAGConfigCMD) {
    return err;
  }
  *handled = TRUE;

  // ExportUtils::SetLanguage();
  //
  SetupQT();
  // // 显示配置面板
  // if (!AEUtils::RunOpenConfigPanelScript()) {
  //   ShowConfigWindow();
  // }
  return err;
}
