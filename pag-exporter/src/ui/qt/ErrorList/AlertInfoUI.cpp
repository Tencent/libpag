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
#include "src/utils/AlertInfo.h"
#include "AlertInfoUI.h"
#include <QtWidgets/QMessageBox>
#include <fstream>
#include <iostream>
#include "ExportSingleAlert.h"
#include "String_Utils.h"
#include "src/ui/qt/ExportAlertDialog/ExportAlertDialog.h"

bool WarningsAlert(std::vector<std::string>& infos) {
  if (infos.empty()) {
    return false;
  }

  std::shared_ptr<QTExportAlertDialog> alert =
      std::shared_ptr<QTExportAlertDialog>(new QTExportAlertDialog(infos, nullptr));
  auto title = QObject::tr("优化建议");
  alert->setAlertTitle(title.toStdString());
  alert->alertShow();

  return !alert->isContinueExport();
}

bool ErrorAlert(std::string info) {
  if (info.empty()) {
    return false;
  }

  std::shared_ptr<ExportSingleAlert> alert =
      std::shared_ptr<ExportSingleAlert>(new ExportSingleAlert(info));
  alert->alertShow();

  return true;
}

bool ErrorsAlert(std::vector<std::string>& infos) {
  if (infos.empty()) {
    return false;
  }

  std::shared_ptr<QTExportAlertDialog> alert =
      std::shared_ptr<QTExportAlertDialog>(new QTExportAlertDialog(infos));
  auto title = QObject::tr("错误");
  alert->setAlertTitle(title.toStdString());
  alert->alertShow();

  infos.clear();
  return !alert->isContinueExport();
}