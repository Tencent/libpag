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

#include "PAGConfigDialog.h"
#include <QDebug>
PAGConfigDialog::PAGConfigDialog(QWidget* parent) : QDialog(parent) {
  init();
  refreshData();
}
PAGConfigDialog::~PAGConfigDialog() {
}

void PAGConfigDialog::init() {
  this->setMinimumSize(QSize(720, 538));
  this->setMaximumSize(QSize(720, 538));

  setWindowTitle(tr("PAG Config"));
  createCommonTab();
  createBmpTab();
  // Tab widget
  QTabWidget* tabWidget = new QTabWidget(this);
  tabWidget->addTab(commonTab, tr("通用"));
  tabWidget->addTab(bmpTab, tr("BMP 预合成"));
  // Button layout
  cancelButton = new QPushButton(tr("取消"), this);
  confirmButton = new QPushButton(tr("确定"), this);

  connect(cancelButton, &QPushButton::clicked, this, &PAGConfigDialog::onConcelClicked);
  connect(confirmButton, &QPushButton::clicked, this, &PAGConfigDialog::onConfirmClicked);
  connect(commResetButton, &QPushButton::clicked, this, &PAGConfigDialog::onResetClicked);
  // connect(bitmapResetButton, &QPushButton::clicked, this, &PAGConfigDialog::onResetClicked);

  QHBoxLayout* buttonLayout = new QHBoxLayout();
  buttonLayout->addStretch();
  buttonLayout->addWidget(cancelButton);
  buttonLayout->addWidget(confirmButton);

  // Main layout
  QVBoxLayout* mainLayout = new QVBoxLayout(this);
  mainLayout->addWidget(tabWidget);
  mainLayout->addLayout(buttonLayout);
  setLayout(mainLayout);
}

void PAGConfigDialog::createCommonTab() {

  commonTab = new QWidget(this);
  commonLayout = new QFormLayout();

  QString QComboBoxStyle =
      "QComboBox {"
      "   background-color: #2f2f2f;"
      "   color: #d0d0d0;"
      "   border: 1px solid #2f2f2f;"  // 设置边框颜色为黑色
      "}";
  // "QComboBox::drop-down {"
  // "   border-right: 1px solid #2f2f2f;"
  // "}";

  languageComboBox = new QComboBox(commonTab);
  languageComboBox->addItem(tr("中文 (简体)"));
  languageComboBox->addItem(tr("English (US)"));
  languageComboBox->setFixedSize(192, 30);
  languageComboBox->setStyleSheet(QComboBoxStyle);

  exportSceneComboBox = new QComboBox(commonTab);
  exportSceneComboBox->addItem(tr("通用"));
  exportSceneComboBox->addItem(tr("UI动画"));
  exportSceneComboBox->addItem(tr("视频编辑"));
  // exportSceneComboBox->setFixedSize(192, 30);
  exportSceneComboBox->setFixedWidth(192);
  exportSceneComboBox->setFixedHeight(30);
  exportSceneComboBox->setStyleSheet(QComboBoxStyle);

  exportVersionComboBox = new QComboBox(commonTab);
  exportVersionComboBox->addItem("Stable");
  exportVersionComboBox->addItem("Beta");
  exportVersionComboBox->addItem("Custom");
  exportVersionComboBox->setFixedSize(192, 30);
  exportVersionComboBox->setStyleSheet(QComboBoxStyle);

  tagLevelLineEdit = new QLineEdit(commonTab);
  tagLevelLineEdit->setText("1023");
  tagLevelLineEdit->setEnabled(false);
  tagLevelLineEdit->setFixedSize(192, 30);
  tagLevelLineEdit->setStyleSheet(
      "QLineEdit {"
      "   background-color: #2f2f2f;"
      "}");

  bitmapCompressionSpinBox = new QSpinBox(commonTab);
  bitmapCompressionSpinBox->setRange(0, 100);
  bitmapCompressionSpinBox->setValue(80);
  bitmapCompressionSpinBox->setFixedSize(192, 30);
  bitmapCompressionSpinBox->setStyleSheet(
      "QSpinBox {"
      "   background-color: #2f2f2f;"
      "   color: #d0d0d0;"
      "}");

  bitmapPixelDensitySpinBox = new QDoubleSpinBox(commonTab);
  bitmapPixelDensitySpinBox->setRange(1.0, 3.0);
  bitmapPixelDensitySpinBox->setValue(2.0);
  bitmapPixelDensitySpinBox->setSingleStep(0.1);  // 设置步长为 0.1
  bitmapPixelDensitySpinBox->setFixedSize(192, 30);
  bitmapPixelDensitySpinBox->setStyleSheet(
      "QSpinBox {"
      "   background-color: #2f2f2f;"
      "   color: #d0d0d0;"
      "}");

  exportLayerNameComboBox = new QComboBox(commonTab);
  exportLayerNameComboBox->addItem(tr("是"));
  exportLayerNameComboBox->addItem(tr("否"));
  exportLayerNameComboBox->setFixedSize(192, 30);
  exportLayerNameComboBox->setStyleSheet(QComboBoxStyle);

  exportFontComboBox = new QComboBox(commonTab);
  exportFontComboBox->addItem(tr("是"));
  exportFontComboBox->addItem(tr("否"));
  exportFontComboBox->setCurrentIndex(1);
  exportFontComboBox->setFixedSize(192, 30);
  exportFontComboBox->setStyleSheet(QComboBoxStyle);

  commonLayout->addRow(tr("语言:"), languageComboBox);
  commonLayout->addRow(tr("导出场景:"), exportSceneComboBox);
  commonLayout->addRow(tr("导出版本控制:"), exportVersionComboBox);
  commonLayout->addRow(tr("TAG Level:"), tagLevelLineEdit);
  commonLayout->addRow(tr("位图压缩质量:"), bitmapCompressionSpinBox);
  commonLayout->addRow(tr("位图像素密度:"), bitmapPixelDensitySpinBox);
  commonLayout->addRow(tr("导出图层名字:"), exportLayerNameComboBox);
  commonLayout->addRow(tr("导出字体:"), exportFontComboBox);

  QVBoxLayout* vlayout = new QVBoxLayout();
  QHBoxLayout* hlayout = new QHBoxLayout();
  commResetButton = new QPushButton(tr("重置默认值"), commonTab);
  commResetButton->setStyleSheet(
      "QPushButton {"
      "   width: 85px;"                // 设置按钮宽度
      "   height: 25px;"               // 设置按钮高度
      "   background-color: #202020;"  // 设置背景颜色，例如绿色
      "   border: 2px solid #b9b9b9;"  // 设置边框颜色和宽度，例如蓝色
      "   border-radius: 14px;"        // 设置边框圆角半径
      "   font-size: 14px;"            // 设置字体大小
      "   color: #b9b9b9;"             // 设置字体颜色，例如白色
      // "  text-align: right;"           // 设置文本右对齐
      "  position: absolute;"  // 设置绝对定位
      "  right: 14%;"          // 设置距离父布局右侧 14% 的宽度
      "  bottom: 28px;"        // 设置距离父布局底部 28px
      "}"
      "QPushButton:pressed {"
      "   background-color: #b9b9b9;"  // 按下时的背景颜色，例如深绿色
      "   color: #252525;"             // 设置字体颜色，例如白色
      "}");
  hlayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
  hlayout->addWidget(commResetButton);
  hlayout->setSpacing(14);

  vlayout->addLayout(commonLayout);
  vlayout->addLayout(hlayout);
  commonTab->setLayout(vlayout);
}

