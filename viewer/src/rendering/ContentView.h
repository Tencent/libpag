/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#include <QQuickItem>
#include <QTimer>
#include <atomic>
#include <memory>
#include "IContentRenderer.h"
#include "pag/file.h"
#include "pag/pag.h"
#include "pagx/PAGXDocument.h"
#include "platform/qt/GPUDrawable.h"

namespace pag {

class RenderThread;

/**
 * Base class for content views that can display PAG or PAGX files.
 * Defines the common interface for both PAGView and PAGXView.
 */
class ContentView : public QQuickItem {
  Q_OBJECT
 public:
  explicit ContentView(QQuickItem* parent = nullptr);
  ~ContentView() override;

  Q_PROPERTY(int pagWidth READ getWidth NOTIFY widthChanged)
  Q_PROPERTY(int pagHeight READ getHeight NOTIFY heightChanged)
  Q_PROPERTY(bool hasAnimation READ hasAnimation CONSTANT)
  Q_PROPERTY(bool isPlaying READ isPlaying WRITE setIsPlaying NOTIFY isPlayingChanged)
  Q_PROPERTY(double progress READ getProgress WRITE setProgress NOTIFY progressChanged)
  Q_PROPERTY(QString totalFrame READ getTotalFrame)
  Q_PROPERTY(QString currentFrame READ getCurrentFrame)
  Q_PROPERTY(QString duration READ getDuration)
  Q_PROPERTY(QString filePath READ getFilePath NOTIFY filePathChanged)
  Q_PROPERTY(QString displayedTime READ getDisplayedTime)
  Q_PROPERTY(QColor backgroundColor READ getBackgroundColor)
  Q_PROPERTY(QSizeF preferredSize READ getPreferredSize)
  Q_PROPERTY(int editableTextLayerCount READ getEditableTextLayerCount NOTIFY
                 editableTextLayerCountChanged)
  Q_PROPERTY(int editableImageLayerCount READ getEditableImageLayerCount NOTIFY
                 editableImageLayerCountChanged)
  Q_PROPERTY(bool showVideoFrames READ getShowVideoFrames WRITE setShowVideoFrames)

  virtual int getWidth() const = 0;
  virtual int getHeight() const = 0;
  virtual bool hasAnimation() const = 0;
  virtual bool isPlaying() const = 0;
  virtual double getProgress() const = 0;
  virtual QString getTotalFrame() const = 0;
  virtual QString getCurrentFrame() const = 0;
  virtual QString getDuration() const = 0;
  virtual QString getFilePath() const = 0;
  virtual QString getDisplayedTime() const = 0;
  virtual QColor getBackgroundColor() const = 0;
  virtual QSizeF getPreferredSize() const = 0;
  virtual int getEditableTextLayerCount() const = 0;
  virtual int getEditableImageLayerCount() const = 0;
  virtual bool getShowVideoFrames() const = 0;

  virtual void setIsPlaying(bool isPlaying) = 0;
  virtual void setProgress(double progress) = 0;
  virtual void setShowVideoFrames(bool isShow) = 0;

  Q_SIGNAL void isPlayingChanged(bool isPlaying);
  Q_SIGNAL void progressChanged(double progress);
  Q_SIGNAL void fileChanged(std::shared_ptr<pag::File> file);
  Q_SIGNAL void filePathChanged(const QString& filePath);
  Q_SIGNAL void pagFileChanged(std::shared_ptr<pag::PAGFile> pagFile);
  Q_SIGNAL void pagxDocumentChanged(std::shared_ptr<pagx::PAGXDocument> pagxDocument);
  Q_SIGNAL void editableTextLayerCountChanged(int editableTextLayerCount);
  Q_SIGNAL void editableImageLayerCountChanged(int editableImageLayerCount);
  Q_SIGNAL void widthChanged(int width);
  Q_SIGNAL void heightChanged(int height);

  Q_INVOKABLE virtual bool setFile(const QString& filePath) = 0;
  Q_INVOKABLE virtual void firstFrame() = 0;
  Q_INVOKABLE virtual void lastFrame() = 0;
  Q_INVOKABLE virtual void nextFrame() = 0;
  Q_INVOKABLE virtual void previousFrame() = 0;

  RenderThread* getRenderThread() const;

 protected:
  Q_SLOT void sizeChangedDelayHandle();
  Q_SLOT void onWindowChanged(QQuickWindow* win);

  virtual void initDrawable();
  virtual void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

  std::shared_ptr<GPUDrawable> drawable = nullptr;
  std::unique_ptr<IContentRenderer> contentRenderer = nullptr;
  std::unique_ptr<RenderThread> renderThread = nullptr;
  std::unique_ptr<QTimer> resizeTimer = nullptr;
  std::atomic_bool sizeChanged = false;
};
}  // namespace pag
