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

#include "PAGImageLayerModel.h"
#include <QImageReader>
#include "codec/CodecContext.h"
#include "pag/file.h"
#include "pag/types.h"

namespace pag {

PAGImageLayerModel::PAGImageLayerModel(QObject* parent) : QAbstractListModel(parent) {
}

int PAGImageLayerModel::rowCount(const QModelIndex& parent) const {
  Q_UNUSED(parent);
  return imageLayers.count();
}

QVariant PAGImageLayerModel::data(const QModelIndex& index, int role) const {
  if (index.row() < 0 || index.row() >= imageLayers.count()) {
    return {};
  }

  if (role == static_cast<int>(PAGImageLayerRoles::IndexRole)) {
    return index.row();
  }
  if (role == static_cast<int>(PAGImageLayerRoles::ReveryRole)) {
    return {revertSet.find(index.row()) != revertSet.end()};
  }

  return {};
}

QImage PAGImageLayerModel::requestImage(int index) {
  if (index < 0 || index >= imageLayers.count()) {
    return {};
  }

  return imageLayers[index];
}

void PAGImageLayerModel::setPAGFile(const std::shared_ptr<PAGFile>& pagFile) {
  beginResetModel();
  imageLayers.clear();
  revertSet.clear();
  endResetModel();

  if (pagFile == nullptr) {
    return;
  }

  this->pagFile = pagFile;
  auto editableList = pagFile->getEditableIndices(LayerType::Image);
  if (editableList.empty()) {
    return;
  }

  auto file = pagFile->getFile();

  beginResetModel();
  for (const auto& editableIndex : editableList) {
    auto* layer = file->getImageAt(editableIndex).at(0);
    auto imageData = layer->imageBytes->fileBytes;

    QImage image = QImage::fromData(imageData->data(), static_cast<int>(imageData->length()));
    imageLayers[imageLayers.count()] = image;
  }
  endResetModel();
}

void PAGImageLayerModel::replaceImage(int index, const QString& filePath) {
  if (index < 0 || index >= imageLayers.count()) {
    return;
  }
  QImage newImage;
  auto path = filePath;
  if (path.startsWith("file://")) {
    path = QUrl(path).toLocalFile();
  }

  QImageReader reader(path);
  reader.setDecideFormatFromContent(true);
  if (!reader.canRead()) {
    qDebug() << "Can not read image[" << path << "]: " << reader.errorString();
    return;
  }
  if (!reader.read(&newImage)) {
    qDebug() << "Read image[" << path << "] failed: " << reader.errorString();
    return;
  }

  if (newImage.data_ptr() == nullptr) {
    return;
  }

  auto pagImage = PAGImage::FromPath(path.toStdString());
  if (pagImage == nullptr) {
    return;
  }
  pagFile->replaceImage(convertIndex(index), pagImage);

  beginResetModel();
  revertSet.insert(index);
  imageLayers[index] = newImage;
  endResetModel();
}

void PAGImageLayerModel::revertImage(int index) {
  if (index < 0 || index >= imageLayers.count()) {
    return;
  }
  if (revertSet.find(index) == revertSet.end()) {
    return;
  }

  auto file = pagFile->getFile();
  auto editableImages = pagFile->getEditableIndices(pag::LayerType::Image);
  auto* layer = file->getImageAt(editableImages.at(index)).at(0);
  auto imageData = layer->imageBytes->fileBytes;
  auto image = QImage::fromData(imageData->data(), static_cast<int>(imageData->length()));
  pagFile->replaceImage(convertIndex(index), nullptr);
  beginResetModel();
  revertSet.remove(index);
  imageLayers[index] = image;
  endResetModel();
}

int PAGImageLayerModel::convertIndex(int index) {
  auto editableIndices = pagFile->getEditableIndices(pag::LayerType::Image);
  return editableIndices.at(index);
}

QHash<int, QByteArray> PAGImageLayerModel::roleNames() const {
  static QHash<int, QByteArray> roles = {
      {static_cast<int>(PAGImageLayerRoles::IndexRole), "value"},
      {static_cast<int>(PAGImageLayerRoles::ReveryRole), "canRevert"},
  };
  return roles;
}

}  // namespace pag
