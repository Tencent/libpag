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
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include "export/PAGExport.h"
#include "ui/BaseWindow.h"
#include "utils/AEHelper.h"

namespace exporter {

class ExportWindow : public BaseWindow {
  Q_OBJECT
 public:
  explicit ExportWindow(QApplication* app, const std::string& outputPath = "",
                        QObject* parent = nullptr);

  void show() override;
  void onWindowClosing() override;

 private:
  std::string getOutputPath();
  void init();

  bool showAlertInfo = false;
  AEGP_ItemH itemHandle = nullptr;
  std::string outputPath = "";
  std::unique_ptr<PAGExport> pagExport = nullptr;
};

}  // namespace exporter
