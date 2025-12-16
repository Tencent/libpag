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
#include <QMediaPlayer>
#include <QThread>
#include <QTimer>
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <QVideoSink>
#include "codec/CodecContext.h"
#include "pag/file.h"
#include "pag/types.h"
#include "video/PAGMovie.h"

namespace pag {

QImage PAGImageLayerModel::GetVideoFirstFrame(const QString& filePath) {
  QMediaPlayer player;
  QVideoSink sink;
  QEventLoop loop;
  QImage result = {};

  connect(&sink, &QVideoSink::videoFrameChanged, [&](const QVideoFrame& frame) {
    QVideoFrame videoFrame(frame);
    videoFrame.map(QVideoFrame::ReadOnly);
    QImage image = videoFrame.toImage();
    if (!image.isNull()) {
      result = image.copy();
      loop.quit();
    }
    videoFrame.unmap();
  });

  player.setVideoSink(&sink);
  player.setSource(filePath);
  player.setLoops(1);
  player.play();

  QTimer timer;
  timer.setSingleShot(true);
  connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
  timer.start(1000);

  loop.exec();
  player.stop();

  return result;
}

PAGImageLayerModel::PAGImageLayerModel(QObject* parent) : QAbstractListModel(parent) {
}

int PAGImageLayerModel::rowCount(const QModelIndex& parent) const {
  Q_UNUSED(parent);
  return static_cast<int>(imageLayers.count());
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

void PAGImageLayerModel::setPAGFile(std::shared_ptr<PAGFile> pagFile) {
  imageLayers.clear();
  revertSet.clear();

  if (pagFile == nullptr) {
    return;
  }

  _pagFile = std::move(pagFile);
  auto editableList = _pagFile->getEditableIndices(LayerType::Image);
  if (editableList.empty()) {
    return;
  }

  auto file = _pagFile->getFile();

  for (const auto& editableIndex : editableList) {
    auto* layer = file->getImageAt(editableIndex).at(0);
    auto imageData = layer->imageBytes->fileBytes;

    QImage image = QImage::fromData(imageData->data(), static_cast<int>(imageData->length()));
    imageLayers[static_cast<int>(imageLayers.count())] = image;
  }
  beginResetModel();
  endResetModel();
}

void PAGImageLayerModel::changeImage(int index, const QString& filePath) {
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
  if (reader.canRead()) {
    if (!reader.read(&newImage)) {
      qDebug() << "Read image[" << path << "] failed: " << reader.errorString();
    }
  }

  if (newImage.data_ptr() == nullptr) {
    newImage = GetVideoFirstFrame(path);
  }
  if (newImage.data_ptr() == nullptr) {
    qDebug() << "Can not read image[" << path << "]: " << reader.errorString();
    return;
  }

  auto pagImage = PAGImage::FromPath(path.toStdString());
  if (pagImage == nullptr) {
    pagImage = PAGMovie::MakeFromFile(path.toStdString());
  }
  if (pagImage == nullptr) {
    return;
  }
  replaceImage(index, std::move(pagImage));

  revertSet.insert(index);
  imageLayers[index] = newImage;
  beginResetModel();
  endResetModel();
}

void PAGImageLayerModel::revertImage(int index) {
  if (index < 0 || index >= imageLayers.count()) {
    return;
  }
  if (revertSet.find(index) == revertSet.end()) {
    return;
  }

  auto file = _pagFile->getFile();
  auto editableImages = _pagFile->getEditableIndices(pag::LayerType::Image);
  auto* layer = file->getImageAt(editableImages.at(index)).at(0);
  auto imageData = layer->imageBytes->fileBytes;
  auto image = QImage::fromData(imageData->data(), static_cast<int>(imageData->length()));
  replaceImage(index, nullptr);
  revertSet.remove(index);
  imageLayers[index] = image;
  beginResetModel();
  endResetModel();
}

int PAGImageLayerModel::convertIndex(int index) {
  auto editableIndices = _pagFile->getEditableIndices(pag::LayerType::Image);
  return editableIndices.at(index);
}

QHash<int, QByteArray> PAGImageLayerModel::roleNames() const {
  static QHash<int, QByteArray> roles = {
      {static_cast<int>(PAGImageLayerRoles::IndexRole), "value"},
      {static_cast<int>(PAGImageLayerRoles::ReveryRole), "canRevert"},
  };
  return roles;
}

void PAGImageLayerModel::replaceImage(int index, std::shared_ptr<PAGImage> image) {
  _pagFile->replaceImage(convertIndex(index), std::move(image));
  Q_EMIT imageChanged();
}

}  // namespace pag