void PAGConfigDialog::createBmpTab() {
  bmpTab = new QWidget(this);
  bmpLayout = new QFormLayout();
  QString QComboBoxStyle =
      "QComboBox {"
      "   background-color: #2f2f2f;"
      "   color: #d0d0d0;"
      "   border: 1px solid #2f2f2f;"  // 设置边框颜色为黑色
      "}";
  // "QComboBox::drop-down {"
  // "   border-right: 1px solid #2f2f2f;"
  // "}";

  QString QSpinBoxStyle =
      "QSpinBox {"
      "   background-color: #2f2f2f;"
      "   color: #d0d0d0;"
      "}";

  storageFormatComboBox = new QComboBox(bmpTab);
  storageFormatComboBox->addItem("H.264");
  storageFormatComboBox->addItem("Webp");
  storageFormatComboBox->setFixedSize(192, 30);
  storageFormatComboBox->setStyleSheet(QComboBoxStyle);

  imageQualitySpinBox = new QSpinBox(bmpTab);
  imageQualitySpinBox->setRange(0, 100);
  imageQualitySpinBox->setValue(80);
  imageQualitySpinBox->setFixedSize(192, 30);
  imageQualitySpinBox->setStyleSheet(QSpinBoxStyle);

  exportSizeLimitpinBox = new QSpinBox(bmpTab);
  exportSizeLimitpinBox->setRange(0, 10000);
  exportSizeLimitpinBox->setValue(720);
  exportSizeLimitpinBox->setFixedSize(192, 30);
  exportSizeLimitpinBox->setStyleSheet(QSpinBoxStyle);

  maxFrameRateSpinBox = new QSpinBox(bmpTab);
  maxFrameRateSpinBox->setRange(1, 120);
  maxFrameRateSpinBox->setValue(24);
  maxFrameRateSpinBox->setFixedSize(192, 30);
  maxFrameRateSpinBox->setStyleSheet(QSpinBoxStyle);

  keyFrameIntervalSpinBox = new QSpinBox(bmpTab);
  keyFrameIntervalSpinBox->setValue(60);
  keyFrameIntervalSpinBox->setRange(0, 10000);
  keyFrameIntervalSpinBox->setFixedSize(192, 30);
  keyFrameIntervalSpinBox->setStyleSheet(QSpinBoxStyle);

  bmpLayout->addRow(tr("帧存储格式:"), storageFormatComboBox);
  bmpLayout->addRow(tr("图像质量:"), imageQualitySpinBox);
  bmpLayout->addRow(tr("导出尺寸上限 (短边长):"), exportSizeLimitpinBox);
  bmpLayout->addRow(tr("最大帧率上限:"), maxFrameRateSpinBox);
  bmpLayout->addRow(tr("关键帧间隔:"), keyFrameIntervalSpinBox);

  QVBoxLayout* vlayout = new QVBoxLayout();
  QHBoxLayout* hlayout = new QHBoxLayout();
  bitmapResetButton = new QPushButton(tr("重置默认值"), commonTab);
  bitmapResetButton->setStyleSheet(
      "QPushButton {"
      "   width: 85px;"
      "   height: 25px;"
      "   background-color: #202020;"
      "   border: 2px solid #b9b9b9;"
      "   border-radius: 14px;"
      "   font-size: 14px;"
      "   color: #b9b9b9;"
      // "  text-align: right;"
      "  position: absolute;"
      "  right: 14%;"
      "  bottom: 28px;"
      "}"
      "QPushButton:pressed {"
      "   background-color: #b9b9b9;"
      "   color: #252525;"
      "}");
  hlayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
  hlayout->addWidget(bitmapResetButton);
  hlayout->setSpacing(14);

  vlayout->addLayout(bmpLayout);
  vlayout->addLayout(hlayout);
  bmpTab->setLayout(vlayout);
}

