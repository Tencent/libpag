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
#include "CustomProgressBar.h"
#include <QtCore/QDebug>

CustomProgressBar::CustomProgressBar(QWidget* parent) : QDialog(parent), ui(new Ui::QWidgetProgress) {
  init(nullptr);
}

CustomProgressBar::CustomProgressBar(pagexporter::Context* context, QWidget* parent) : QDialog(parent), ui(new Ui::QWidgetProgress) {
  init(context);
}

void CustomProgressBar::init(pagexporter::Context* context) {
  ui->setupUi(this);
  this->context = context;
  this->setProgressTitle(tr("PAG导出进度"));
  Qt::WindowFlags flags = Qt::Dialog;
  flags |= Qt::CustomizeWindowHint;
  flags |= Qt::WindowTitleHint;
  flags |= Qt::WindowCloseButtonHint;
  flags |= Qt::WindowStaysOnTopHint;
  this->setWindowFlags(flags);
  QApplication::setOverrideCursor(Qt::WaitCursor);
  this->show();
}

void CustomProgressBar::closeEvent(QCloseEvent*) {
  if (context != nullptr) {
    context->bEarlyExit = true;
  }
}

CustomProgressBar::~CustomProgressBar() {
  QApplication::restoreOverrideCursor();
  delete ui;
}

void CustomProgressBar::setProgress(double progressValue) {
  if (progressValue < 0.0) {
    this->progress = 0.0;
  } else if (progressValue > 100.0) {
    this->progress = 100.0;
  } else {
    this->progress = progressValue;
  }
  QApplication::processEvents();
  ui->progressBar->setValue(static_cast<int>(this->progress));
}

void CustomProgressBar::setProgressTips(const QString& tip) {
  ui->tipLabel->setText(tip);
}

void CustomProgressBar::setProgressTitle(const QString& title){
  this->setWindowTitle(title);
}
