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

  // Reset the advance/seek trackers on a play-state transition OR a content reload (generation
  // bump), so a paused→resumed cycle does not count pause time as animation delta, and a freshly
  // loaded scene always re-seats to its start instead of inheriting the previous file's timers.
  bool playStateChanged = state.isPlaying != lastIsPlaying;
  bool generationChanged = state.generation != lastGeneration;
  bool reseat = playStateChanged || generationChanged;
  if (reseat) {
    lastIsPlaying = state.isPlaying;
    lastGeneration = state.generation;
    lastAdvanceTimeUs = -1;
    lastSeekTimeUs = -1;
    playbackFinishNotified = false;
  }

  if (state.isPlaying && state.animation != nullptr && state.animation->duration() > 0) {
    advancePlayback(state, reseat);
  } else if (state.animation != nullptr && state.animation->duration() > 0) {
    seekPaused(state, reseat);
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
  if (!recording) {
    device->unlock();
    return metrics;
  }
  context->submit(std::move(recording));
  metrics.renderTime = tgfx::Clock::Now() - renderStart;
  // PAGX rendering via pagx::Record has no distinct image-decode phase, so imageDecodeTime stays 0.

  auto presentStart = tgfx::Clock::Now();
  drawable->present(context);
  metrics.presentTime = tgfx::Clock::Now() - presentStart;

  metrics.rendered = true;
  device->unlock();
  return metrics;
}

void PAGXRenderer::advancePlayback(PAGXViewModel::RenderState& state, bool reseat) {
  // On a fresh play request or content reload, re-seat the animation at the requested position.
  // PAGAnimation holds no play/pause state of its own (playback is gated by the owning
  // PAGComposition's pausedTimelineIds), so replaying only requires seeking to the target time and
  // applying it; the per-frame advanceAndApply below then drives it forward.
  if (reseat) {
    // Seek to the requested progress so playback starts from the position the UI shows,
    // regardless of where the timeline was left. A finished non-looping (Once) animation is
    // rewound to the start: the replay path (play button) leaves progress at ~1.0, and the user
    // expects a restart rather than resuming stuck at the end. Looping animations (Loop/PingPong)
    // are never rewound here because PingPong can legitimately sit at currentTime == duration at
    // the mirror peak; they always resume from their reported progress. state.progress is in [0,1].
    auto durationUs = state.animation->duration();
    int64_t targetUs =
        state.loopMode == pagx::LoopMode::Once && state.animation->currentTime() >= durationUs
            ? 0
            : static_cast<int64_t>(state.progress * static_cast<double>(durationUs));
    state.animation->setCurrentTime(targetUs);
    state.animation->apply();
  }
  auto now = tgfx::Clock::Now();
  if (lastAdvanceTimeUs < 0) {
    lastAdvanceTimeUs = now;
  }
  int64_t deltaUs = now - lastAdvanceTimeUs;
  lastAdvanceTimeUs = now;
  if (deltaUs > 0) {
    state.animation->advanceAndApply(deltaUs);
    state.scene->advanceAndApply(deltaUs);
  }
  // Report the current playback progress back to the main thread for UI updates.
  auto durationUs = state.animation->duration();
  auto currentUs = state.animation->currentTime();
  auto renderProgress = static_cast<double>(currentUs) / static_cast<double>(durationUs);
  if (renderProgress < 0.0) {
    renderProgress = 0.0;
  }
  if (renderProgress > 1.0) {
    renderProgress = 1.0;
  }
  viewModel->updateProgressFromRender(renderProgress, state.generation);
  // A non-looping animation stays at the last frame once it reaches the end. Notify the main
  // thread so it flips the UI back to the paused state and stops requesting further renders.
  // Looping modes (Loop/PingPong) never report finished: Loop keeps currentTime in [0, duration),
  // while PingPong can momentarily land exactly on duration at the mirror peak, which must NOT be
  // treated as finished.
  if (state.loopMode == pagx::LoopMode::Once &&
      state.animation->currentTime() >= state.animation->duration() && !playbackFinishNotified) {
    playbackFinishNotified = true;
    viewModel->notifyPlaybackFinished(state.generation);
  }
}

void PAGXRenderer::seekPaused(PAGXViewModel::RenderState& state, bool reseat) {
  auto durationUs = state.animation->duration();
  if (reseat) {
    if (state.seekRequested) {
      // An explicit seek was requested alongside the pause (frame-step controls or slider while
      // transitioning). Honor the target progress instead of reporting the timeline's actual
      // position, which would overwrite the user's intent.
      auto targetUs = static_cast<int64_t>(state.progress * static_cast<double>(durationUs));
      lastSeekTimeUs = targetUs;
      state.animation->setCurrentTime(targetUs);
      state.animation->apply();
    } else {
      // Pure pause (play button): the timeline already sits at the true playback position, which
      // leads the main thread's progress snapshot by up to one queued frame. Keep the timeline
      // where it is and report its actual position so the UI catches up.
      auto currentUs = state.animation->currentTime();
      lastSeekTimeUs = currentUs;
      auto pausedProgress = static_cast<double>(currentUs) / static_cast<double>(durationUs);
      if (pausedProgress < 0.0) {
        pausedProgress = 0.0;
      }
      if (pausedProgress > 1.0) {
        pausedProgress = 1.0;
      }
      viewModel->updateProgressFromRender(pausedProgress, state.generation);
    }
  } else {
    // Steady-state paused (e.g. slider scrubbing): seek to the requested progress position.
    auto targetUs = static_cast<int64_t>(state.progress * static_cast<double>(durationUs));
    if (lastSeekTimeUs < 0 || std::abs(targetUs - lastSeekTimeUs) >= SeekThresholdUs) {
      lastSeekTimeUs = targetUs;
      state.animation->setCurrentTime(targetUs);
      state.animation->apply();
    }
  }
}

int64_t PAGXRenderer::computeProfileFrame(const PAGXViewModel::RenderState& state) const {
  if (state.animation == nullptr || state.animation->duration() <= 0) {
    return -1;
  }
  auto rate = state.animation->frameRate();
  if (rate <= 0) {
    return -1;
  }
  auto durationUs = state.animation->duration();
  auto currentUs = state.animation->currentTime();
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