void PAGConfigDialog::onConcelClicked() {
  accept();
}

void PAGConfigDialog::onConfirmClicked() {
  ConfigParam param;
  // Common tab
  param.commonParam.language = static_cast<LanguageType>(languageComboBox->currentIndex());
  param.commonParam.exportScene = static_cast<ExportScenes>(exportSceneComboBox->currentIndex());
  param.commonParam.exportVersion =
      static_cast<ExportVersionType>(exportVersionComboBox->currentIndex());
  param.commonParam.tagLevel = tagLevelLineEdit->text().toInt();
  param.commonParam.imageQuality = bitmapCompressionSpinBox->value();
  param.commonParam.imagePixelRatio = bitmapPixelDensitySpinBox->value();
  param.commonParam.enableLayerName = !exportLayerNameComboBox->currentIndex();
  param.commonParam.enableFontFile = !exportFontComboBox->currentIndex();
  // Bmp tab
  param.bmpComposParam.frameFormat =
      static_cast<FrameFormatType>(storageFormatComboBox->currentIndex());
  param.bmpComposParam.sequenceQuality = imageQualitySpinBox->value();
  param.bmpComposParam.bitmapMaxResolution = exportSizeLimitpinBox->value();
  param.bmpComposParam.maxFrameRate = maxFrameRateSpinBox->value();
  param.bmpComposParam.bitmapKeyFrameInterval = keyFrameIntervalSpinBox->value();

  paramManager.setConfigParam(param);
}

void PAGConfigDialog::onResetClicked() {
  ConfigParam param;
  resetData(param);
}

void PAGConfigDialog::resetData(ConfigParam& configParam) {

  // Common tab
  languageComboBox->setCurrentIndex(static_cast<int>(configParam.commonParam.language));
  exportSceneComboBox->setCurrentIndex(static_cast<int>(configParam.commonParam.exportScene));
  exportVersionComboBox->setCurrentIndex(static_cast<int>(configParam.commonParam.exportVersion));
  tagLevelLineEdit->setText(QString::number(configParam.commonParam.tagLevel));
  bitmapCompressionSpinBox->setValue(configParam.commonParam.imageQuality);
  bitmapPixelDensitySpinBox->setValue(configParam.commonParam.imagePixelRatio);
  exportLayerNameComboBox->setCurrentIndex(!configParam.commonParam.enableLayerName);
  exportFontComboBox->setCurrentIndex(!configParam.commonParam.enableFontFile);

  // Bmp tab
  storageFormatComboBox->setCurrentIndex(static_cast<int>(configParam.bmpComposParam.frameFormat));
  imageQualitySpinBox->setValue(configParam.bmpComposParam.sequenceQuality);
  exportSizeLimitpinBox->setValue(configParam.bmpComposParam.bitmapMaxResolution);
  maxFrameRateSpinBox->setValue(configParam.bmpComposParam.maxFrameRate);
  keyFrameIntervalSpinBox->setValue(configParam.bmpComposParam.bitmapKeyFrameInterval);
}

void PAGConfigDialog::refreshData() {
  if (paramManager.configFileIsExist()) {
    paramManager.getConfigParam(configParam);
    resetData(configParam);
  }
}
