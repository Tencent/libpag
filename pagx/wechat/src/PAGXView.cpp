/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making tgfx available.
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
#include <tgfx/gpu/opengl/GLDevice.h>
#include "GridBackground.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Stream.h"
#include "tgfx/core/Typeface.h"
#include "pagx/PAGXImporter.h"

using namespace emscripten;

namespace pagx {

std::shared_ptr<PAGXView> PAGXView::MakeFrom(int width, int height) {
  if (width <= 0 || height <= 0) {
    return nullptr;
  }

  auto device = tgfx::GLDevice::Current();
  if (device == nullptr) {
    return nullptr;
  }

  return std::shared_ptr<PAGXView>(new PAGXView(device, width, height));
}

static std::shared_ptr<tgfx::Data> GetDataFromEmscripten(const val& emscriptenData) {
  if (emscriptenData.isUndefined()) {
    return nullptr;
  }
  unsigned int length = emscriptenData["length"].as<unsigned int>();
  if (length == 0) {
    return nullptr;
  }
  auto buffer = new (std::nothrow) uint8_t[length];
  if (buffer) {
    auto memory = val::module_property("HEAPU8")["buffer"];
    auto memoryView = emscriptenData["constructor"].new_(
        memory, static_cast<unsigned int>(reinterpret_cast<uintptr_t>(buffer)), length);
    memoryView.call<void>("set", emscriptenData);
    return tgfx::Data::MakeAdopted(buffer, length, tgfx::Data::DeleteProc);
  }
  return nullptr;
}

PAGXView::PAGXView(std::shared_ptr<tgfx::Device> device, int width, int height)
: device(device), _width(width), _height(height) {
  displayList.setRenderMode(tgfx::RenderMode::Tiled);
  displayList.setAllowZoomBlur(true);
  displayList.setMaxTileCount(256);
  displayList.setMaxTilesRefinedPerFrame(currentMaxTilesRefinedPerFrame);
}

void PAGXView::loadPAGX(const val& pagxData) {
  auto data = GetDataFromEmscripten(pagxData);
  if (!data) {
    return;
  }
  auto document = PAGXImporter::FromXML(data->bytes(), data->size());
  if (!document) {
    return;
  }
  contentLayer = LayerBuilder::Build(document.get());
  if (!contentLayer) {
    return;
  }
  pagxWidth = document->width;
  pagxHeight = document->height;
  displayList.root()->removeChildren();
  displayList.root()->addChild(contentLayer);
  applyCenteringTransform();
}

void PAGXView::updateSize(int width, int height) {
  if (width <= 0 || height <= 0) {
    return;
  }

  _width = width;
  _height = height;
  surface = nullptr;

  if (contentLayer) {
    applyCenteringTransform();
  }
}

void PAGXView::applyCenteringTransform() {
  if (_width <= 0 || _height <= 0 || !contentLayer) {
    return;
  }
  if (pagxWidth <= 0 || pagxHeight <= 0) {
    return;
  }

  float scaleX = static_cast<float>(_width) / pagxWidth;
  float scaleY = static_cast<float>(_height) / pagxHeight;
  float scale = std::min(scaleX, scaleY);
  float offsetX = (static_cast<float>(_width) - pagxWidth * scale) * 0.5f;
  float offsetY = (static_cast<float>(_height) - pagxHeight * scale) * 0.5f;

  auto matrix = tgfx::Matrix::MakeTrans(offsetX, offsetY);
  matrix.preScale(scale, scale);

  contentLayer->setMatrix(matrix);
}

void PAGXView::updateZoomScaleAndOffset(float zoom, float offsetX, float offsetY) {
  bool zoomChanged = (std::abs(zoom - lastZoom) > 0.001f);

  if (zoomChanged) {
    if (!isZooming) {
      isZooming = true;
      zoomStartValue = lastZoom;
      accumulatedZoomChange = 0.0f;
      updateAdaptiveTileRefinement();
    }
    
    // Accumulate zoom change to determine overall direction
    float currentChange = zoom - lastZoom;
    accumulatedZoomChange += currentChange;
    
    // Determine direction based on accumulated change (ignoring noise < 0.01)
    if (std::abs(accumulatedZoomChange) > 0.01f) {
      isZoomingIn = (accumulatedZoomChange > 0.0f);
    }
    
    lastZoomUpdateTimestampMs = emscripten_get_now();
  }

  displayList.setZoomScale(zoom);
  displayList.setContentOffset(offsetX, offsetY);
  lastZoom = zoom;
}

void PAGXView::onZoomEnd() {
  if (!isZooming) {
    return;
  }

  isZooming = false;

  if (!enablePerformanceAdaptation) {
    return;
  }

  currentMaxTilesRefinedPerFrame = 1;
  displayList.setMaxTilesRefinedPerFrame(currentMaxTilesRefinedPerFrame);

  tryUpgradeTimestampMs = emscripten_get_now() + 200.0;
}

bool PAGXView::draw() {
  if (device == nullptr) {
    return false;
  }

  auto context = device->lockContext();
  if (context == nullptr) {
    return false;
  }

  if (surface == nullptr || surface->width() != _width || surface->height() != _height) {
    tgfx::GLFrameBufferInfo glInfo = {};
    glInfo.id = 0;
    glInfo.format = 0x8058;
    tgfx::BackendRenderTarget renderTarget(glInfo, _width, _height);
    surface = tgfx::Surface::MakeFrom(context, renderTarget, tgfx::ImageOrigin::BottomLeft);
    if (surface == nullptr) {
      device->unlock();
      return false;
    }
  }
  double frameStartMs = emscripten_get_now();

  auto canvas = surface->getCanvas();
  canvas->clear();

  DrawBackground(canvas, _width, _height, 1.0f);

  displayList.render(surface.get(), false);

  auto recording = context->flush();
  if (recording) {
    context->submit(std::move(recording));
  }
  device->unlock();

  double frameEndMs = emscripten_get_now();
  double frameDurationMs = frameEndMs - frameStartMs;

  updatePerformanceState(frameDurationMs);

  if (isZooming && lastZoomUpdateTimestampMs > 0.0) {
    double currentTimeoutMs = isZoomingIn ? zoomInEndTimeoutMs : zoomOutEndTimeoutMs;
    double timeSinceLastUpdate = frameStartMs - lastZoomUpdateTimestampMs;
    
    if (timeSinceLastUpdate >= currentTimeoutMs) {
      onZoomEnd();
    }
  }

  if (!isZooming && enablePerformanceAdaptation) {
    if (tryUpgradeTimestampMs > 0.0) {
      if (frameStartMs >= tryUpgradeTimestampMs) {
        if (!lastFrameSlow) {
          int targetCount = calculateTargetTileRefinement(lastZoom);
          currentMaxTilesRefinedPerFrame = targetCount;
          displayList.setMaxTilesRefinedPerFrame(targetCount);
          tryUpgradeTimestampMs = 0.0;
        } else {
          tryUpgradeTimestampMs = frameStartMs + upgradeRetryDelayMs;
        }
      }
    } else {
      updateAdaptiveTileRefinement();
    }
  }

  return true;
}

int PAGXView::width() const {
  return _width;
}

int PAGXView::height() const {
  return _height;
}

void PAGXView::updatePerformanceState(double frameDurationMs) {
  if (!enablePerformanceAdaptation) {
    return;
  }

  double now = emscripten_get_now();

  if (frameDurationMs > slowFrameThresholdMs) {
    if (!lastFrameSlow) {
      frameHistory.clear();
      frameHistoryTotalTime = 0.0;
    }
    lastFrameSlow = true;
  }

  frameHistory.push_back({now, frameDurationMs});
  frameHistoryTotalTime += frameDurationMs;

  double windowStart = now - recoveryWindowMs;
  while (!frameHistory.empty() && frameHistory.front().timestampMs < windowStart) {
    frameHistoryTotalTime -= frameHistory.front().durationMs;
    frameHistory.pop_front();
  }

  if (lastFrameSlow && !frameHistory.empty()) {
    double avgTime = frameHistoryTotalTime / static_cast<double>(frameHistory.size());
    size_t minFrames = isZooming ? minRecoveryFramesZoomEnd : minRecoveryFramesStatic;
    if (avgTime <= slowFrameThresholdMs && frameHistory.size() >= minFrames) {
      lastFrameSlow = false;
    }
  }
}

int PAGXView::calculateTargetTileRefinement(float zoom) const {
  // Performance-first strategy: disable tile refinement during zooming to reduce stutter
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
  if (!enablePerformanceAdaptation) {
    return;
  }

  int targetCount = calculateTargetTileRefinement(lastZoom);

  if (targetCount != currentMaxTilesRefinedPerFrame) {
    currentMaxTilesRefinedPerFrame = targetCount;
    displayList.setMaxTilesRefinedPerFrame(targetCount);
  }
}

void PAGXView::setPerformanceAdaptationEnabled(bool enabled) {
  enablePerformanceAdaptation = enabled;
  if (!enabled) {
    currentMaxTilesRefinedPerFrame = 3;
    displayList.setMaxTilesRefinedPerFrame(currentMaxTilesRefinedPerFrame);
    tryUpgradeTimestampMs = 0.0;
  }
}

void PAGXView::setSlowFrameThreshold(double thresholdMs) {
  slowFrameThresholdMs = thresholdMs;
}

void PAGXView::setRecoveryWindow(double windowMs) {
  recoveryWindowMs = windowMs;
}

}  // namespace pagx
