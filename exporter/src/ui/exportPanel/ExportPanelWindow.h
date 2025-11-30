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
#include "CompositionsModel.h"
#include "ui/BaseWindow.h"
#include "utils/AEResource.h"
#include "utils/PAGExportSession.h"

namespace exporter {

class ExportPanelWindow : public BaseWindow {
  Q_OBJECT
 public:
  explicit ExportPanelWindow(QApplication* app, QObject* parent = nullptr);

  void viewLayers(const std::shared_ptr<AEResource>& resource);
  std::shared_ptr<PAGExportSession> getSession(int row);
  void updateSession(int row);

  Q_INVOKABLE QString getBackgroundColor(int row) const;
  Q_INVOKABLE bool isAEWindowActive();

 private:
  void init();

  std::unique_ptr<CompositionsModel> compositionsModel = nullptr;
  std::vector<std::shared_ptr<AEResource>> resources = {};
  std::map<A_long, std::shared_ptr<PAGExportSession>> sessionMap = {};
};

}  // namespace exporter
