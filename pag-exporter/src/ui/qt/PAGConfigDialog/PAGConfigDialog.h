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
#ifndef PAGCONFIGDIALOG_H
#define PAGCONFIGDIALOG_H

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTranslator>
#include <QVBoxLayout>
#include <QWidget>
#include "ConfigParam.h"

class PAGConfigDialog : public QDialog {
  Q_OBJECT
 public:
  PAGConfigDialog(QWidget* parent = nullptr);
  ~PAGConfigDialog() override;

 private:
  void init();
  void createCommonTab();
  void createBmpTab();

  void resetData(ConfigParam& configParam);

  void refreshData();
 private Q_SLOTS:
  void onConcelClicked();
  void onConfirmClicked();
  void onResetClicked();

 private:
  QTabWidget* tabWidget;
  QFormLayout* commonLayout;
  QFormLayout* bmpLayout;
  QComboBox* languageComboBox;
  QComboBox* exportSceneComboBox;
  QComboBox* exportVersionComboBox;
  QLineEdit* tagLevelLineEdit;
  QSpinBox* bitmapCompressionSpinBox;
  QDoubleSpinBox* bitmapPixelDensitySpinBox;
  QComboBox* exportLayerNameComboBox;
  QComboBox* exportFontComboBox;
  QComboBox* storageFormatComboBox;
  QSpinBox* imageQualitySpinBox;
  QSpinBox* exportSizeLimitpinBox;

  QSpinBox* maxFrameRateSpinBox;
  QSpinBox* keyFrameIntervalSpinBox;
  QPushButton* cancelButton;
  QPushButton* confirmButton;
  QPushButton* commResetButton;
  QPushButton* bitmapResetButton;

  QWidget* commonTab;
  QWidget* bmpTab;

  ConfigParam configParam;
  ConfigParamManager& paramManager = ConfigParamManager::getInstance();
};

#endif  //PAGCONFIGDIALOG_H
