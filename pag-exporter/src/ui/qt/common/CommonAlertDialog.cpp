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
#include "CommonAlertDialog.h"
#include "ui_CommonAlertDialog.h"

CommonAlertDialog::CommonAlertDialog(QWidget* parent) : QDialog(parent), ui(new Ui::CommonAlertDialog) {
  Qt::WindowFlags flags = Qt::Dialog;
  flags |= Qt::FramelessWindowHint;
  flags |= Qt::WindowStaysOnTopHint;
  setWindowFlags(flags);

  ui->setupUi(this);

  connect(ui->cancelButton, SIGNAL(clicked(bool)), this, SLOT(onCancelBtnClick(bool)));
  connect(ui->sureButton, SIGNAL(clicked(bool)), this, SLOT(onSureBtnClick(bool)));
}

CommonAlertDialog::~CommonAlertDialog() {
  delete ui;
}

void CommonAlertDialog::onCancelBtnClick(bool checked) {
  reject();
}

void CommonAlertDialog::onSureBtnClick(bool checked) {
  accept();
}
