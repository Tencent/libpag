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
#include <QQuickWindow>
#include "ContentView.h"
#include "audio/PAGAudioPlayer.h"

namespace pag {

class PAGView : public ContentView {
  Q_OBJECT
 public:
  explicit PAGView(QQuickItem* parent = nullptr);
  ~PAGView() override;

  int getWidth() const override;
  int getHeight() const override;
  bool hasAnimation() const override;
  int getEditableTextLayerCount() const override;
  int getEditableImageLayerCount() const override;

  bool isPlaying() const override;
  bool getShowVideoFrames() const override;
  double getProgress() const override;
  QString getTotalFrame() const override;
  QString getCurrentFrame() const override;
  QString getDuration() const override;
  QString getFilePath() const override;
  QString getDisplayedTime() const override;
  QColor getBackgroundColor() const override;
  QSizeF getPreferredSize() const override;

  void setIsPlaying(bool isPlaying) override;
  void setShowVideoFrames(bool isShow) override;
  void setProgress(double progress) override;
  void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

  Q_SLOT void flush() const;
  Q_SLOT void onAudioTimeChanged(int64_t audioTime);

  Q_INVOKABLE bool setFile(const QString& filePath) override;
  Q_INVOKABLE void firstFrame() override;
  Q_INVOKABLE void lastFrame() override;
  Q_INVOKABLE void nextFrame() override;
  Q_INVOKABLE void previousFrame() override;

  QSGNode* updatePaintNode(QSGNode*, UpdatePaintNodeData*) override;

 private:
  void setProgressInternal(double progress, bool isAudioSeek);
  void initDrawable() override;

  int editableTextLayerCount = 0;
  int editableImageLayerCount = 0;
  int64_t lastPlayTime = 0;
  bool isPlaying_ = true;
  qreal lastWidth = 0;
  qreal lastHeight = 0;
  qreal lastPixelRatio = 1;
  double progress = 0.0;
  double progressPerFrame = 0.0;
  std::unique_ptr<PAGPlayer> pagPlayer = nullptr;
  std::shared_ptr<PAGFile> pagFile = nullptr;
  std::unique_ptr<PAGAudioPlayer> audioPlayer = nullptr;

  friend class RenderThread;
};
}  // namespace pag
