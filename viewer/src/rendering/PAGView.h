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

#include <QOpenGLContext>
#include <QQuickItem>
#include <QTimer>
#include "audio/PAGAudioPlayer.h"
#include "pag/pag.h"
#include "platform/qt/GPUDrawable.h"
#include "rendering/PAGRenderThread.h"

namespace pag {

class PAGView : public QQuickItem {
  Q_OBJECT
 public:
  explicit PAGView(QQuickItem* parent = nullptr);
  ~PAGView() override;

  Q_PROPERTY(int pagWidth READ getPAGWidth)
  Q_PROPERTY(int pagHeight READ getPAGHeight)
  Q_PROPERTY(int editableTextLayerCount READ getEditableTextLayerCount NOTIFY
                 editableTextLayerCountChanged)
  Q_PROPERTY(int editableImageLayerCount READ getEditableImageLayerCount NOTIFY
                 editableImageLayerCountChanged)
  Q_PROPERTY(bool isPlaying READ isPlaying WRITE setIsPlaying NOTIFY isPlayingChanged)
  Q_PROPERTY(bool showVideoFrames READ getShowVideoFrames WRITE setShowVideoFrames)
  Q_PROPERTY(double progress READ getProgress WRITE setProgress NOTIFY progressChanged)
  Q_PROPERTY(QString totalFrame READ getTotalFrame)
  Q_PROPERTY(QString currentFrame READ getCurrentFrame)
  Q_PROPERTY(QString duration READ getDuration)
  Q_PROPERTY(QString filePath READ getFilePath NOTIFY fileChanged)
  Q_PROPERTY(QString displayedTime READ getDisplayedTime)
  Q_PROPERTY(QColor backgroundColor READ getBackgroundColor)
  Q_PROPERTY(QSizeF preferredSize READ getPreferredSize)

  int getPAGWidth() const;
  int getPAGHeight() const;
  int getEditableTextLayerCount() const;
  int getEditableImageLayerCount() const;

  bool isPlaying() const;
  bool getShowVideoFrames() const;
  double getProgress() const;
  QString getTotalFrame() const;
  QString getCurrentFrame() const;
  QString getDuration() const;
  QString getFilePath() const;
  QString getDisplayedTime() const;
  QColor getBackgroundColor() const;
  QSizeF getPreferredSize() const;

  void setIsPlaying(bool isPlaying);
  void setShowVideoFrames(bool isShow);
  void setProgress(double progress);
  void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

  Q_SIGNAL void editableTextLayerCountChanged(int editableTextLayerCount);
  Q_SIGNAL void editableImageLayerCountChanged(int editableImageLayerCount);
  Q_SIGNAL void isPlayingChanged(bool isPlaying);
  Q_SIGNAL void progressChanged(double progress);
  Q_SIGNAL void fileChanged(std::shared_ptr<pag::File> file);
  Q_SIGNAL void filePathChanged(const std::string& filePath);
  Q_SIGNAL void pagFileChanged(std::shared_ptr<pag::PAGFile> pagFile);

  Q_SLOT void flush() const;
  Q_SLOT void sizeChangedDelayHandle();
  Q_SLOT void onAudioTimeChanged(int64_t audioTime);

  Q_INVOKABLE bool setFile(const QString& filePath);
  Q_INVOKABLE void firstFrame();
  Q_INVOKABLE void lastFrame();
  Q_INVOKABLE void nextFrame();
  Q_INVOKABLE void previousFrame();

  QSGNode* updatePaintNode(QSGNode*, UpdatePaintNodeData*) override;
  PAGRenderThread* getRenderThread() const;

 private:
  void setProgressInternal(double progress, bool isAudioSeek);

  int editableTextLayerCount = 0;
  int editableImageLayerCount = 0;
  int64_t lastPlayTime = 0;
  bool isPlaying_ = true;
  std::atomic_bool sizeChanged = false;
  qreal lastWidth = 0;
  qreal lastHeight = 0;
  qreal lastPixelRatio = 1;
  double progress = 0.0;
  double progressPerFrame = 0.0;
  std::unique_ptr<QTimer> resizeTimer = nullptr;
  std::unique_ptr<PAGPlayer> pagPlayer = nullptr;
  std::unique_ptr<PAGRenderThread> renderThread = nullptr;
  std::shared_ptr<PAGFile> pagFile = nullptr;
  std::shared_ptr<GPUDrawable> drawable = nullptr;
  std::unique_ptr<PAGAudioPlayer> audioPlayer = nullptr;

  friend class PAGRenderThread;
};
}  // namespace pag