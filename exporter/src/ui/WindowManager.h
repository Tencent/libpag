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

#pragma once

#include <QApplication>
#include <QString>
#include <QTranslator>
#include <string>
#include <vector>
#include "config/ConfigWindow.h"
#include "export/ExportWindow.h"
#include "exportPanel/ExportPanelWindow.h"
#include "utils/AlertInfo.h"

namespace exporter {

class WindowManager {
 public:
  static WindowManager& GetInstance();

  void initializeQtEnvironment();

  void showExportPanelWindow();

  void showPAGConfigWindow();

  void showExportPreviewWindow();

  void showExportWindow();

  bool showWarnings(const std::vector<AlertInfo>& infos);

  bool showErrors(const std::vector<AlertInfo>& infos);

  bool showSimpleError(const QString& message);

  bool showPAGViewerInstallDialog(const std::string& pagFilePath);

  WindowManager(const WindowManager&) = delete;
  WindowManager& operator=(const WindowManager&) = delete;

  WindowManager(WindowManager&&) = delete;
  WindowManager& operator=(WindowManager&&) = delete;

 private:
  void init();
  void runEventLoopIfNeeded();

  WindowManager();
  ~WindowManager() = default;

  int argc = 0;
  char** argv = nullptr;
  std::unique_ptr<QApplication> app = nullptr;
  std::unique_ptr<QTranslator> translator = nullptr;
  std::unique_ptr<ConfigWindow> configWindow = nullptr;
  std::unique_ptr<ExportWindow> exportWindow = nullptr;
  std::unique_ptr<ExportWindow> previewWindow = nullptr;
  std::unique_ptr<ExportPanelWindow> exportPanelWindow = nullptr;
};

}  // namespace exporter
