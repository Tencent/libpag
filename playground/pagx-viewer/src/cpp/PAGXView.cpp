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
#include "pagx/types/Data.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Typeface.h"
#include "utils/ImagePatternMatrixCalculator.h"

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
  defaultTimeline = nullptr;
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
  // TODO: Remove ResolveAllImagePatternMatrices() and ResolveAllGradientCoordinates() after the
  // pagx exporter adapts to relative coordinates for image and gradient fills. Currently we force
  // them to absolute coordinates to ensure correct rendering.
  ResolveAllImagePatternMatrices(document.get());
  ResolveAllGradientCoordinates(document.get());
  document->applyLayout(&fontConfig);
  scene = PAGScene::Make(document);
  if (scene == nullptr) {
    return;
  }
  defaultTimeline = scene->getDefaultTimeline();
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
  if (defaultTimeline != nullptr) {
    defaultTimeline->advanceAndApply(deltaUs);
  }
  if (scene != nullptr) {
    scene->advanceAndApply(deltaUs);
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
  if (pagSurface != nullptr && lastSurfaceWidth == canvasWidth &&
      lastSurfaceHeight == canvasHeight) {
    return;
  }
  auto device = window->getDevice();
  auto context = device->lockContext();
  if (context == nullptr) {
    return;
  }
  pag::GLFrameBufferInfo frameBufferInfo = {};
  frameBufferInfo.id = 0;
  frameBufferInfo.format = GL_RGBA8;
  pag::BackendRenderTarget renderTarget(frameBufferInfo, canvasWidth, canvasHeight);
  pagSurface = PAGSurface::MakeFrom(renderTarget, pag::ImageOrigin::BottomLeft);
  device->unlock();
  if (pagSurface == nullptr) {
    return;
  }
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
  options->setRenderMode(PAGRenderMode::Tiled);
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
  if (pagSurface == nullptr) {
    return;
  }
  if (useCustomBackgroundColor) {
    scene->getDisplayOptions()->setBackgroundColor(customBackgroundColor);
  } else {
    scene->getDisplayOptions()->setBackgroundColor({});
  }
  scene->draw(pagSurface, true);
  auto device = window->getDevice();
  auto context = device->lockContext();
  if (context != nullptr) {
    auto recording = context->flush();
    if (presentImmediately) {
      presentImmediately = false;
      lastRecording = nullptr;
      if (recording) {
        context->submit(std::move(recording));
      }
    } else if (lastRecording) {
      context->submit(std::move(lastRecording));
      lastRecording = std::move(recording);
    } else {
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

}  // namespace pagx
