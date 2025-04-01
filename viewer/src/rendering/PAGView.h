/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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
#include "pag/pag.h"
#include "platform/qt/GPUDrawable.h"

namespace pag {

class PAGRenderThread;

class PAGView : public QQuickItem {
  Q_OBJECT
 public:
  explicit PAGView(QQuickItem* parent = nullptr);
  ~PAGView() override;

  Q_PROPERTY(int pagWidth READ getPAGWidth)
  Q_PROPERTY(int pagHeight READ getPAGHeight)
  Q_PROPERTY(int totalFrame READ getTotalFrame)
  Q_PROPERTY(int currentFrame READ getCurrentFrame)
  Q_PROPERTY(bool isPlaying READ isPlaying WRITE setIsPlaying NOTIFY isPlayingChanged)
  Q_PROPERTY(bool showVideoFrames READ getShowVideoFrames WRITE setShowVideoFrames)
  Q_PROPERTY(double duration READ getDuration)
  Q_PROPERTY(double progress READ getProgress WRITE setProgress NOTIFY progressChanged)
  Q_PROPERTY(QString filePath READ getFilePath NOTIFY fileChanged)
  Q_PROPERTY(QColor backgroundColor READ getBackgroundColor)
  Q_PROPERTY(QSizeF preferredSize READ getPreferredSize)

  auto getPAGWidth() const -> int;
  auto getPAGHeight() const -> int;
  auto getTotalFrame() const -> int;
  auto getCurrentFrame() const -> int;
  auto isPlaying() const -> bool;
  auto getShowVideoFrames() const -> bool;
  auto getDuration() const -> double;
  auto getProgress() const -> double;
  auto getFilePath() const -> QString;
  auto getBackgroundColor() const -> QColor;
  auto getPreferredSize() const -> QSizeF;

  auto setIsPlaying(bool isPlaying) -> void;
  auto setShowVideoFrames(bool isShow) -> void;
  auto setProgress(double progress) -> void;

  Q_SIGNAL void isPlayingChanged(bool isPlaying);
  Q_SIGNAL void progressChanged(double progress);
  Q_SIGNAL void fileChanged(std::shared_ptr<pag::PAGFile> pagFile, std::string filePath);

  Q_INVOKABLE bool setFile(const QString& filePath);
  Q_INVOKABLE void firstFrame();
  Q_INVOKABLE void lastFrame();
  Q_INVOKABLE void nextFrame();
  Q_INVOKABLE void previousFrame();

  auto updatePaintNode(QSGNode*, UpdatePaintNodeData*) -> QSGNode* override;

 private:
  int64_t lastPlayTime = 0;
  bool isPlaying_ = true;
  qreal lastWidth = 0;
  qreal lastHeight = 0;
  qreal lastPixelRatio = 1;
  double progress = 0.0;
  double progressPerFrame = 0.0;
  QString filePath = "";
  PAGPlayer* pagPlayer = nullptr;
  PAGRenderThread* renderThread = nullptr;
  std::shared_ptr<PAGFile> pagFile = nullptr;
  std::shared_ptr<GPUDrawable> drawable = nullptr;

  friend class PAGRenderThread;
};
}  // namespace pag
