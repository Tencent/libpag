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
#include "ExportSettingDialog.h"
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickWindow>
#include "src/ui/qt/TextEdit/TextEdit.h"
#include "src/utils/AEUtils.h"

static QLatin1String COMPOSITION_IMAGE("CompositionImg");
static QLatin1String PLACEHOLDER_IMAGE("PlaceHolderImg");

void ExportSettingDialog::initTabWidget() {
  engine = new QQmlApplicationEngine(this);
  resetData(currentAEItem);
  QQmlContext* ctx = engine->rootContext();
  ctx->setContextProperty("exportSettingDialog", this);
}

ExportSettingDialog::ExportSettingDialog(AEGP_ItemH& currentAEItem, QObject* parent)
    : currentAEItem(currentAEItem), QObject(parent) {
  initTabWidget();
  connect(compositionModel, &CompositionModel::bmpDataChange, this, &ExportSettingDialog::onBmpDataChange);
}

ExportSettingDialog::~ExportSettingDialog() {
  compositionPanel = nullptr;
  placeImagePanel = nullptr;
  snapshotCenter = nullptr;
  placeHolderImageProvider = nullptr;
  placeholderModel = nullptr;
  layerEditableModel = nullptr;
  compositionModel = nullptr;
}

void ExportSettingDialog::showPage() {
  engine->load(QUrl(QStringLiteral("qrc:/qml/ExportSettings.qml")));
  window = static_cast<QQuickWindow*>(engine->rootObjects().at(0));

  QString titleName(tr("设置面板-"));
  titleName.append(AEUtils::GetItemName(currentAEItem).c_str());
  titleName = TextEdit::elideText(titleName, 14, 350);
  window->setTitle(titleName);

  window->setTextRenderType(QQuickWindow::TextRenderType::NativeTextRendering);
#ifdef ENABLE_OLD_API
  window->setPersistentOpenGLContext(true);
#else
  window->setPersistentGraphics(true);
#endif
  window->setPersistentSceneGraph(true);
  window->show();
}

void ExportSettingDialog::setAEItem(const AEGP_ItemH& currentAEItem) {
  this->currentAEItem = currentAEItem;
  resetData(currentAEItem);
}

void ExportSettingDialog::resetData(AEGP_ItemH const& currentAEItem) {
  compositionPanel = std::make_shared<AECompositionPanel>(currentAEItem);
  placeImagePanel = std::make_shared<PlaceImagePanel>(currentAEItem);
  placeTextPanel = std::make_shared<PlaceTextPanel>(currentAEItem);
  compositionModel = new CompositionModel(compositionPanel, this);
  placeholderModel = new PlaceholderModel(this);
  placeholderModel->setPlaceHolderData(placeImagePanel->getList());
  layerEditableModel = new LayerEditableModel(this);
  layerEditableModel->setPlaceHolderData(placeTextPanel->getList());
  timeStretchModel = new TimeStretchModel(currentAEItem);

  QQmlContext* ctx = engine->rootContext();
  ctx->setContextProperty("compositionModel", compositionModel);
  ctx->setContextProperty("placeholderModel", placeholderModel);
  ctx->setContextProperty("layerEditableModel", layerEditableModel);
  ctx->setContextProperty("timeStretchModel", timeStretchModel);
  if (snapshotCenter == nullptr) {
    snapshotCenter = new SnapshotCenter(compositionPanel, this);
    engine->addImageProvider(COMPOSITION_IMAGE, snapshotCenter->compositionImageProvider);
    ctx->setContextProperty("snapshotCenter", snapshotCenter);
  } else {
    snapshotCenter->setCompositionPanel(compositionPanel);
  }
  if (placeHolderImageProvider == nullptr) {
    placeHolderImageProvider = new PlaceHolderImageProvider(placeImagePanel->getList());
    engine->addImageProvider(PLACEHOLDER_IMAGE, placeHolderImageProvider);
  } else {
    placeHolderImageProvider->resetLayers(placeImagePanel->getList());
  }
}

void ExportSettingDialog::show() const {
  if (window) {
    window->show();
  }
}

void ExportSettingDialog::exit() {
  if (window) {
    window->hide();
  }
  engine->removeImageProvider(PLACEHOLDER_IMAGE);
  engine->removeImageProvider(COMPOSITION_IMAGE);
  Q_EMIT showExportConfigDialogSignal();
}

int ExportSettingDialog::getPlaceHolderSize() const {
  if (!placeImagePanel) {
    return 0;
  }
  return placeImagePanel->getList()->size();
}

void ExportSettingDialog::onBmpDataChange() {
  placeImagePanel->refresh();
  placeholderModel->setPlaceHolderData(placeImagePanel->getList(), true);
  //placeholderModel->setCurrentSelect(0);
  layerEditableModel->setPlaceHolderData(placeTextPanel->getList(), true);

  Q_EMIT bmpStatusChange();
}
