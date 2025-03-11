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
#include "src/configparam/ConfigParam.h"
#include "src/exports/PAGExporter/PAGExporter.h"
#include "src/ui/qt/EnvConfig.h"
#include "src/ui/qt/ErrorList/AlertInfoUI.h"
#include "src/ui/qt/PAGConfigDialog/PAGConfigDialog.h"
#include "src/utils/AEResource.h"
#include "src/utils/AEUtils.h"
#include "src/utils/PrintStream.h"


AEGP_Command AECommand::PAGExporterCMD = 0L;
AEGP_Command AECommand::PAGConfigCMD = 0L;
AEGP_Command AECommand::PAGPanelCMD = 0L;
AEGP_Command AECommand::PAGPreviewCMD = 0L;
WindowManager& AECommand::windowManager = WindowManager::getInstance();


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

  if (windowManager.PAGConfigDialogIsActive()) {
    ERR2(suites.CommandSuite1()->AEGP_DisableCommand(PAGConfigCMD));
  } else {
    ERR(suites.CommandSuite1()->AEGP_EnableCommand(PAGConfigCMD));
  }

  if (AEResource::HasCompositionResource()) {
    if (windowManager.PanelExporterDialogIsActive()) {
      ERR2(suites.CommandSuite1()->AEGP_DisableCommand(PAGPanelCMD));
    } else {
      ERR(suites.CommandSuite1()->AEGP_EnableCommand(PAGPanelCMD));
    }
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
  AEUtils::RunScriptPreWarm();
  windowManager.showPAGConfigDialog();
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
  AEUtils::RunScriptPreWarm();
  windowManager.showPanelExporterDialog();
  return err;
}

A_Err AECommand::OnClickExporter(AEGP_GlobalRefcon globalRefcon, AEGP_CommandRefcon commandRefcon,
                                 AEGP_Command command, AEGP_HookPriority hookPriority,
                                 A_Boolean alreadyHandled, A_Boolean* handled) {
  A_Err err = A_Err_NONE;
  if (command != PAGExporterCMD) {
    return A_Err_PROJECT_LOAD_FATAL;
  }

  SetupQT();
  *handled = TRUE;
  auto& suites = SUITES();
  AEUtils::RunScriptPreWarm();

  auto filePath = AEUtils::BrowseForSave();
  if (filePath.empty()) {
    return err;
  }

  AEGP_ItemH activeItemH = AEUtils::GetActiveCompositionItem();
  if (activeItemH) {
    bool success = PAGExporter::ExportFile(activeItemH, filePath, true, true,
                                           QObject::tr("PAG导出进度"), QObject::tr("正在导出PAG"));
    if (!success) {
      return err;
    }
  } else {
    ErrorAlert(QObject::tr("没有选择需要导出的合成").toStdString());
    return err;
  }

  AEGP_LayerH layerHandle = nullptr;
  ERR(suites.LayerSuite6()->AEGP_GetActiveLayer(&layerHandle));
  if (layerHandle) {
    AEGP_StreamRefH layerStream;
    suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefForLayer(PLUGIN_ID(), layerHandle,
                                                               &layerStream);
    PrintStream(layerStream);
    suites.StreamSuite4()->AEGP_DisposeStream(layerStream);
  }

  PreviewPagFile(filePath);  // 预览
  return err;
}

A_Err AECommand::OnClickPreview(AEGP_GlobalRefcon globalRefcon,
                          AEGP_CommandRefcon commandRefcon,
                          AEGP_Command command,
                          AEGP_HookPriority hookPriority,
                          A_Boolean alreadyHandled,
                          A_Boolean* handled) {

  A_Err err = A_Err_NONE;
  if (command != PAGPreviewCMD) {
    return err;
  }
  *handled = TRUE;
  AEUtils::RunScriptPreWarm();

  AEGP_ItemH activeItemH = AEUtils::GetActiveCompositionItem();
  SetupQT();
  windowManager.showExportPreviewDialog(activeItemH, true, false, false);
  return err;
}