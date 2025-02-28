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
#include <QApplication>
#include <QtWidgets/QApplication>
#include <iostream>
#include <string>
#include "AEConfig.h"
#include "AEGP_SuiteHandler.h"
#include "AE_GeneralPlug.h"
#include "src/ui/qt/PAGConfigDialog/PAGConfigDialog.h"
#include "src/ui/qt/PAGPanelExporterDialog/PAGPanelExporterDialog.h"
#include "src/configparam/ConfigParam.h"
#include "src/ui/qt/EnvConfig.h"
#include "src/utils/AEUtils.h"
#include "src/utils/AEResource.h"
AEGP_Command AECommand::PAGExporterCMD = 0L;
AEGP_Command AECommand::PAGConfigCMD = 0L;
AEGP_Command AECommand::PAGPanelCMD = 0L;
AEGP_Command AECommand::PAGPreviewCMD = 0L;

A_Err AECommand::OnUpdateMenu(AEGP_GlobalRefcon globalRefcon, AEGP_UpdateMenuRefcon menuRefcon,
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
  if (AEResource::HasCompositionResource()) {
    ERR(suites.CommandSuite1()->AEGP_EnableCommand(PAGPanelCMD));
  } else {
    ERR2(suites.CommandSuite1()->AEGP_DisableCommand(PAGPanelCMD));
  }
  return err;
}


A_Err AECommand::OnClickConfig(AEGP_GlobalRefcon globalRefcon, AEGP_CommandRefcon commandRefcon,
                               AEGP_Command command, AEGP_HookPriority hookPriority,
                               A_Boolean alreadyHandled, A_Boolean* handled) {

  A_Err err = A_Err_NONE;
  if (command != PAGConfigCMD) {
    return A_Err_PROJECT_LOAD_FATAL;
  }
  *handled = TRUE;
  SetupQT();
  PAGConfigDialog dialog;
  dialog.exec();
  return err;
}

A_Err AECommand::OnClickPanel(AEGP_GlobalRefcon globalRefcon, AEGP_CommandRefcon commandRefcon,
                              AEGP_Command command, AEGP_HookPriority hookPriority,
                              A_Boolean alreadyHandled, A_Boolean* handled) {

  A_Err err = A_Err_NONE;
  if (command != PAGPanelCMD) {
    return A_Err_PROJECT_LOAD_FATAL;
  }
  *handled = TRUE;
  SetupQT();

  const auto dialog = new PAGPanelExporterDialog();
  dialog->showMainPage();
  return err;
}
