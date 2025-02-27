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
#include "ExportAlertDialog.h"
#include <QtCore/QObject>
#include <QtCore/QTranslator>
#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>

QTExportAlertDialog::QTExportAlertDialog(const std::vector<std::string>& infos, QWidget* parent)
    : QDialog(parent), ui(new Ui::ExportAlert) {

  ui->setupUi(this);
  Qt::WindowFlags flags = Qt::Dialog;
  flags |= Qt::WindowCloseButtonHint;
  flags |= Qt::WindowStaysOnTopHint;
  this->setWindowFlags(flags);

  QString tips = QObject::tr("共 %1 条, 请参考优化后重新导出, 详情如下:");
  ui->label->setText(tips.arg(infos.size()));

  itemModel = new QStandardItemModel();
  //定义item
  QStandardItem* item;
  for (int i = 0; i < infos.size(); i++) {
    auto text = infos[i];
    item = new QStandardItem(QString::fromStdString(text));
    itemModel->setItem(i, 0, item);
  }
  ui->tableView->setModel(itemModel);
  ui->tableView->setColumnWidth(0, 1100);
  ui->tableView->horizontalHeader()->hide();
  ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);

  connect(ui->confirmButton, SIGNAL(released()), this, SLOT(confirmPushButtonClicked()));
  connect(ui->cancelButton, SIGNAL(released()), this, SLOT(cancelPushButtonClicked()));
  connect(ui->jumpUrlButton, SIGNAL(released()), this, SLOT(jumpUrlButtonClicked()));
}

QTExportAlertDialog::~QTExportAlertDialog() {
  delete itemModel;
}

void QTExportAlertDialog::setAlertTitle(const std::string& title) {
  this->setWindowTitle(QString::fromStdString(title));
}

void QTExportAlertDialog::alertShow() {
  this->exec();
}

void QTExportAlertDialog::confirmPushButtonClicked() {
  bIsContinueExport = true;
  this->close();
}

void QTExportAlertDialog::cancelPushButtonClicked() {
  bIsContinueExport = false;
  this->close();
}

void QTExportAlertDialog::jumpUrlButtonClicked() {
  QDesktopServices::openUrl(QUrl(QLatin1String("https://pag.art/docs/pag-export-verify.html")));
}

bool QTExportAlertDialog::isContinueExport() {
  return bIsContinueExport;
}