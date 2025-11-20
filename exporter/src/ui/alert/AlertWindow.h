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

#include "ui/AlertInfoModel.h"
#include "ui/BaseWindow.h"

namespace exporter {

class AlertWindow : public BaseWindow {
  Q_OBJECT
 public:
  explicit AlertWindow(QApplication* app, QObject* parent = nullptr);

  bool showWarnings(const std::vector<AlertInfo>& infos);
  bool showErrors(const std::vector<AlertInfo>& infos, const QString& errorMessage = "");
  void onWindowClosing() override;

  Q_INVOKABLE void continueExport();
  Q_INVOKABLE void cancelAndModify();

 private:
  void init(const std::vector<AlertInfo>& infos);
  void wait() const;

  bool continue_ = false;
  bool cancel = false;
  std::unique_ptr<AlertInfoModel> alertInfoModel = nullptr;
};

}  // namespace exporter
