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
#ifndef EXPORTSETTINGDIALOG_H
#define EXPORTSETTINGDIALOG_H
#include <QtQml/QQmlApplicationEngine>
#include <QtQuick/QQuickWindow>
#include <QtWidgets/QTabWidget>
#include "src/ui/qt/ExportSettingDialog/composition/CompositionModel.h"
#include "src/ui/qt/ExportSettingDialog/composition/SnapshotCenter.h"
#include "src/ui/qt/ExportSettingDialog/layerEditable/LayerEditableModel.h"
#include "src/ui/qt/ExportSettingDialog/placeholder/PlaceHolderImageProvider.h"
#include "src/ui/qt/ExportSettingDialog/placeholder/PlaceholderModel.h"
#include "src/ui/qt/ExportSettingDialog/timestreach/TimeStretchModel.h"
#include "src/ui/qt/common/BaseDialog.h"
#include "src/utils/PANels/AECompositionPanel.h"
#include "src/utils/Panels/PlaceImagePanel.h"
#include "src/utils/Panels/TextLayerEditablePanel.h"

class ExportSettingDialog : public QObject {
  Q_OBJECT
 public:
  explicit ExportSettingDialog(AEGP_ItemH& currentAEItem, QObject* parent = nullptr);
  ~ExportSettingDialog() override;
  void show();
  void showPage();
  void setAEItem(AEGP_ItemH& currentAEItem);

  Q_INVOKABLE void exit();
  Q_INVOKABLE int getPlaceHolderSize() const;
  Q_SIGNAL void bmpStatusChange();
  Q_SLOT void onBmpDataChange();
  Q_SIGNAL void showExportConfigDialogSignal();

 private:
  void initTabWidget();
  AEGP_ItemH& currentAEItem;
  std::shared_ptr<AECompositionPanel> compositionPanel;
  std::shared_ptr<PlaceImagePanel> placeImagePanel;
  std::shared_ptr<PlaceTextPanel> placeTextPanel;
  CompositionModel* compositionModel = nullptr;
  PlaceholderModel* placeholderModel = nullptr;
  LayerEditableModel* layerEditableModel = nullptr;
  TimeStretchModel* timeStretchModel = nullptr;
  QQmlApplicationEngine* engine = nullptr;
  QQuickWindow* window = nullptr;
  SnapshotCenter* snapshotCenter = nullptr;
  PlaceHolderImageProvider* placeHolderImageProvider = nullptr;
  void resetData(AEGP_ItemH const& currentAEItem);
};
#endif  //EXPORTSETTINGDIALOG_H
