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

#include "PAGXView.h"
#include <GLES3/gl3.h>
#include <emscripten/html5.h>
#include <algorithm>
#include <cstdint>
#include "pagx/PAGXImporter.h"
#include "pagx/tgfx.h"
#include "pagx/types/Data.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Surface.h"
#include "tgfx/core/Typeface.h"
#include "tgfx/gpu/opengl/webgl/WebGLWindow.h"

using namespace emscripten;

namespace pagx {

// The frame duration threshold in milliseconds above which a frame is considered slow.
static constexpr double SlowFrameThresholdMs = 32.0;
// The time window in milliseconds for averaging frame durations to detect performance recovery.
static constexpr double RecoveryWindowMs = 2000.0;
// The timeout in milliseconds to detect the end of a zoom-in gesture.
static constexpr double ZoomInEndTimeoutMs = 300.0;
// The timeout in milliseconds to detect the end of a zoom-out gesture.
static constexpr double ZoomOutEndTimeoutMs = 800.0;
// The delay in milliseconds before retrying a tile refinement upgrade after zoom ends.
static constexpr double UpgradeRetryDelayMs = 300.0;
// The initial delay in milliseconds before upgrading tile refinement after zoom ends.
static constexpr double InitialUpgradeDelayMs = 200.0;
// The minimum number of normal frames required to recover from slow state in static mode.
static constexpr size_t MinRecoveryFramesStatic = 20;
// The minimum number of normal frames required to recover from slow state after zoom ends.
static constexpr size_t MinRecoveryFramesZoomEnd = 10;

static uint8_t* CopyFromEmscripten(const val& emscriptenData, unsigned int* outLength) {
  if (emscriptenData.isUndefined()) {
    return nullptr;
  }
  unsigned int length = emscriptenData["length"].as<unsigned int>();
  if (length == 0) {
    return nullptr;
  }
  auto buffer = new (std::nothrow) uint8_t[length];
  if (!buffer) {
    return nullptr;
  }
  auto memory = val::module_property("HEAPU8")["buffer"];
  auto memoryView = emscriptenData["constructor"].new_(
      memory, static_cast<unsigned int>(reinterpret_cast<uintptr_t>(buffer)), length);
  memoryView.call<void>("set", emscriptenData);
  *outLength = length;
  return buffer;
}

static std::shared_ptr<tgfx::Data> GetTGFXDataFromEmscripten(const val& emscriptenData) {
  unsigned int length = 0;
  auto buffer = CopyFromEmscripten(emscriptenData, &length);
  if (!buffer) {
    return nullptr;
  }
  return tgfx::Data::MakeAdopted(buffer, length, tgfx::Data::DeleteProc);
}

static std::shared_ptr<Data> GetPagxDataFromEmscripten(const val& emscriptenData) {
  unsigned int length = 0;
  auto buffer = CopyFromEmscripten(emscriptenData, &length);
  if (!buffer) {
    return nullptr;
  }
  return Data::MakeAdopt(buffer, length);
}

PAGXView::PAGXView(const std::string& canvasID) : canvasID(canvasID) {
}

void PAGXView::registerFonts(const val& fontVal, const val& emojiFontVal) {
  auto fontData = GetTGFXDataFromEmscripten(fontVal);
  if (fontData) {
    fontConfig.addFallbackFont(fontData->data(), fontData->size(), 0);
  }
  auto emojiFontData = GetTGFXDataFromEmscripten(emojiFontVal);
  if (emojiFontData) {
    fontConfig.addFallbackFont(emojiFontData->data(), emojiFontData->size(), 0);
  }
}

void PAGXView::loadPAGX(const val& pagxData) {
  parsePAGX(pagxData);
  buildLayers();
}

void PAGXView::parsePAGX(const val& pagxData) {
  document = nullptr;
  scene = nullptr;
  timelines.clear();
  defaultTimeline = nullptr;
  defaultAnimation = nullptr;
  lastRecording = nullptr;
  lastAnimationTimeMs = -1.0;
  auto data = GetPagxDataFromEmscripten(pagxData);
  if (!data) {
    return;
  }
  document = PAGXImporter::FromXML(data->bytes(), data->size());
}

std::vector<std::string> PAGXView::getExternalFilePaths() const {
  if (!document) {
    return {};
  }
  return document->getExternalFilePaths();
}

bool PAGXView::loadFileData(const std::string& filePath, const val& fileData) {
  if (!document) {
    return false;
  }
  auto data = GetPagxDataFromEmscripten(fileData);
  if (!data) {
    return false;
  }
  return document->loadFileData(filePath, std::move(data));
}

void PAGXView::buildLayers() {
  if (!document) {
    return;
  }
  document->applyLayout(&fontConfig);
  scene = PAGScene::Make(document);
  if (scene == nullptr) {
    return;
  }
  defaultTimeline = scene->getDefaultTimeline();
  defaultAnimation = nullptr;
  if (defaultTimeline != nullptr && defaultTimeline->type() == TimelineType::Animation) {
    defaultAnimation = std::static_pointer_cast<PAGAnimation>(defaultTimeline);
  }
  // Collect every top-level animation other than the default one. A PAGX document (notably an
  // HTML-imported one) may declare several independent animations (e.g. one per animated element);
  // the default animation is driven with play/loop gating below, while these extras are advanced
  // alongside it so all animated elements play instead of only the first.
  timelines.clear();
  for (const auto& id : scene->getAnimationIds()) {
    auto animation = scene->getAnimation(id);
    if (animation != nullptr && animation != defaultAnimation) {
      timelines.push_back(std::move(animation));
    }
  }
  playing = true;
  lastAnimationTimeMs = -1.0;
  pagxWidth = scene->width();
  pagxHeight = scene->height();
  applySceneDisplayOptions();
  updateContentTransform();
  presentImmediately = true;
}

void PAGXView::advanceTimelines(double frameStartMs) {
  int64_t deltaUs = 0;
  if (lastAnimationTimeMs >= 0.0) {
    deltaUs = static_cast<int64_t>(std::max(0.0, frameStartMs - lastAnimationTimeMs) * 1000.0);
  }
  lastAnimationTimeMs = frameStartMs;
  if (deltaUs <= 0) {
    return;
  }
  if (playing) {
    if (defaultAnimation != nullptr) {
      int64_t duration = defaultAnimation->duration();
      int64_t before = defaultAnimation->currentTime();
      // Let the engine advance and wrap according to the file's loop mode; this keeps the in-cycle
      // motion intact, including PingPong mirroring. The view only overrides what happens at a cycle
      // boundary based on loopEnabled, so the file's loop flag never dictates repeat vs stop.
      bool changed = defaultAnimation->advanceAndApply(deltaUs);
      int64_t after = defaultAnimation->currentTime();
      if (!changed) {
        // A Once file clamps at the last frame and stops changing. Rewind to the head either way;
        // when looping keep playing for the next cycle, otherwise park on the first frame so a
        // finished single pass resets to the start instead of freezing on the last frame.
        if (duration > 0) {
          defaultAnimation->setCurrentTime(0);
          defaultAnimation->apply();
        }
        if (!loopEnabled) {
          playing = false;
        }
      } else if (!loopEnabled && duration > 0 && after < before) {
        // A Loop/PingPong file wrapped or reversed past the end while the user wants a single pass.
        // Forward motion is monotonic until the boundary, so the first backward step marks one
        // completed pass: rewind to the first frame and stop there. For PingPong this counts the
        // forward half as the single pass and stops as soon as it reaches the end, so the mirrored
        // return half is not played — "single pass" is inherently ambiguous for PingPong and this
        // is the accepted tradeoff.
        defaultAnimation->setCurrentTime(0);
        defaultAnimation->apply();
        playing = false;
      }
    } else if (defaultTimeline != nullptr) {
      // Non-animation timelines (state machines) have no seekable duration to gate; drive as-is.
      defaultTimeline->advanceAndApply(deltaUs);
    }
    // Drive the remaining top-level animations alongside the default so multi-animation documents
    // (e.g. HTML imports) play every animated element. These follow the same play gate but are not
    // subject to the single-pass loop gating above, which only governs the default animation's
    // progress-bar semantics.
    for (const auto& timeline : timelines) {
      if (timeline != nullptr) {
        timeline->advanceAndApply(deltaUs);
      }
    }
    // Drive the scene inside the playing gate so pausing freezes the whole picture: this advances
    // the auto-playing nested compositions, which would otherwise keep animating (and keep
    // hasContentChanged() true) even while the top-level animation is paused.
    if (scene != nullptr) {
      scene->advanceAndApply(deltaUs);
    }
  }
}

void PAGXView::updateSize() {
  if (!ensureWindow()) {
    return;
  }
  int canvasWidth = 0;
  int canvasHeight = 0;
  emscripten_get_canvas_element_size(canvasID.c_str(), &canvasWidth, &canvasHeight);
  syncSurfaceSize(canvasWidth, canvasHeight);
}

bool PAGXView::ensureWindow() {
  if (window == nullptr) {
    window = tgfx::WebGLWindow::MakeFrom(canvasID);
  }
  return window != nullptr && window->getDevice() != nullptr;
}

void PAGXView::syncSurfaceSize(int canvasWidth, int canvasHeight) {
  if (!ensureWindow() || canvasWidth <= 0 || canvasHeight <= 0) {
    return;
  }
  if (tgfxSurface != nullptr && lastSurfaceWidth == canvasWidth &&
      lastSurfaceHeight == canvasHeight) {
    return;
  }
  auto device = window->getDevice();
  auto context = device->lockContext();
  if (context == nullptr) {
    return;
  }
  tgfxSurface = tgfx::Surface::MakeFrom(context, window);
  device->unlock();
  if (tgfxSurface == nullptr) {
    return;
  }
  pagSurface = pagx::MakeFrom(tgfxSurface);
  lastSurfaceWidth = canvasWidth;
  lastSurfaceHeight = canvasHeight;
  updateContentTransform();
  presentImmediately = true;
}

void PAGXView::updateContentTransform() {
  if (lastSurfaceWidth <= 0 || lastSurfaceHeight <= 0) {
    return;
  }
  if (pagxWidth <= 0 || pagxHeight <= 0) {
    return;
  }
  float scaleX = static_cast<float>(lastSurfaceWidth) / pagxWidth;
  float scaleY = static_cast<float>(lastSurfaceHeight) / pagxHeight;
  contentScale = std::min(scaleX, scaleY);
  contentOffsetX = (static_cast<float>(lastSurfaceWidth) - pagxWidth * contentScale) * 0.5f;
  contentOffsetY = (static_cast<float>(lastSurfaceHeight) - pagxHeight * contentScale) * 0.5f;
  applyDisplayTransform();
}

void PAGXView::applyDisplayTransform() {
  if (scene == nullptr) {
    return;
  }
  scene->getDisplayOptions()->setZoomScale(contentScale * userZoom);
  scene->getDisplayOptions()->setContentOffset(contentOffsetX * userZoom + userOffsetX,
                                               contentOffsetY * userZoom + userOffsetY);
}

void PAGXView::applySceneDisplayOptions() {
  if (scene == nullptr) {
    return;
  }
  auto options = scene->getDisplayOptions();
  options->setRenderMode(PAGRenderMode::Partial);
  options->setTileUpdateMode(PAGTileUpdateMode::Smooth);
  options->setMaxTileCount(512);
  options->setMaxTilesRefinedPerFrame(currentMaxTilesRefinedPerFrame);
}

void PAGXView::updateZoomScaleAndOffset(float zoom, float offsetX, float offsetY) {
  if (scene != nullptr) {
    if (zoom <= 1.0f) {
      scene->getDisplayOptions()->setSubtreeCacheMaxSize(1024);
    } else {
      scene->getDisplayOptions()->setSubtreeCacheMaxSize(0);
    }
  }

  bool zoomChanged = (std::abs(zoom - lastZoom) > 0.001f);
  if (zoomChanged) {
    if (!isZooming) {
      isZooming = true;
      accumulatedZoomChange = 0.0f;
      updateAdaptiveTileRefinement();
    }
    float currentChange = zoom - lastZoom;
    accumulatedZoomChange += currentChange;
    if (std::abs(accumulatedZoomChange) > 0.01f) {
      isZoomingIn = (accumulatedZoomChange > 0.0f);
    }
    lastZoomUpdateTimestampMs = emscripten_get_now();
  }

  userZoom = zoom;
  userOffsetX = offsetX;
  userOffsetY = offsetY;
  applyDisplayTransform();
  presentImmediately = true;
  lastZoom = zoom;
}

void PAGXView::setBackgroundColor(float red, float green, float blue, float alpha) {
  useCustomBackgroundColor = true;
  customBackgroundColor = {std::clamp(red, 0.0f, 1.0f), std::clamp(green, 0.0f, 1.0f),
                           std::clamp(blue, 0.0f, 1.0f), std::clamp(alpha, 0.0f, 1.0f)};
  if (scene != nullptr) {
    scene->getDisplayOptions()->setBackgroundColor(customBackgroundColor);
  }
  presentImmediately = true;
}

void PAGXView::clearBackgroundColor() {
  useCustomBackgroundColor = false;
  customBackgroundColor = {};
  if (scene != nullptr) {
    scene->getDisplayOptions()->setBackgroundColor(customBackgroundColor);
  }
  presentImmediately = true;
}

void PAGXView::draw() {
  if (!ensureWindow() || scene == nullptr) {
    return;
  }
  double frameStartMs = emscripten_get_now();
  advanceTimelines(frameStartMs);
  int currentCanvasWidth = 0;
  int currentCanvasHeight = 0;
  emscripten_get_canvas_element_size(canvasID.c_str(), &currentCanvasWidth, &currentCanvasHeight);
  syncSurfaceSize(currentCanvasWidth, currentCanvasHeight);
  if (tgfxSurface == nullptr) {
    return;
  }
  // Dirty gate: skip the Record()/submit pass on idle frames. advanceTimelines() above already
  // refreshed the scene's content-changed flag, so hasContentChanged() reflects the latest state
  // (a running animation or in-progress tile refinement keeps it true). presentImmediately forces a
  // render after loads/resizes/zoom/background changes; lastRecording must still be flushed when
  // present, otherwise the double-buffered frame it holds would be dropped.
  if (!presentImmediately && lastRecording == nullptr && !scene->hasContentChanged()) {
    return;
  }
  if (useCustomBackgroundColor) {
    scene->getDisplayOptions()->setBackgroundColor(customBackgroundColor);
  } else {
    scene->getDisplayOptions()->setBackgroundColor({});
  }
  auto device = window->getDevice();
  auto context = device->lockContext();
  if (context != nullptr) {
    auto recording = pagx::Record(context, scene, pagSurface, true);
    if (presentImmediately) {
      // Force the freshest frame on screen right now. Drop whatever frame is still parked in
      // lastRecording so the deferred (older) content cannot resurface one frame later.
      presentImmediately = false;
      lastRecording = nullptr;
      if (recording) {
        context->submit(std::move(recording));
      }
    } else {
      // Double buffer: park this frame's recording and submit the one deferred from the previous
      // frame, giving the GPU an extra frame to finish. When the scene content is unchanged,
      // Record() returns null; swapping that null into lastRecording lets the dirty gate resume
      // skipping idle frames once the last deferred frame has been flushed out.
      std::swap(lastRecording, recording);
      if (recording) {
        context->submit(std::move(recording));
      }
    }
    device->unlock();
  }

  double frameEndMs = emscripten_get_now();
  double frameDurationMs = frameEndMs - frameStartMs;
  updatePerformanceState(frameDurationMs);

  if (isZooming && lastZoomUpdateTimestampMs > 0.0) {
    double currentTimeoutMs = isZoomingIn ? ZoomInEndTimeoutMs : ZoomOutEndTimeoutMs;
    double timeSinceLastUpdate = frameStartMs - lastZoomUpdateTimestampMs;
    if (timeSinceLastUpdate >= currentTimeoutMs) {
      onZoomEnd();
    }
  }

  if (!isZooming && tryUpgradeTimestampMs > 0.0) {
    if (frameStartMs >= tryUpgradeTimestampMs) {
      if (!lastFrameSlow) {
        int targetCount = calculateTargetTileRefinement(lastZoom);
        currentMaxTilesRefinedPerFrame = targetCount;
        if (scene != nullptr) {
          scene->getDisplayOptions()->setMaxTilesRefinedPerFrame(targetCount);
        }
        tryUpgradeTimestampMs = 0.0;
      } else {
        tryUpgradeTimestampMs = frameStartMs + UpgradeRetryDelayMs;
      }
    }
  } else if (!isZooming) {
    updateAdaptiveTileRefinement();
  }
}

void PAGXView::onZoomEnd() {
  if (!isZooming) {
    return;
  }
  isZooming = false;
  currentMaxTilesRefinedPerFrame = 1;
  if (scene != nullptr) {
    scene->getDisplayOptions()->setMaxTilesRefinedPerFrame(currentMaxTilesRefinedPerFrame);
  }
  tryUpgradeTimestampMs = emscripten_get_now() + InitialUpgradeDelayMs;
}

void PAGXView::updatePerformanceState(double frameDurationMs) {
  double now = emscripten_get_now();
  if (frameDurationMs > SlowFrameThresholdMs) {
    if (!lastFrameSlow) {
      frameHistory.clear();
      frameHistoryTotalTime = 0.0;
    }
    lastFrameSlow = true;
  }
  frameHistory.push_back({now, frameDurationMs});
  frameHistoryTotalTime += frameDurationMs;
  double windowStart = now - RecoveryWindowMs;
  while (!frameHistory.empty() && frameHistory.front().timestampMs < windowStart) {
    frameHistoryTotalTime -= frameHistory.front().durationMs;
    frameHistory.pop_front();
  }
  if (lastFrameSlow && !frameHistory.empty()) {
    double avgTime = frameHistoryTotalTime / static_cast<double>(frameHistory.size());
    size_t minFrames = isZooming ? MinRecoveryFramesZoomEnd : MinRecoveryFramesStatic;
    if (avgTime <= SlowFrameThresholdMs && frameHistory.size() >= minFrames) {
      lastFrameSlow = false;
    }
  }
}

int PAGXView::calculateTargetTileRefinement(float zoom) const {
  if (isZooming) {
    return 0;
  }
  if (lastFrameSlow) {
    return 1;
  }
  if (zoom < 1.0f) {
    int count = static_cast<int>(zoom / 0.33f) + 1;
    return std::clamp(count, 1, 3);
  }
  return 3;
}

void PAGXView::updateAdaptiveTileRefinement() {
  int targetCount = calculateTargetTileRefinement(lastZoom);
  if (targetCount != currentMaxTilesRefinedPerFrame) {
    currentMaxTilesRefinedPerFrame = targetCount;
    if (scene != nullptr) {
      scene->getDisplayOptions()->setMaxTilesRefinedPerFrame(targetCount);
    }
  }
}

void PAGXView::play() {
  playing = true;
}

void PAGXView::pause() {
  playing = false;
}

bool PAGXView::isPlaying() const {
  return playing;
}

int64_t PAGXView::currentTimeMicros() const {
  if (defaultAnimation != nullptr) {
    return defaultAnimation->currentTime();
  }
  return 0;
}

int64_t PAGXView::durationMicros() const {
  if (defaultAnimation != nullptr) {
    return defaultAnimation->duration();
  }
  return 0;
}

float PAGXView::frameRate() const {
  if (defaultAnimation != nullptr) {
    return defaultAnimation->frameRate();
  }
  return 0.0f;
}

// Known limitation: seek only repositions the default (top-level) animation. Nested
// auto-playing compositions rendered by `scene` are delta-driven via advanceAndApply() and have
// no absolute-time entry point, so their frame lags behind the main timeline after a scrub:
// the main animation jumps to `micros` while the nested compositions stay wherever their
// accumulated delta left them. Acceptable for the MVP viewer; a future fix would need per-scene
// seek support (or a full scene rebuild) rather than a workaround here.
void PAGXView::setCurrentTimeMicros(int64_t micros) {
  if (defaultAnimation != nullptr) {
    defaultAnimation->setCurrentTime(micros);
    // setCurrentTime only moves the playhead; apply() is required to reflect it in the content.
    // Force a present so a manual seek (e.g. dragging the progress bar while paused) updates the
    // frame immediately instead of being skipped by the idle dirty gate in draw().
    defaultAnimation->apply();
    lastAnimationTimeMs = -1.0;
    presentImmediately = true;
  }
}

void PAGXView::setLoop(bool loop) {
  loopEnabled = loop;
}

bool PAGXView::isLoop() const {
  return loopEnabled;
}

}  // namespace pagx
