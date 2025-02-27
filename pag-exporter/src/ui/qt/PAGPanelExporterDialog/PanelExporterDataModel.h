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
#ifndef PANELEXPORTERDATAMODEL_H
#define PANELEXPORTERDATAMODEL_H

#include <QtCore/QAbstractTableModel>
#include <QtWidgets/QItemDelegate>
#include "AEResource.h"

class QmlCompositionData : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString layerName MEMBER layerName)
  Q_PROPERTY(QString savePath MEMBER savePath)
  Q_PROPERTY(bool isFolder MEMBER isFolder)
  Q_PROPERTY(bool isUnFold MEMBER isUnFold)
  Q_PROPERTY(int layerLevel MEMBER layerLevel)
  Q_PROPERTY(bool itemChecked MEMBER itemChecked)
  Q_PROPERTY(QString itemBackColor MEMBER itemBackColor)
  Q_PROPERTY(int itemHeight MEMBER itemHeight)
 public:
  Q_INVOKABLE explicit QmlCompositionData(QObject* parent = nullptr){};

  QString layerName;
  QString savePath;
  bool isFolder;
  bool isUnFold;
  int layerLevel;
  bool itemChecked;
  QString itemBackColor;
  int itemHeight;
};

class ConfigLayerData {
 public:
  ConfigLayerData(std::shared_ptr<AEResource> aeResource)
      : aeResource(aeResource), id(aeResource->id), name(aeResource->name) {
    layerData = std::make_shared<QList<std::shared_ptr<ConfigLayerData>>>();
  }

  virtual ~ConfigLayerData() {
    parent = nullptr;
    layerData = nullptr;
  }

  inline bool operator==(const ConfigLayerData& data) const {
    return data.id == this->id;
  }

  bool hasParent() {
    return parent != nullptr;
  }

  bool hasChild() {
    return !layerData->isEmpty();
  }

  void addChild(std::shared_ptr<ConfigLayerData> child) {
    if (child == nullptr) {
      return;
    }
    if (!layerData->contains(child)) {
      layerData->append(child);
    }
  }

  bool isChecked() {
    return aeResource ? aeResource->getExportFlag() : false;
  }

  void setChecked(bool checked) {
    if (!aeResource) {
      return;
    }
    aeResource->setExportFlag(checked);
  }

  int id;
  std::string name;
  std::shared_ptr<AEResource> aeResource = nullptr;
  std::shared_ptr<ConfigLayerData> parent = nullptr;
  std::shared_ptr<QList<std::shared_ptr<ConfigLayerData>>> layerData;
};

class FolderData : public ConfigLayerData {
 public:
  FolderData(std::shared_ptr<AEResource> aeResource) : ConfigLayerData(aeResource) {
  }

  ~FolderData() {
  }

  bool isUnfold = true;  // 文件夹是否展开
};

/**
 * 导出主面板，图层相关数据 model 层
 */
class ExportConfigLayerModel : public QAbstractTableModel {
  Q_OBJECT
 private:
  std::shared_ptr<QList<std::shared_ptr<ConfigLayerData>>> layerDataList = nullptr;
  std::shared_ptr<QList<std::shared_ptr<ConfigLayerData>>> originLayerData = nullptr;
  bool isFolderData(const QModelIndex& index) const;

 public:
  ExportConfigLayerModel(QObject* parent = nullptr);
  ~ExportConfigLayerModel();

  Q_INVOKABLE int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  Q_INVOKABLE int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  Q_INVOKABLE QVariant data(const QModelIndex& index, int role) const override;
  Q_INVOKABLE bool setData(const QModelIndex& index, const QVariant& value,
                           int role = Qt::EditRole);
  Q_INVOKABLE bool setData(const int row, const QVariant& value, const QString& roleAlias);
  Q_INVOKABLE QmlCompositionData* get(int row);
  Q_INVOKABLE void handleSettingIconClick(int row);
  Q_INVOKABLE void handlePreviewIconClick(int row);
  Q_INVOKABLE void handleSavePathClick(int row);
  Q_INVOKABLE bool isLayerSelected();
  Q_INVOKABLE bool isAllSelected();
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  QHash<int, QByteArray> roleNames() const override;

  void setLayerData(std::shared_ptr<QList<std::shared_ptr<ConfigLayerData>>> dataList,
                    bool forceRefresh = false);
  void searchCompositionsByName(QString name);
  void resetAfterNoSearch();
  void setAllChecked(bool checked);

  /**
   * 获取已经选中图层实体数据
   */
  std::shared_ptr<QList<std::shared_ptr<ConfigLayerData>>> getSelectedLayer();

  Q_SIGNAL void foldStatusChange();
  Q_SIGNAL void settingIconClicked(std::shared_ptr<AEResource> aeResource);
  Q_SIGNAL void previewIconClicked(std::shared_ptr<AEResource> aeResource);
  Q_SIGNAL void savePathClicked(const QModelIndex& index);
  Q_SIGNAL void compositionCheckChanged();

 public Q_SLOTS:
  void onTitleCheckBoxStateChange(int state);

 private:
  mutable QHash<int, QByteArray> roles;

  int getRoleFromName(const QString& roleAlias);

  /**
   * 展开文件夹
   */
  void unfoldFolder(std::shared_ptr<FolderData> folderData);
  /**
   * 收折文件夹
   */
  void foldFolder(std::shared_ptr<FolderData> folderData);
  /**
   * 清空全部合成选择, 二期支持多选后，需要去掉
   */
  void clearAllChecked();
};

#endif  //PANELEXPORTERDATAMODEL_H
