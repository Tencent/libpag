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

#include "AECommand.h"
#include "export/PAGExport.h"
#include "platform/PlatformHelper.h"
#include "ui/AlertInfoModel.h"
#include "ui/WindowManager.h"
#include "utils/AEHelper.h"
#include "utils/AEResource.h"
#include "utils/AlertInfo.h"

namespace exporter {

AEGP_Command AECommand::PAGExporterCMD = 0L;
AEGP_Command AECommand::PAGConfigCMD = 0L;
AEGP_Command AECommand::PAGPanelCMD = 0L;
AEGP_Command AECommand::PAGPreviewCMD = 0L;

A_Err AECommand::OnUpdateMenu(AEGP_GlobalRefcon /*globalRefcon*/,
                              AEGP_UpdateMenuRefcon /*menuRefcon*/,
                              AEGP_WindowType /*windowType*/) {
  A_Err err = A_Err_NONE;
  A_Err err2 = A_Err_NONE;
  const auto& suites = GetSuites();
  AEGP_ItemH activeItemHandle = GetActiveCompositionItem();
  if (activeItemHandle) {
    ERR(suites->CommandSuite1()->AEGP_EnableCommand(PAGExporterCMD));
    ERR(suites->CommandSuite1()->AEGP_EnableCommand(PAGPreviewCMD));
  } else {
    ERR2(suites->CommandSuite1()->AEGP_DisableCommand(PAGExporterCMD));
    ERR2(suites->CommandSuite1()->AEGP_DisableCommand(PAGPreviewCMD));
  }

  ERR(suites->CommandSuite1()->AEGP_EnableCommand(PAGConfigCMD));
  if (HasCompositionResource()) {
    ERR(suites->CommandSuite1()->AEGP_EnableCommand(PAGPanelCMD));
  } else {
    ERR2(suites->CommandSuite1()->AEGP_DisableCommand(PAGPanelCMD));
  }
  return err;
}

A_Err AECommand::OnClickConfig(AEGP_GlobalRefcon /*globalRefcon*/,
                               AEGP_CommandRefcon /*commandRefcon*/, AEGP_Command command,
                               AEGP_HookPriority /*hookPriority*/, A_Boolean /*alreadyHandled*/,
                               A_Boolean* handled) {
  A_Err err = A_Err_NONE;
  if (command != PAGConfigCMD) {
    return err;
  }
  *handled = TRUE;
  WindowManager::GetInstance().showPAGConfigWindow();
  return err;
}

A_Err AECommand::OnClickPanel(AEGP_GlobalRefcon /*globalRefcon*/,
                              AEGP_CommandRefcon /*commandRefcon*/, AEGP_Command command,
                              AEGP_HookPriority /*hookPriority*/, A_Boolean /*alreadyHandled*/,
                              A_Boolean* handled) {

  A_Err err = A_Err_NONE;
  if (command != PAGPanelCMD) {
    return err;
  }
  *handled = TRUE;
  WindowManager::GetInstance().showExportPanelWindow();
  return err;
}

A_Err AECommand::OnClickExporter(AEGP_GlobalRefcon /*globalRefcon*/,
                                 AEGP_CommandRefcon /*commandRefcon*/, AEGP_Command command,
                                 AEGP_HookPriority /*hookPriority*/, A_Boolean /*alreadyHandled*/,
                                 A_Boolean* handled) {
  A_Err err = A_Err_NONE;
  if (command != PAGExporterCMD) {
    return err;
  }
  *handled = TRUE;
  WindowManager::GetInstance().showExportWindow();
  return err;
}

A_Err AECommand::OnClickPreview(AEGP_GlobalRefcon /*globalRefcon*/,
                                AEGP_CommandRefcon /*commandRefcon*/, AEGP_Command command,
                                AEGP_HookPriority /*hookPriority*/, A_Boolean /*alreadyHandled*/,
                                A_Boolean* handled) {

  A_Err err = A_Err_NONE;
  if (command != PAGPreviewCMD) {
    return err;
  }
  *handled = TRUE;
  WindowManager::GetInstance().showExportPreviewWindow();
  return err;
}

}  // namespace exporter
