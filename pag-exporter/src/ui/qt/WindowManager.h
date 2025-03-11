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
#ifndef WINDOWMANAGER_H
#define WINDOWMANAGER_H
#include "AE_GeneralPlug.h"

class PAGPanelExporterDialog;
class PAGConfigDialog;

class WindowManager {
 public:
  static WindowManager& getInstance();

  WindowManager(const WindowManager&) = delete;
  WindowManager& operator=(const WindowManager&) = delete;

  void showPanelExporterDialog();
  void showPAGConfigDialog();
  void showExportPreviewDialog(AEGP_ItemH itemH, bool isExportAudio, bool needShowExport,
                               bool isQWidget = false);
  void exitPAGPanelExporterDialog();

  bool PanelExporterDialogIsActive();

  bool PAGConfigDialogIsActive();

 private:
  WindowManager() = default;
  ~WindowManager();

  PAGPanelExporterDialog* panelExporterDialog = nullptr;
  PAGConfigDialog* configDialog = nullptr;
  bool isConfigDialogDestroy = false;
};
#endif  //WINDOWMANAGER_H
