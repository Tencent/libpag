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

#include <QAbstractListModel>
#include <QQmlEngine>
#include "ProgressListModel.h"
#include "export/PAGExport.h"
#include "ui/AlertInfoModel.h"
#include "utils/AEResource.h"

namespace exporter {

class ExportCompositionData {
 public:
  bool isFolder = false;
  bool isUnfold = true;
  int level = 0;
  std::shared_ptr<AEResource> resource = nullptr;
};

class CompositionsModel : public QAbstractListModel {
  Q_OBJECT
 public:
  enum class ExportCompositionModelRoles {
    NameRole = Qt::UserRole + 1,
    SavePathRole,
    IsSelectedRole,
    IsFolderRole,
    IsUnfoldRole,
    LevelRole
  };

  explicit CompositionsModel(QObject* parent = nullptr);

  Q_PROPERTY(bool allSelected READ getAllSelected NOTIFY allSelectedChanged)
  Q_PROPERTY(bool canExport READ getCanExport NOTIFY canExportChanged)
  Q_PROPERTY(bool exportAudio READ getExportAudio WRITE setExportAudio NOTIFY exportAudioChanged)

  void setAEResources(const std::vector<std::shared_ptr<AEResource>>& resources);
  void setQmlEngine(QQmlEngine* engine);
  Q_INVOKABLE bool getAllSelected() const;
  Q_INVOKABLE bool getCanExport() const;
  Q_INVOKABLE bool getExportAudio() const;
  Q_INVOKABLE void setIsSelected(int index, bool isSelected, bool isAllSelected = false);
  Q_INVOKABLE void setIsUnfold(int index, bool isUnfold);
  Q_INVOKABLE void setSavePath(int index, const QString& savePath);
  Q_INVOKABLE void setAllSelected(bool allSelected);
  Q_INVOKABLE void setSerachText(const QString& searchText);
  Q_INVOKABLE void setExportAudio(bool exportAudio);
  Q_INVOKABLE void exportSelectedCompositions();
  Q_INVOKABLE void prepareForPreview(int row);
  Q_INVOKABLE void previewComposition(int row);
  Q_INVOKABLE void updateNames();
  Q_INVOKABLE void onWindowClosing();

  int rowCount(const QModelIndex& parent) const override;
  int columnCount(const QModelIndex& parent) const override;
  QVariant data(const QModelIndex& index, int role) const override;

  Q_SIGNAL void allSelectedChanged(bool allSelected);
  Q_SIGNAL void canExportChanged(bool canExport);
  Q_SIGNAL void exportAudioChanged(bool exportAudio);
  Q_SIGNAL void cancelExport();

 protected:
  void updateCompositionLevel();
  void updateAllSelectedNum();
  void updateAlertInfos();
  QHash<int, QByteArray> roleNames() const override;

 private:
  bool exportAudio = false;
  size_t selectedNum = 0;
  size_t allSelectedNum = 0;
  QQmlEngine* engine = nullptr;
  std::unique_ptr<PAGExport> pagExport = nullptr;
  std::unique_ptr<AlertInfoModel> alertInfoModel = nullptr;
  std::vector<std::unique_ptr<PAGExport>> pagExports = {};
  std::unique_ptr<ProgressListModel> progressListModel = nullptr;
  std::vector<std::shared_ptr<AEResource>> resources = {};
  std::vector<std::shared_ptr<ExportCompositionData>> compositions = {};
};

}  // namespace exporter
