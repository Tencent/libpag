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
#include "ContentViewModel.h"
#include "platform/qt/GPUDrawable.h"

namespace pag {

class RenderThread;

/**
 * Base class for content views that render PAG or PAGX files via the Qt Scene Graph.
 * Manages the render thread lifecycle, drawable, and resize debouncing.
 * All business state is owned by the subclass ViewModel, accessible via getViewModel().
 */
class ContentView : public QQuickItem {
  Q_OBJECT
 public:
  Q_PROPERTY(ContentViewModel* viewModel READ getViewModel CONSTANT)

  explicit ContentView(QQuickItem* parent = nullptr);
  ~ContentView() override;

  virtual ContentViewModel* getViewModel() const = 0;
  RenderThread* getRenderThread() const;

  /// Releases the GPU drawable and shuts down the render thread.
  /// Called from QML before the Loader switches components, ensuring the drawable
  /// is released while the item is still attached to the window.
  /// Note: the same releaseDrawable() effect is also triggered by ~ContentView() and
  /// itemChange(ItemSceneChange) when the item loses its window, but those fire too
  /// late for a Loader component switch where RenderThread needs the window context.
  Q_INVOKABLE void prepareForRemoval();
  Q_INVOKABLE void zoomAt(double factor, double x, double y);
  Q_INVOKABLE void panBy(double deltaX, double deltaY);
  Q_INVOKABLE void resetView();

  /**
   * Updates the QSG texture node with the current drawable's texture.
   * Handles both node creation and texture binding.
   * Returns the updated node, or nullptr if drawable is not available.
   */
  QSGNode* updateTextureNode(QSGNode* oldNode);

  Q_SLOT virtual void flush() const = 0;

  // Schedules a render-thread flush via a queued connection.
  void triggerFlush() const;

  bool takeSizeChanged();
  GPUDrawable* getDrawable() const;

  /// True while the renderer expects further frames. Views that re-flush once per vsync consult it
  /// so an idle renderer, such as a PAGX state machine waiting for an input, stops the loop instead
  /// of re-rendering identical pixels at the refresh rate.
  bool isRenderActive() const;

  friend class RenderThread;

 protected:
  Q_SLOT virtual void sizeChangedDelayHandle();
  Q_SLOT void onWindowChanged(QQuickWindow* win);

  virtual void initDrawable();
  virtual void releaseDrawable();
  virtual void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
  void itemChange(ItemChange change, const ItemChangeData& value) override;

  /// 开始内容驱动的尺寸调整，在当前调整对应的首帧渲染完成后退出；之后的几何变化
  /// 恢复普通窗口 resize 防抖。旧调整的渲染完成事件不会结束新的调整。
  void beginContentResize();

  // Called at the end of sizeChangedDelayHandle(). Subclasses may override to perform
  // additional work after a resize debounce, such as marking render state dirty.
  virtual void onSizeChangedDelayHandled() {
  }

  std::shared_ptr<GPUDrawable> drawable = nullptr;
  std::unique_ptr<RenderThread> renderThread = nullptr;
  std::unique_ptr<QTimer> resizeTimer = nullptr;
  std::atomic_bool sizeChanged = false;
  // Written by the render thread at the end of every flush, read by the view on the GUI thread.
  std::atomic_bool renderActive = true;
  bool contentResizing = false;

 private:
  void setRenderActive(bool active);
  Q_SLOT void endContentResize(quint64 generation);
  QRectF textureRect(QSGTexture* texture) const;

  // 纹理发布与尺寸快照更新必须和 QSG 纹理读取互斥，避免新纹理搭配旧尺寸。
  std::mutex renderedFrameMutex = {};
  QSizeF renderedContentSize = {};
  std::atomic<quint64> contentResizeGeneration = 0;
};

}  // namespace pag
