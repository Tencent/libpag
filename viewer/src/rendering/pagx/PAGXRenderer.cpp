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

#include "rendering/pagx/PAGXRenderer.h"
#include <cmath>
#include <cstdlib>
#include "pagx/PAGSurface.h"
#include "pagx/tgfx.h"
#include "tgfx/core/Clock.h"

namespace pag {

// Minimum change in seek target (microseconds) before re-seeking the paused timeline. Avoids
// redundant setCurrentTime/apply calls when the progress position is effectively unchanged.
static constexpr int64_t SeekThresholdUs = 1000;

PAGXRenderer::PAGXRenderer(PAGXViewModel* viewModel) : viewModel(viewModel) {
}

void PAGXRenderer::setDrawable(GPUDrawable* drawable) {
  this->drawable = drawable;
}

bool PAGXRenderer::isReady() const {
  return viewModel != nullptr && drawable != nullptr;
}

void PAGXRenderer::updateSize() {
  if (drawable != nullptr) {
    drawable->freeSurface();
    drawable->updateSize();
  }
}

IContentRenderer::RenderMetrics PAGXRenderer::flush() {
  RenderMetrics metrics = {};

  if (!viewModel->takeNeedsRender()) {
    return metrics;
  }
  if (!isReady()) {
    return metrics;
  }

  auto state = viewModel->getRenderState();
  if (state.scene == nullptr) {
    return metrics;
  }

  auto device = drawable->getDevice();
  if (device == nullptr) {
    return metrics;
  }
  auto context = device->lockContext();
  if (context == nullptr) {
    return metrics;
  }
  auto tgfxSurface = drawable->getSurface(context, false);
  if (tgfxSurface == nullptr) {
    device->unlock();
    return metrics;
  }

  // Detect play-state transitions to reset the advance/seek trackers so a paused→resumed cycle
  // does not count the pause time as animation delta, and the next paused frame always re-seeks.
  bool playStateChanged = state.isPlaying != lastIsPlaying;
  if (playStateChanged) {
    lastIsPlaying = state.isPlaying;
    lastAdvanceTimeUs = -1;
    lastSeekTimeUs = -1;
    playbackFinishNotified = false;
  }

  if (state.isPlaying && state.timeline != nullptr && state.timeline->duration() > 0) {
    advancePlayback(state, playStateChanged);
  } else if (state.timeline != nullptr && state.timeline->duration() > 0) {
    seekPaused(state, playStateChanged);
  }

  metrics.currentFrame = computeProfileFrame(state);

  if (state.contentWidth > 0 && state.contentHeight > 0) {
    applyFitTransform(state, tgfxSurface->width(), tgfxSurface->height());
  }

  auto renderStart = tgfx::Clock::Now();
  auto pagSurface = pagx::MakeFrom(tgfxSurface);
  if (pagSurface == nullptr) {
    device->unlock();
    return metrics;
  }
  auto recording = pagx::Record(context, state.scene, pagSurface, true);
  if (recording) {
    context->submit(std::move(recording));
  }
  metrics.renderTime = tgfx::Clock::Now() - renderStart;
  // PAGX rendering via pagx::Record has no distinct image-decode phase, so imageDecodeTime stays 0.

  auto presentStart = tgfx::Clock::Now();
  drawable->present(context);
  metrics.presentTime = tgfx::Clock::Now() - presentStart;

  metrics.rendered = true;
  device->unlock();
  return metrics;
}

void PAGXRenderer::advancePlayback(PAGXViewModel::RenderState& state, bool playStateChanged) {
  // On a fresh play request, re-arm the timeline. PAGTimeline gates advance() on an internal
  // playing flag that a non-looping animation clears once it reaches the end, so replaying after
  // it finished requires rewinding to the start and calling play() again.
  if (playStateChanged) {
    // Seek to the requested progress so playback starts from the position the UI shows,
    // regardless of where the timeline was left. A finished timeline is instead rewound to the
    // start: the replay path (play button) leaves progress at ~1.0, and the user expects a
    // restart rather than resuming stuck at the end. state.progress is in [0, 1].
    auto durationUs = state.timeline->duration();
    int64_t targetUs = state.timeline->currentTime() >= durationUs
                           ? 0
                           : static_cast<int64_t>(state.progress * static_cast<double>(durationUs));
    state.timeline->setCurrentTime(targetUs);
    state.timeline->play();
  }
  auto now = tgfx::Clock::Now();
  if (lastAdvanceTimeUs < 0) {
    lastAdvanceTimeUs = now;
  }
  int64_t deltaUs = now - lastAdvanceTimeUs;
  lastAdvanceTimeUs = now;
  if (deltaUs > 0) {
    state.timeline->advanceAndApply(deltaUs);
    state.scene->advanceAndApply(deltaUs);
  }
  // Report the current playback progress back to the main thread for UI updates.
  auto durationUs = state.timeline->duration();
  auto currentUs = state.timeline->currentTime();
  auto renderProgress = static_cast<double>(currentUs) / static_cast<double>(durationUs);
  if (renderProgress < 0.0) {
    renderProgress = 0.0;
  }
  if (renderProgress > 1.0) {
    renderProgress = 1.0;
  }
  viewModel->updateProgressFromRender(renderProgress, state.generation);
  // A non-looping animation clears its playing flag once it reaches the end. Notify the main
  // thread so it flips the UI back to the paused state and stops requesting further renders.
  if (!state.timeline->isPlaying() && !playbackFinishNotified) {
    playbackFinishNotified = true;
    viewModel->notifyPlaybackFinished(state.generation);
  }
}

void PAGXRenderer::seekPaused(PAGXViewModel::RenderState& state, bool playStateChanged) {
  auto durationUs = state.timeline->duration();
  if (playStateChanged) {
    // Just paused: the timeline already sits at the true playback position, which leads the main
    // thread's progress snapshot by up to one queued frame. Keep the timeline where it is and
    // report its actual position so the UI catches up, rather than seeking back to the stale
    // progress (which would visibly jump the frame backward).
    auto currentUs = state.timeline->currentTime();
    lastSeekTimeUs = currentUs;
    auto pausedProgress = static_cast<double>(currentUs) / static_cast<double>(durationUs);
    if (pausedProgress < 0.0) {
      pausedProgress = 0.0;
    }
    if (pausedProgress > 1.0) {
      pausedProgress = 1.0;
    }
    viewModel->updateProgressFromRender(pausedProgress, state.generation);
  } else {
    // Steady-state paused (e.g. slider scrubbing): seek to the requested progress position.
    auto targetUs = static_cast<int64_t>(state.progress * static_cast<double>(durationUs));
    if (lastSeekTimeUs < 0 || std::abs(targetUs - lastSeekTimeUs) >= SeekThresholdUs) {
      lastSeekTimeUs = targetUs;
      state.timeline->setCurrentTime(targetUs);
      state.timeline->apply();
    }
  }
}

int64_t PAGXRenderer::computeProfileFrame(const PAGXViewModel::RenderState& state) const {
  if (state.timeline == nullptr || state.timeline->duration() <= 0) {
    return -1;
  }
  auto rate = state.timeline->frameRate();
  if (rate <= 0) {
    return -1;
  }
  auto durationUs = state.timeline->duration();
  auto currentUs = state.timeline->currentTime();
  auto totalFrames =
      static_cast<int64_t>(std::round(static_cast<double>(durationUs) * rate / 1000000.0));
  auto frame = static_cast<int64_t>(static_cast<double>(currentUs) * rate / 1000000.0);
  // Clamp to the last valid frame index so an animation ending exactly on a frame boundary
  // (integer fps) still records the final frame instead of overflowing totalFrame.
  if (totalFrames > 0 && frame >= totalFrames) {
    frame = totalFrames - 1;
  }
  return frame;
}

void PAGXRenderer::applyFitTransform(PAGXViewModel::RenderState& state, int surfaceWidth,
                                     int surfaceHeight) {
  float scaleX = static_cast<float>(surfaceWidth) / static_cast<float>(state.contentWidth);
  float scaleY = static_cast<float>(surfaceHeight) / static_cast<float>(state.contentHeight);
  float scale = std::min(scaleX, scaleY);
  float offsetX =
      (static_cast<float>(surfaceWidth) - static_cast<float>(state.contentWidth) * scale) * 0.5f;
  float offsetY =
      (static_cast<float>(surfaceHeight) - static_cast<float>(state.contentHeight) * scale) * 0.5f;
  state.scene->getDisplayOptions()->setZoomScale(scale);
  state.scene->getDisplayOptions()->setContentOffset(offsetX, offsetY);
}

}  // namespace pag
