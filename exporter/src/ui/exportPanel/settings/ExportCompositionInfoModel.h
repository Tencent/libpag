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
#include "ExportFrameImageProvider.h"
#include "utils/AEResource.h"
#include "utils/PAGExportSession.h"

namespace exporter {

using GetSessionHandler = std::function<std::shared_ptr<PAGExportSession>(A_long ID)>;
using UpdateSessionHandler = std::function<void(A_long ID)>;

class ExportCompositionInfoModel : public QAbstractListModel {
  Q_OBJECT
 public:
  struct Data {
    int level = 0;
    int parentIndex = -1;
    bool isEnable = true;
    std::shared_ptr<AEResource> resource = nullptr;
  };

  enum class ExportCompositionInfoModelRoles {
    NameRole = Qt::UserRole + 1,
    ExportAsBmpRole,
    EnableRole,
    LevelRole
  };

  Q_PROPERTY(int width READ getWidth NOTIFY widthChanged)
  Q_PROPERTY(int height READ getHeight NOTIFY heightChanged)
  Q_PROPERTY(QString currentTime READ getCurrentTime NOTIFY currentTimeChanged)
  Q_PROPERTY(QString duration READ getDuration NOTIFY durationChanged)
  Q_PROPERTY(
      QString currentFrame READ getCurrentFrame WRITE setCurrentFrame NOTIFY currentFrameChanged)
  Q_PROPERTY(QString imageProviderName READ getImageProviderName NOTIFY imageProviderNameChanged)

  explicit ExportCompositionInfoModel(ExportFrameImageProvider* imageProvider,
                                      GetSessionHandler getSessionHandler,
                                      UpdateSessionHandler updateSessionHandler,
                                      QObject* parent = nullptr);
  void setAEResource(std::shared_ptr<AEResource> resource);
  void refreshData(int parentIndex, std::shared_ptr<AEResource> resource);

  Q_INVOKABLE int getWidth() const;
  Q_INVOKABLE int getHeight() const;
  Q_INVOKABLE QString getCurrentTime() const;
  Q_INVOKABLE QString getDuration() const;
  Q_INVOKABLE QString getCurrentFrame() const;
  Q_INVOKABLE QString getImageProviderName() const;
  Q_INVOKABLE void setCurrentFrame(const QString& currentFrame);
  Q_INVOKABLE void setExportAsBmp(int row, bool exportAsBmp);
  Q_INVOKABLE bool isCompositionHasEditableLayer(int row);
  bool isCompositionHasEditableLayer(std::shared_ptr<AEResource> resource);

  int rowCount(const QModelIndex& parent) const override;
  QVariant data(const QModelIndex& index, int role) const override;

  Q_SIGNAL void widthChanged(int width);
  Q_SIGNAL void heightChanged(int height);
  Q_SIGNAL void currentTimeChanged(const QString& currentTime);
  Q_SIGNAL void durationChanged(const QString& duration);
  Q_SIGNAL void currentFrameChanged(const QString& currentFrame);
  Q_SIGNAL void imageProviderNameChanged(const QString& imageProviderName);
  Q_SIGNAL void compositionExportAsBmpChanged();

 protected:
  QHash<int, QByteArray> roleNames() const override;

 private:
  QSize size = {};
  pag::Frame duration = 0;
  pag::Frame currentFrame = 0;
  ExportFrameImageProvider* imageProvider = nullptr;
  std::shared_ptr<AEResource> resource = nullptr;
  std::vector<Data> items = {};
  GetSessionHandler getSessionHandler = nullptr;
  UpdateSessionHandler updateSessionHandler = nullptr;
};

}  // namespace exporter
