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

#include "rendering/pag/PAGView.h"
#include "rendering/RenderThread.h"
#include "rendering/pag/PAGRenderer.h"
#include "tgfx/core/Clock.h"

namespace pag {

static constexpr int64_t MaxAudioLeadThreshold = 25000;
static constexpr int64_t MinAudioLagThreshold = -100000;

PAGView::PAGView(QQuickItem* parent) : ContentView(parent) {
  viewModel = std::make_unique<PAGViewModel>();
  connect(viewModel.get(), &ContentViewModel::requestFlush, this, &PAGView::triggerFlush);
  connect(viewModel.get(), &ContentViewModel::contentSizeChanged, this,
          &PAGView::onRequestSizeChanged);
  connect(viewModel.get(), &PAGViewModel::preferredSizeChanged, this,
          &PAGView::onPreferredSizeChanged);
  connect(viewModel.get(), &PAGViewModel::audioTimeChanged, this, &PAGView::onAudioTimeChanged);
  connect(viewModel.get(), &PAGViewModel::isPlayingChanged, this, &PAGView::onIsPlayingChanged);
  connect(viewModel.get(), &PAGViewModel::fileChanged, this, &PAGView::onFileChanged);
  renderThread =
      std::make_unique<RenderThread>(this, std::make_unique<PAGRenderer>(viewModel.get()));
  connect(renderThread.get(), &RenderThread::rendered, this, &PAGView::update,
          Qt::QueuedConnection);
  renderThread->moveToThread(renderThread.get());
}

PAGView::~PAGView() {
  // Destructor implemented by ContentView
}

ContentViewModel* PAGView::getViewModel() const {
  return viewModel.get();
}

void PAGView::flush() const {
  if (viewModel->isPlaying()) {
    triggerFlush();
  }
}

void PAGView::initDrawable() {
  if (drawable != nullptr) {
    return;
  }
  drawable = GPUDrawable::MakeFrom(this);
  if (drawable == nullptr) {
    return;
  }
  viewModel->setWindow(this->window());
  auto pagSurface = PAGSurface::MakeFrom(drawable);
  if (pagSurface == nullptr) {
    drawable = nullptr;
    return;
  }
  viewModel->getPAGPlayer()->setSurface(pagSurface);
  if (renderThread != nullptr) {
    drawable->moveToThread(renderThread.get());
  }
}

void PAGView::onRequestSizeChanged() {
  sizeChanged = true;
}

void PAGView::onPreferredSizeChanged() {
  setSize(viewModel->getPreferredSize());
}

void PAGView::onIsPlayingChanged(bool isPlaying) {
  if (isPlaying) {
    std::lock_guard<std::mutex> lock(lastPlayTimeMutex);
    lastPlayTime = tgfx::Clock::Now();
  }
}

void PAGView::onFileChanged(std::shared_ptr<pag::File>) {
  std::lock_guard<std::mutex> lock(lastPlayTimeMutex);
  lastPlayTime = tgfx::Clock::Now();
}

void PAGView::onAudioTimeChanged(int64_t audioTime) {
  auto timeNow = tgfx::Clock::Now();
  int64_t lastPlayTimeCopy = 0;
  {
    std::lock_guard<std::mutex> lock(lastPlayTimeMutex);
    lastPlayTimeCopy = lastPlayTime;
  }
  auto displayTime = timeNow - lastPlayTimeCopy;
  auto duration = static_cast<double>(viewModel->getPAGPlayer()->duration());
  auto currentDisplayTime = static_cast<int64_t>(viewModel->getProgress() * duration) + displayTime;
  if (audioTime == 0 || (audioTime - currentDisplayTime > MaxAudioLeadThreshold)) {
    {
      std::lock_guard<std::mutex> lock(lastPlayTimeMutex);
      lastPlayTime = timeNow;
    }
    viewModel->setProgressInternal(static_cast<double>(audioTime) / duration, false);
  } else if (audioTime - currentDisplayTime < MinAudioLagThreshold) {
    std::lock_guard<std::mutex> lock(lastPlayTimeMutex);
    lastPlayTime = timeNow;
  }
}

QSGNode* PAGView::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
  if (!renderThread->isRunning()) {
    renderThread->start();
    {
      std::lock_guard<std::mutex> lock(lastPlayTimeMutex);
      lastPlayTime = tgfx::Clock::Now();
    }
  }

  auto node = updateTextureNode(oldNode);

  auto timeNow = tgfx::Clock::Now();
  int64_t lastPlayTimeCopy = 0;
  {
    std::lock_guard<std::mutex> lock(lastPlayTimeMutex);
    lastPlayTimeCopy = lastPlayTime;
    lastPlayTime = timeNow;
  }
  auto displayTime = timeNow - lastPlayTimeCopy;

  if (viewModel->isPlaying()) {
    auto duration = viewModel->getPAGPlayer()->duration();
    if (duration > 0) {
      auto newProgress = static_cast<double>(displayTime) / static_cast<double>(duration) +
                         viewModel->getProgress();
      if (newProgress > 1.0) {
        newProgress = 0.0;
      }
      viewModel->setProgressInternal(newProgress, false);
    }
  }

  return node;
}

}  // namespace pag
