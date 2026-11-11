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

#include "platform/qt/GPUDrawable.h"
#include "rendering/IContentRenderer.h"
#include "rendering/pagx/PAGXViewModel.h"

namespace pag {

/**
 * Renders a PAGX scene via pagx::Record() and drives its timeline with wall-clock deltas.
 */
class PAGXRenderer : public IContentRenderer {
 public:
  explicit PAGXRenderer(PAGXViewModel* viewModel);

  void setDrawable(GPUDrawable* drawable) override;

  RenderMetrics flush() override;
  void updateSize() override;
  bool isReady() const override;

 private:
  // Advances the default timeline while playing. Animations are seeked/reseated through the
  // wall-clock delta since the last flush, with progress reported back to the main thread and a
  // playback-finished notification for non-looping ones; timelines without a time base are handed
  // over to advanceTimeline(). reseat is true on the flush that first observes a paused→playing
  // transition or a content reload (generation bump), which re-arms the animation (seek to the
  // shown progress or rewind a finished one, then play).
  void advancePlayback(PAGXViewModel::RenderState& state, bool reseat);

  // Advances a timeline that has no seekable time base (a state machine) together with the scene,
  // which also carries auto-playing nested compositions.
  void advanceTimeline(PAGXViewModel::RenderState& state);

  // Handles the paused timeline: on a play-state transition or content reload it reports the
  // timeline's actual position so the UI catches up; in steady state it seeks to the requested
  // progress (slider scrubbing).
  void seekPaused(PAGXViewModel::RenderState& state, bool reseat);

  // Returns the current frame index for the profiling histogram, clamped to the last valid frame.
  // Returns -1 when no frame applies (no timeline, zero duration, or non-positive frame rate).
  int64_t computeProfileFrame(const PAGXViewModel::RenderState& state) const;

  // Applies the combined fit-to-screen and user zoom/pan transform to the scene's display options.
  void applyDisplayTransform(PAGXViewModel::RenderState& state, int surfaceWidth,
                             int surfaceHeight);

  PAGXViewModel* viewModel = nullptr;
  GPUDrawable* drawable = nullptr;
  // Last wall-clock time (microseconds) when the animation was advanced.
  // Set to -1 on pause/stop so the first advance after resume starts from zero delta.
  int64_t lastAdvanceTimeUs = -1;
  // Last seek position in microseconds, used to avoid redundant seeks when progress is unchanged.
  // Set to -1 on play-state transition so the first paused frame after a transition always seeks.
  int64_t lastSeekTimeUs = -1;
  // Tracks the playback state from the previous flush to detect transitions.
  bool lastIsPlaying = false;
  // Tracks the playback generation from the previous flush to detect content reloads, so a freshly
  // loaded scene re-seats to its start instead of inheriting the previous file's advance timers.
  uint64_t lastGeneration = 0;
  // Guards against posting duplicate finish notifications every frame after a non-looping
  // animation ends, until the main thread flips the playing state off.
  bool playbackFinishNotified = false;
};

}  // namespace pag
