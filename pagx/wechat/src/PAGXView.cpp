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
#include <GLES3/gl31.h>
#include <tgfx/gpu/opengl/GLDevice.h>
#include "GridBackground.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Stream.h"
#include "tgfx/core/Typeface.h"
#include "pagx/PAGXImporter.h"

using namespace emscripten;

namespace pagx {

// Maximum cache limit: 1GB
constexpr size_t MAX_CACHE_LIMIT = 1U * 1024 * 1024 * 1024;
// GPU resource expiration in frames. Resources unused beyond this threshold will be released.
constexpr size_t EXPIRATION_FRAMES = 10 * 60;
// Slow frame threshold in milliseconds (more lenient than desktop 32ms due to WeChat environment).
constexpr double SLOW_FRAME_THRESHOLD_MS = 50.0;
// Recovery time window in milliseconds (longer than desktop 2s to reduce jitter).
constexpr double RECOVERY_WINDOW_MS = 3000.0;
// Timeout for detecting zoom-in gesture end (faster refinement).
constexpr double ZOOM_IN_END_TIMEOUT_MS = 300.0;
// Timeout for detecting zoom-out gesture end (slower refinement).
constexpr double ZOOM_OUT_END_TIMEOUT_MS = 800.0;
// Delay before retrying tile refinement upgrade when performance is still slow.
constexpr double UPGRADE_RETRY_DELAY_MS = 300.0;
// Minimum frames to confirm recovery in static state.
constexpr size_t MIN_RECOVERY_FRAMES_STATIC = 20;
// Minimum frames to confirm recovery after zoom ends.
constexpr size_t MIN_RECOVERY_FRAMES_ZOOM_END = 10;

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

// Copies data from a JavaScript Uint8Array into a tgfx::Data object.
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

void PAGXView::registerFonts(const val& fontVal, const val& emojiFontVal) {
  std::vector<std::shared_ptr<tgfx::Typeface>> fallbackTypefaces;
  auto fontData = GetDataFromEmscripten(fontVal);
  if (fontData) {
    auto typeface = tgfx::Typeface::MakeFromData(fontData, 0);
    if (typeface) {
      fallbackTypefaces.push_back(std::move(typeface));
    }
  }
  auto emojiFontData = GetDataFromEmscripten(emojiFontVal);
  if (emojiFontData) {
    auto typeface = tgfx::Typeface::MakeFromData(emojiFontData, 0);
    if (typeface) {
      fallbackTypefaces.push_back(std::move(typeface));
    }
  }
  typesetter.setFallbackTypefaces(std::move(fallbackTypefaces));
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
  contentLayer = LayerBuilder::Build(document.get(), &typesetter);
  if (!contentLayer) {
    return;
  }
  hasRenderedFirstFrame = false;
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
  if (zoom <= 1.0f) {
    displayList.setSubtreeCacheMaxSize(1024);
  } else {
    displayList.setSubtreeCacheMaxSize(0);
  }

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

  currentMaxTilesRefinedPerFrame = 1;
  displayList.setMaxTilesRefinedPerFrame(currentMaxTilesRefinedPerFrame);

  tryUpgradeTimestampMs = emscripten_get_now() + 200.0;
}

bool PAGXView::draw() {
  if (device == nullptr) {
    return false;
  }

  double frameStartMs = emscripten_get_now();

  if (displayList.hasContentChanged()) {
    auto context = device->lockContext();
    if (context == nullptr) {
      return false;
    }

    if (surface == nullptr || surface->width() != _width || surface->height() != _height) {
      context->setCacheLimit(MAX_CACHE_LIMIT);
      context->setResourceExpirationFrames(EXPIRATION_FRAMES);
      tgfx::GLFrameBufferInfo glInfo = {};
      glInfo.id = 0;
      glInfo.format = GL_RGBA8;
      tgfx::BackendRenderTarget renderTarget(glInfo, _width, _height);
      surface = tgfx::Surface::MakeFrom(context, renderTarget, tgfx::ImageOrigin::BottomLeft);
      if (surface == nullptr) {
        device->unlock();
        return false;
      }
    }

    auto canvas = surface->getCanvas();
    canvas->clear();

    DrawBackground(canvas, _width, _height, 1.0f);

    displayList.render(surface.get(), false);

    auto recording = context->flush();
    if (recording) {
      context->submit(std::move(recording));
      if (!hasRenderedFirstFrame) {
        hasRenderedFirstFrame = true;
      }
    }
    device->unlock();
  }

  double frameEndMs = emscripten_get_now();
  double frameDurationMs = frameEndMs - frameStartMs;

  updatePerformanceState(frameDurationMs);

  if (isZooming && lastZoomUpdateTimestampMs > 0.0) {
    double currentTimeoutMs = isZoomingIn ? ZOOM_IN_END_TIMEOUT_MS : ZOOM_OUT_END_TIMEOUT_MS;
    double timeSinceLastUpdate = frameStartMs - lastZoomUpdateTimestampMs;
    
    if (timeSinceLastUpdate >= currentTimeoutMs) {
      onZoomEnd();
    }
  }

  if (!isZooming) {
    if (tryUpgradeTimestampMs > 0.0) {
      if (frameStartMs >= tryUpgradeTimestampMs) {
        if (!lastFrameSlow) {
          int targetCount = calculateTargetTileRefinement(lastZoom);
          currentMaxTilesRefinedPerFrame = targetCount;
          displayList.setMaxTilesRefinedPerFrame(targetCount);
          tryUpgradeTimestampMs = 0.0;
        } else {
          tryUpgradeTimestampMs = frameStartMs + UPGRADE_RETRY_DELAY_MS;
        }
      }
    } else {
      updateAdaptiveTileRefinement();
    }
  }

  return true;
}

bool PAGXView::firstFrameRendered() const {
  return hasRenderedFirstFrame;
}

int PAGXView::width() const {
  return _width;
}

int PAGXView::height() const {
  return _height;
}

void PAGXView::updatePerformanceState(double frameDurationMs) {
  double now = emscripten_get_now();

  if (frameDurationMs > SLOW_FRAME_THRESHOLD_MS) {
    if (!lastFrameSlow) {
      frameHistory.clear();
      frameHistoryTotalTime = 0.0;
    }
    lastFrameSlow = true;
  }

  frameHistory.push_back({now, frameDurationMs});
  frameHistoryTotalTime += frameDurationMs;

  double windowStart = now - RECOVERY_WINDOW_MS;
  while (!frameHistory.empty() && frameHistory.front().timestampMs < windowStart) {
    frameHistoryTotalTime -= frameHistory.front().durationMs;
    frameHistory.pop_front();
  }

  if (lastFrameSlow && !frameHistory.empty()) {
    double avgTime = frameHistoryTotalTime / static_cast<double>(frameHistory.size());
    size_t minFrames = isZooming ? MIN_RECOVERY_FRAMES_ZOOM_END : MIN_RECOVERY_FRAMES_STATIC;
    if (avgTime <= SLOW_FRAME_THRESHOLD_MS && frameHistory.size() >= minFrames) {
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
  int targetCount = calculateTargetTileRefinement(lastZoom);

  if (targetCount != currentMaxTilesRefinedPerFrame) {
    currentMaxTilesRefinedPerFrame = targetCount;
    displayList.setMaxTilesRefinedPerFrame(targetCount);
  }
}

}  // namespace pagx
