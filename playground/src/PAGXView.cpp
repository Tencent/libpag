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
#include <emscripten/html5.h>
#include "pagx/PAGXImporter.h"
#include "pagx/types/Data.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Typeface.h"

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
  displayList.setRenderMode(tgfx::RenderMode::Tiled);
  displayList.setAllowZoomBlur(true);
  displayList.setMaxTileCount(512);
  displayList.setMaxTilesRefinedPerFrame(currentMaxTilesRefinedPerFrame);
}

void PAGXView::registerFonts(const val& fontVal, const val& emojiFontVal) {
  std::vector<std::shared_ptr<tgfx::Typeface>> fallbackTypefaces;
  auto fontData = GetTGFXDataFromEmscripten(fontVal);
  if (fontData) {
    auto typeface = tgfx::Typeface::MakeFromData(fontData, 0);
    if (typeface) {
      fallbackTypefaces.push_back(std::move(typeface));
    }
  }
  auto emojiFontData = GetTGFXDataFromEmscripten(emojiFontVal);
  if (emojiFontData) {
    auto typeface = tgfx::Typeface::MakeFromData(emojiFontData, 0);
    if (typeface) {
      fallbackTypefaces.push_back(std::move(typeface));
    }
  }
  textLayout.setFallbackTypefaces(std::move(fallbackTypefaces));
}

void PAGXView::loadPAGX(const val& pagxData) {
  parsePAGX(pagxData);
  buildLayers();
}

void PAGXView::parsePAGX(const val& pagxData) {
  document = nullptr;
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
  contentLayer = LayerBuilder::Build(document.get(), &textLayout);
  if (!contentLayer) {
    return;
  }
  pagxWidth = document->width;
  pagxHeight = document->height;
  displayList.root()->removeChildren();
  displayList.root()->addChild(contentLayer);
  applyCenteringTransform();
}

void PAGXView::updateSize() {
  if (window == nullptr) {
    window = tgfx::WebGLWindow::MakeFrom(canvasID);
  }
  if (window == nullptr) {
    return;
  }
  window->invalidSize();
  auto device = window->getDevice();
  auto context = device->lockContext();
  if (context == nullptr) {
    return;
  }
  auto surface = window->getSurface(context);
  if (surface == nullptr) {
    device->unlock();
    return;
  }
  if (surface->width() != lastSurfaceWidth || surface->height() != lastSurfaceHeight) {
    lastSurfaceWidth = surface->width();
    lastSurfaceHeight = surface->height();
    applyCenteringTransform();
    presentImmediately = true;
  }
  device->unlock();
}

void PAGXView::applyCenteringTransform() {
  if (lastSurfaceWidth <= 0 || lastSurfaceHeight <= 0 || !contentLayer) {
    return;
  }
  if (pagxWidth <= 0 || pagxHeight <= 0) {
    return;
  }
  float scaleX = static_cast<float>(lastSurfaceWidth) / pagxWidth;
  float scaleY = static_cast<float>(lastSurfaceHeight) / pagxHeight;
  float scale = std::min(scaleX, scaleY);
  float offsetX = (static_cast<float>(lastSurfaceWidth) - pagxWidth * scale) * 0.5f;
  float offsetY = (static_cast<float>(lastSurfaceHeight) - pagxHeight * scale) * 0.5f;
  auto matrix = tgfx::Matrix::MakeTrans(offsetX, offsetY);
  matrix.preScale(scale, scale);
  contentLayer->setMatrix(matrix);
}

void PAGXView::updateZoomScaleAndOffset(float zoom, float offsetX, float offsetY) {
  if (zoom <= 1.0f) {
    displayList.setSubtreeCacheMaxSize(1024);
  } else {
    displayList.setSubtreeCacheMaxSize(0);
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

  displayList.setZoomScale(zoom);
  displayList.setContentOffset(offsetX, offsetY);
  lastZoom = zoom;
}

void PAGXView::draw() {
  if (window == nullptr) {
    window = tgfx::WebGLWindow::MakeFrom(canvasID);
  }
  if (window == nullptr) {
    return;
  }
  double frameStartMs = emscripten_get_now();
  bool hasContentChanged = displayList.hasContentChanged();
  bool hasLastRecording = (lastRecording != nullptr);
  if (!hasContentChanged && !hasLastRecording) {
    return;
  }
  auto device = window->getDevice();
  auto context = device->lockContext();
  if (context == nullptr) {
    return;
  }
  auto surface = window->getSurface(context);
  if (surface == nullptr) {
    device->unlock();
    return;
  }
  auto canvas = surface->getCanvas();
  canvas->clear();
  int width = 0;
  int height = 0;
  emscripten_get_canvas_element_size(canvasID.c_str(), &width, &height);
  auto density = width > 0 ? static_cast<float>(surface->width()) / static_cast<float>(width) : 1.0f;
  int bgWidth = surface->width();
  int bgHeight = surface->height();
  if (!backgroundLayer || bgWidth != lastBackgroundWidth || bgHeight != lastBackgroundHeight ||
      std::abs(density - lastBackgroundDensity) > 0.001f) {
    backgroundLayer = GridBackgroundLayer::Make(bgWidth, bgHeight, density);
    lastBackgroundWidth = bgWidth;
    lastBackgroundHeight = bgHeight;
    lastBackgroundDensity = density;
  }
  backgroundLayer->draw(canvas);
  displayList.render(surface.get(), false);
  auto recording = context->flush();
  if (presentImmediately) {
    presentImmediately = false;
    if (recording) {
      context->submit(std::move(recording));
      window->present(context);
    }
  } else {
    std::swap(lastRecording, recording);
    if (recording) {
      context->submit(std::move(recording));
      window->present(context);
    }
  }
  device->unlock();

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
        displayList.setMaxTilesRefinedPerFrame(targetCount);
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
  displayList.setMaxTilesRefinedPerFrame(currentMaxTilesRefinedPerFrame);
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
    displayList.setMaxTilesRefinedPerFrame(targetCount);
  }
}

}  // namespace pagx
