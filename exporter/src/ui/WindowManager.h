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

#include <string>
#include <vector>

namespace exporter {
class WindowManager {
 public:
  static WindowManager& GetInstance();

  void initializeQtEnvironment();

  void showPanelExporterWindow();

  void showPAGConfigWindow();

  void showExportPreviewWindow();

  bool showWarnings(std::vector<std::string>& infos);

  bool showErrors(std::vector<std::string>& infos);

  WindowManager(const WindowManager&) = delete;
  WindowManager& operator=(const WindowManager&) = delete;

  WindowManager(WindowManager&&) = delete;
  WindowManager& operator=(WindowManager&&) = delete;

 private:
  WindowManager();
  ~WindowManager() = default;
};
}  // namespace exporter
