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

#include <QOpenGLContext>
#include <QTimer>
#include "ContentView.h"
#include "pagx/PAGXDocument.h"
#include "platform/qt/GPUDrawable.h"
#include "tgfx/layers/DisplayList.h"
#include "tgfx/layers/Layer.h"

namespace pag {

class PAGXRenderThread;

/**
 * PAGXView is a view for rendering PAGX files.
 */
class PAGXView : public ContentView {
  Q_OBJECT
 public:
  explicit PAGXView(QQuickItem* parent = nullptr);
  ~PAGXView() override;

  int getWidth() const override;
  int getHeight() const override;
  bool hasAnimation() const override;
  bool isPlaying() const override;
  double getProgress() const override;
  QString getTotalFrame() const override;
  QString getCurrentFrame() const override;
  QString getDuration() const override;
  QString getFilePath() const override;
  QString getDisplayedTime() const override;
  QColor getBackgroundColor() const override;
  QSizeF getPreferredSize() const override;
  int getEditableTextLayerCount() const override;
  int getEditableImageLayerCount() const override;
  bool getShowVideoFrames() const override;

  void setIsPlaying(bool isPlaying) override;
  void setProgress(double progress) override;
  void setShowVideoFrames(bool isShow) override;
  void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

  Q_SLOT void sizeChangedDelayHandle();

  Q_INVOKABLE bool setFile(const QString& filePath) override;
  Q_INVOKABLE void firstFrame() override;
  Q_INVOKABLE void lastFrame() override;
  Q_INVOKABLE void nextFrame() override;
  Q_INVOKABLE void previousFrame() override;

  void flush() const;

  PAGXRenderThread* getRenderThread() const;

  QSGNode* updatePaintNode(QSGNode*, UpdatePaintNodeData*) override;

  struct RenderTimeMetrics {
    int64_t renderTime = 0;
    int64_t imageTime = 0;
    int64_t presentTime = 0;
  };

 private:
  RenderTimeMetrics renderPAGX();
  void clearContent();
  void updateAnimationState();

  std::atomic_bool sizeChanged = false;
  std::atomic_bool needsRender = false;
  std::unique_ptr<QTimer> resizeTimer = nullptr;
  std::shared_ptr<GPUDrawable> drawable = nullptr;
  std::unique_ptr<PAGXRenderThread> renderThread = nullptr;

  std::shared_ptr<pagx::PAGXDocument> pagxDocument = nullptr;
  std::shared_ptr<tgfx::Layer> pagxContentLayer = nullptr;
  std::unique_ptr<tgfx::DisplayList> displayList = nullptr;
  int pagxWidth = 0;
  int pagxHeight = 0;
  std::string currentFilePath = {};

  int64_t totalFrames = 1;
  float frameRate = 0.0f;
  double progress = 0.0;
  double progressPerFrame = 1.0;
  bool isPlaying_ = false;
  int64_t lastPlayTime = 0;

  friend class PAGXRenderThread;
};
}  // namespace pag
