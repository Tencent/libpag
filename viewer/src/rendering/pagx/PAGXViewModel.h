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
#include "pagx/PAGAnimation.h"
#include "pagx/PAGScene.h"
#include "pagx/PAGXDocument.h"
#include "rendering/ContentViewModel.h"
#include "rendering/pagx/XmlLinesModel.h"

class QQuickWindow;

namespace pag {

/**
 * ViewModel for PAGX format content. Owns PAGXDocument and PAGScene with PAGTimeline,
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
    std::shared_ptr<pagx::PAGScene> scene;
    std::shared_ptr<pagx::PAGAnimation> animation;
    pagx::LoopMode loopMode = pagx::LoopMode::Once;
    int contentWidth = 0;
    int contentHeight = 0;
    bool isPlaying = false;
    double progress = 0.0;
    bool seekRequested = false;
    uint64_t generation = 0;
  };

  void setWindow(QQuickWindow* window);
  bool takeNeedsRender();
  void markNeedsRender();
  /**
   * Returns a snapshot of render state with shared ownership; the render thread may then operate on
   * the snapshot's scene/timeline without holding renderMutex.
   *
   * This is safe even though PAGScene/PAGTimeline are not thread-safe, because each content load
   * builds brand-new scene/timeline objects (never reused) and swaps them into the members under
   * renderMutex. The main thread performs all of its accesses to a given object (build,
   * getAnimation, updateAnimationState) inside that lock and never touches the object again
   * after the swap. The render thread reaches an object only via this snapshot, whose lock
   * establishes a happens-before ordering with the main thread's setup. So any single
   * scene/timeline is accessed by the main thread first (under the lock) and by the render thread
   * afterwards, never concurrently: the caller-serialization the classes require is satisfied.
   */
  RenderState getRenderState();
  bool hasContent();

  /**
   * Updates playback progress from any thread; the signal is emitted on the main thread. The
   * generation identifies the playback session the update belongs to, so an update from a session
   * that ended (e.g. after a content reload) is ignored on the main thread.
   */
  void updateProgressFromRender(double newProgress, uint64_t generation);

  /**
   * Signals from any thread that the animation for the given playback generation has reached its
   * end and stopped; the playing state is cleared on the main thread only if the generation is
   * still current.
   */
  void notifyPlaybackFinished(uint64_t generation);

  Q_SIGNAL void pagxDocumentChanged(std::shared_ptr<pagx::PAGXDocument> pagxDocument);

  /**
   * Called by PAGXView when the render thread completes a render.
   * This is used to defer XmlLinesModel updates until the first render is done.
   */
  Q_SLOT void onRenderCompleted();

 private:
  void clearContent();
  void resolveDefaultAnimation(const std::shared_ptr<pagx::PAGXDocument>& document);
  void updateAnimationState();

  // Emits the content-state reset signals shared by loadFile (both success and failure paths) and
  // applyXmlChanges: progress, playing, total-frame and has-animation. Centralized so a new content
  // load cannot silently forget to refresh one of them. Call after updateAnimationState() so the
  // emitted values reflect the freshly loaded (or cleared) content.
  void emitContentStateReset();

  // Applies a progress update on the main thread. Invoked by name from updateProgressFromRender via
  // a queued connection so the signal is always emitted on the main thread. The generation
  // identifies the playback session; a stale update from a session that ended is ignored.
  Q_SLOT void applyProgressFromRender(double newProgress, quint64 generation);

  // Clears the playing state on the main thread. Invoked by name from notifyPlaybackFinished via a
  // queued connection when the animation reaches its end. The generation identifies the playback
  // session the notification belongs to, so a stale event from a finished session is ignored.
  Q_SLOT void handlePlaybackFinished(quint64 generation);

  QQuickWindow* window = nullptr;
  std::atomic_bool needsRender = false;
  std::atomic<bool> pendingSeek = false;
  std::mutex renderMutex = {};
  std::shared_ptr<pagx::PAGXDocument> pagxDocument = nullptr;
  std::shared_ptr<pagx::PAGScene> scene = nullptr;
  std::shared_ptr<pagx::PAGAnimation> defaultAnimation = nullptr;
  pagx::LoopMode defaultLoopMode = pagx::LoopMode::Once;
  int pagxWidth = 0;
  int pagxHeight = 0;
  std::string currentFilePath = {};
  XmlLinesModel* xmlLinesModel = nullptr;
  QString pendingXmlContent = {};
  int64_t totalFrames = 1;
  float frameRate = 0.0f;
  std::atomic<double> progress = 0.0;
  double progressPerFrame = 1.0;
  std::atomic<bool> isPlaying_ = false;
  // Incremented each time playback starts. Stamped onto finish notifications so a notification from
  // a prior session can be distinguished from the current one and dropped if the user restarted.
  std::atomic<uint64_t> playbackGeneration = 0;
};

}  // namespace pag
