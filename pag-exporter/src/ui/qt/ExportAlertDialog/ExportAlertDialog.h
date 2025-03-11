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
#ifndef EXPORTALERTDIALOG_H
#define EXPORTALERTDIALOG_H
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QDialog>
#include "ui_ExportAlert.h"

class QTExportAlertDialog : public QDialog {
  Q_OBJECT
 public:
  explicit QTExportAlertDialog(const std::vector<std::string>& infos, QWidget* parent = nullptr);

  ~QTExportAlertDialog() override;

  void alertShow();

  void setAlertTitle(const std::string& title = "");

  bool isContinueExport();

 private:
  Ui::ExportAlert* ui = nullptr;

  QStandardItemModel* itemModel;

  bool bIsContinueExport = false;

 private Q_SLOTS:
  void cancelPushButtonClicked();

  void confirmPushButtonClicked();

  static void jumpUrlButtonClicked();
};

#endif  //EXPORTALERTDIALOG_H
