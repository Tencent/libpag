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

#include <atomic>
#include <mutex>
#include "pagx/PAGXDocument.h"
#include "rendering/ContentViewModel.h"
#include "rendering/pagx/XmlLinesModel.h"
#include "tgfx/layers/DisplayList.h"
#include "tgfx/layers/Layer.h"

class QQuickWindow;

namespace pag {

/**
 * ViewModel for PAGX format content. Owns PAGXDocument and DisplayList,
 * and manages playback state for PAGX content.
 */
class PAGXViewModel : public ContentViewModel {
  Q_OBJECT
  Q_PROPERTY(XmlLinesModel* linesModel READ linesModel CONSTANT)
 public:
  explicit PAGXViewModel(QObject* parent = nullptr);

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
  ContentType getContentType() const override;

  void setIsPlaying(bool isPlaying) override;
  void setProgress(double progress) override;
  void setShowVideoFrames(bool isShow) override;

  Q_INVOKABLE bool loadFile(const QString& filePath) override;
  Q_INVOKABLE void firstFrame() override;
  Q_INVOKABLE void lastFrame() override;
  Q_INVOKABLE void nextFrame() override;
  Q_INVOKABLE void previousFrame() override;

  XmlLinesModel* linesModel() const;
  Q_INVOKABLE QString applyXmlChanges(const QString& newXml);
  Q_INVOKABLE QString saveXmlToFile(const QString& xml);

  struct RenderState {
    std::shared_ptr<tgfx::DisplayList> displayList;
    std::shared_ptr<tgfx::Layer> contentLayer;
    int contentWidth = 0;
    int contentHeight = 0;
  };

  void setWindow(QQuickWindow* window);
  bool takeNeedsRender();
  void markNeedsRender();
  // Returns a snapshot of render state with shared ownership; safe to use without holding any lock.
  RenderState getRenderState();
  bool hasContent();

  Q_SIGNAL void pagxDocumentChanged(std::shared_ptr<pagx::PAGXDocument> pagxDocument);

  /**
   * Called by PAGXView when the render thread completes a render.
   * This is used to defer XmlLinesModel updates until the first render is done.
   */
  Q_SLOT void onRenderCompleted();

 private:
  void clearContent();
  void updateAnimationState();

  QQuickWindow* window = nullptr;
  std::atomic_bool needsRender = false;
  std::mutex renderMutex = {};
  std::shared_ptr<pagx::PAGXDocument> pagxDocument = nullptr;
  std::shared_ptr<tgfx::Layer> pagxContentLayer = nullptr;
  std::shared_ptr<tgfx::DisplayList> displayList = nullptr;
  int pagxWidth = 0;
  int pagxHeight = 0;
  std::string currentFilePath = {};
  XmlLinesModel* xmlLinesModel = nullptr;
  QString pendingXmlContent = {};
  int64_t totalFrames = 1;
  float frameRate = 0.0f;
  double progress = 0.0;
  double progressPerFrame = 1.0;
  std::atomic<bool> isPlaying_ = false;
};

}  // namespace pag
